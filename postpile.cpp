#include <cstdio>
#include <map>
#include <set>
#include <algorithm>

#include <GL/glew.h>
#include <SDL.h>
#include <SDL_video.h>
#include <SDL_opengl.h>
#include <SDL_error.h>
#include <SDL_image.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include "osn.h"
#include "tiles.h"
#include "gl_aux.h"
}

#include "wavefront.hpp"
#include "gl3.hpp"
#include "fir_filter.hpp"
#include "hex.hpp"

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define CLAMP(x, lil, big) MAX(MIN(big, x), lil)
#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

#define VIEW_MAX_PITCH (M_PI/2.01)
#define VIEW_MIN_PITCH (M_PI/5.0)
#define VIEW_MAX_DISTANCE 50
#define VIEW_MIN_DISTANCE 10

using namespace std;
using namespace glm;

int draw_tile_count = 0;

mat4 proj_matrix;
vec2 mouse;
gl3_mesh post_mesh;
map<char, gl3_material> top_materials;
map<char, gl3_material> side_materials;
gl3_material cursor_mtl;
gl3_mesh cursor_mesh;
gl3_program program;

// scipy.signal.firwin(40, 0.01)
const vector<float> view_filter_coeffs {
    0.00358851,  0.00388049,  0.00470869,  0.00606107,  0.00791063,
    0.01021609,  0.01292289,  0.01596464,  0.01926486,  0.0227391 ,
    0.02629727,  0.02984614,  0.03329195,  0.03654306,  0.03951257,
    0.04212075,  0.04429733,  0.04598353,  0.04713372,  0.04771672,
    0.04771672,  0.04713372,  0.04598353,  0.04429733,  0.04212075,
    0.03951257,  0.03654306,  0.03329195,  0.02984614,  0.02629727,
    0.0227391 ,  0.01926486,  0.01596464,  0.01292289,  0.01021609,
    0.00791063,  0.00606107,  0.00470869,  0.00388049,  0.00358851
};

#define MAX_VIEW_DISTANCE 8
#define CLIFF_HEIGHT 2

int bail = 0;
SDL_Window *window;
struct {
    int yaw; // clock: 0->noon, 1->2o'clock, 2->4o'clock...
    filtered_value<float> pitch, distance;
    HexCoord<int> center;
    HexCoord<double> filtered_center;
} view = {
    .yaw = 0,
    .pitch = filtered_value<float>(view_filter_coeffs,
        VIEW_MAX_PITCH, VIEW_MIN_PITCH),
    .distance = filtered_value<float>(view_filter_coeffs,
        VIEW_MAX_DISTANCE, VIEW_MIN_DISTANCE),
    .center = {0, 0},
    .filtered_center = {0, 0},
};

#define NOON 48
#define MIDNIGHT (2*NOON)
#define HORIZON 0.2
int time_of_day = 0;
const glm::vec3 sun_color(1, 0.9, 0.9);
const glm::vec3 moon_color(0.3, 0.4, 0.5);

#define YEAR_LENGTH 73
#define LATITUDE 0.4
int day_of_year = 0;

fir_filter<vec3> center_filter(view_filter_coeffs, vec3(0, 0, 0));
fir_filter<float> eye_angle_filter(view_filter_coeffs, M_PI/2.0);
fir_filter<float> filtered_elevation(view_filter_coeffs, 0);

void libs_print_err() {
    fprintf(stderr, "SDL Error: %s\n", SDL_GetError());
    fprintf(stderr, "IMG Error: %s\n", IMG_GetError());
}

#define libs_assert(x) if (!(x)) \
    {fprintf(stderr, "%s:%d Assert failed: %s\n", __FILE__, __LINE__, #x); \
        libs_print_err(); exit(1); }

/*
static void set_msaa(int buf, int samp)
{
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, buf);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, samp);
}
*/

static SDL_Window *make_window()
{
    return SDL_CreateWindow("postpile",
                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
}

// low elevation -> high elevation
// The distribution tends to be gaussian, so fudge a slightly more uniform
// distribution by adding more instances of tiles to the edges
static const string top_tileset {"~~~~++....%%:!^^$$>>>"};
static const map<char, string> top_texfiles {
    {'~', "horiz-water.jpg"},
    {'+', "horiz-waterplants.jpg"},
    {'.', "horiz-sand.jpg"},
    //{'*', "soil.jpg"},
    {'%', "horiz-grass-rock.jpg"},
    {':', "horiz-bush.jpg"},
    {'!', "horiz-grass-fine.jpg"},
    {'^', "horiz-trees.jpg"},
    {'$', "horiz-grass-frozen.jpg"},
    {'>', "horiz-ice.jpg"}
};

// just kind of special case the water and waterplants out eventually
static const string side_tileset {"....::::__==--->>"};
static const map<char, string> side_texfiles {
    {'.', "vertical-sand-light1.jpg"},
    {':', "vertical-sand-dark.jpg"},
    {'=', "vertical-brick-darktop1.jpg"},
    {'_', "soil.jpg"},
    {'-', "vertical-brick-greentop1.jpg"},
    {'>', "horiz-ice.jpg"}
};

int pushd(const char *path)
{
    int ret = open(".", O_RDONLY);
    if (ret < 0) {
        perror(".");
        return ret;
    }

    if (chdir(path) < 0) {
        perror(path);
        return -1;
    }
    return ret;
}

int popd(int fd)
{
    if (fchdir(fd) < 0) {
        perror("popd");
        return -1;
    }
    return close(fd);
}

map<char, gl3_material> load_tex_mtls(const map<char, string> &texfiles)
{
    int olddir = pushd("tex");

    map<char, gl3_material> ret;
    for (const auto &p : texfiles) {
        wf_material w;
        w.diffuse.color = vec4(1, 1, 1, 1);
        w.diffuse.texture_file = p.second;
        ret[p.first] = gl3_material(w, IMG_Load);
    }

    popd(olddir);
    return ret;
}

glm::mat4 view_matrix()
{
    // yaw = 0 -> looking up the positive Y-axis
    // yaw is negative because... fudge factor
    float target_angle = -view.yaw * (M_PI / 3.0) + (M_PI / 2.0);
    float angle = eye_angle_filter.next(target_angle);
    Point<double> c = hex_to_pixel(view.center);
    float distance = view.distance.get();
    float pitch = view.pitch.get();
    vec3 relative_eye(-distance * cos(angle) * cos(pitch),
                      -distance * sin(angle) * cos(pitch),
                      distance * sin(pitch));

    glm::vec3 target_center(c.x, c.y, 0);
    vec3 center = center_filter.next(target_center);
    view.filtered_center = pixel_to_hex_double(center.x, center.y);
    vec3 eye = center + relative_eye;

    return glm::lookAt(eye, center, vec3(0, 0, 1));
}

inline
mat4 hex_model_matrix(int q, int r, float z)
{
    HexCoord<int> coord = { .q = q, .r = r };
    Point<double> center = hex_to_pixel(coord);
    return glm::translate(mat4(1), vec3(center.x, center.y, z));
}

char float_index(const string &coll, float x)
{
    auto size = coll.size();
    return coll.at(CLAMP(x * size, 0, size-1));
}

static inline
HexCoord<int> hex_under_mouse(glm::mat4 mvp, glm::vec2 mouse)
{
    // Define two points in screen space
    glm::vec4 screen_a(mouse.x, mouse.y, -1, 1);
    glm::vec4 screen_b(mouse.x, mouse.y, 1, 1);

    // Transform them into object space
    glm::vec4 a(glm::inverse(mvp) * screen_a);
    a /= a.w;
    glm::vec4 b(glm::inverse(mvp) * screen_b);
    b /= b.w;

    // Draw a line through the two points and intersect with the Z plane
    double t = a.z / (a.z - b.z);
    double x = a.x + t * (b.x - a.x);
    double y = a.y + t * (b.y - a.y);

    HexCoord<int> ret = pixel_to_hex_int(x, y);
    return ret;
}

vec2 to_vec2(Point<double> p)
{
    return vec2(p.x, p.y);
}

vector<HexCoord<int>> hex_range(int n, const HexCoord<int> &center)
{
    vector<HexCoord<int>> ret;
    for (int q = -n; q <= n; q++) {
        int start = std::max(-n, -n-q);
        int stop = std::min(n, -q+n);
        for (int r = start; r <= stop; r++) {
            ret.push_back({center.q + q, center.r + r});
        }
    }
    return ret;
}

vector<HexCoord<int>> visible_hexes()
{
    // The +1 makes the vertically sliding tiles on the margin visible
    return hex_range(MAX_VIEW_DISTANCE+1, view.center);
}

void draw_mouse_cursor(const tile_generator &tile_gen)
{
    check_gl_error();
    mat4 vm = view_matrix();
    mat4 view_proj = proj_matrix * vm;
    int window_h = 0, window_w = 0;
    SDL_GetWindowSize(window, &window_w, &window_h);
    glm::vec2 offset_mouse(2 * mouse.x / (float)window_w - 1,
                           2 * (window_h-mouse.y) / (float)window_h - 1);
    struct HexCoord<int> mouse_hex = hex_under_mouse(view_proj, offset_mouse);
    int distance = hex_distance(mouse_hex, view.center);
    if (distance > MAX_VIEW_DISTANCE) {
        return;
    }

    Point<double> center = hex_to_pixel(mouse_hex);
    float elevation = tile_value(&tile_gen, center.x, center.y);

    Drawlist drawlist;
    drawlist.mesh = &cursor_mesh;
    drawlist.view = view_matrix();
    drawlist.projection = proj_matrix;
    Drawlist::Model dlm;
    dlm.model_matrix = hex_model_matrix(mouse_hex.q, mouse_hex.r, elevation-0.4);
    dlm.material = &cursor_mtl;

    drawlist.groups["cursor"].push_back(dlm);
    gl3_draw(drawlist, program);
}

float astro_bias()
{
    float t = (float)day_of_year + (float)time_of_day / MIDNIGHT;
    return LATITUDE * sin(t / YEAR_LENGTH);
}

Light astro_light(float time, glm::vec3 base_color, int bias_sign)
{
    float angle = 2.0 * M_PI * time / (float)MIDNIGHT;
    float x = sin(angle);
    float y = cos(angle) + bias_sign * astro_bias();
    glm::vec3 direction(x, 0, y);

    float brightness = (y - HORIZON) / HORIZON;
    glm::vec3 color = CLAMP(brightness, 0, 1) * base_color;
    return Light { .direction = direction, .color = color };
}

double cliff(double distance)
{
    distance = std::abs(distance);
    if (distance <= MAX_VIEW_DISTANCE) return 0;
    if (distance >= MAX_VIEW_DISTANCE + 1) return 1;
    double ret = 1 - cos(0.5 * M_PI * (distance - MAX_VIEW_DISTANCE));
    if (ret < 0) {
        fprintf(stderr, "dist %g -> cliff %g\n", distance, ret);
    }
    assert(ret >= 0);
    assert(ret <= 1);
    return ret;
}

void draw(const tile_generator &tile_gen)
{
    // TODO gl3_light();

    Drawlist drawlist;
    drawlist.mesh = &post_mesh;
    drawlist.view = view_matrix();
    drawlist.projection = proj_matrix;

    drawlist.lights.put(astro_light(time_of_day, sun_color, 1));
    drawlist.lights.put(astro_light(NOON + time_of_day, moon_color, -1));

    // For benchmarking
    draw_tile_count = 0;

    Point<double> view_center = hex_to_pixel(view.center);
    float center_elevation = 5*tile_value(&tile_gen,
        view_center.x, view_center.y);

    float elevation_offset = filtered_elevation.next(center_elevation);

    for (const HexCoord<int>& coord : visible_hexes()) {
        Drawlist::Model top;
        Drawlist::Model side;
        Point<double> center = hex_to_pixel(coord);
        double distance = hex_distance(
            HexCoord<double>::from(coord),
            view.filtered_center);

        float elevation = tile_value(&tile_gen, center.x, center.y);

        mat4 mm = hex_model_matrix(coord.q, coord.r,
            elevation_offset + 5 * elevation - 0.5 - CLIFF_HEIGHT * cliff(distance));

        char top_tile = float_index(top_tileset, elevation);
        char side_tile = float_index(side_tileset, elevation);

        top.model_matrix = mm;
        side.model_matrix = mm;
        top.visibility = 1 - cliff(distance);
        side.visibility = 1 - cliff(distance);

        top.material = &top_materials.at(top_tile);
        side.material = &side_materials.at(side_tile);

        drawlist.groups["hex_top"].push_back(top);
        drawlist.groups["side"].push_back(side);
        draw_tile_count++;
    }

    glEnable(GL_DEPTH_TEST);
    check_gl_error();
    glDepthFunc(GL_LESS);
    check_gl_error();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    check_gl_error();

    draw_mouse_cursor(tile_gen);

    gl3_draw(drawlist, program);
}


void resize()
{
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    glViewport(0, 0, w, h);
    float fov = M_PI * 50.0 / 180.8;
    float aspect = w / (float)h;
    proj_matrix = perspective<float>(fov, aspect, 1, 1e2);
}

void move(int n)
{
    view.center = hex_add(view.center, adjacent_hex(view.yaw + n));
    time_of_day++;
    if (time_of_day > MIDNIGHT) {
        day_of_year++;
        day_of_year %= YEAR_LENGTH;
    }
    time_of_day %= MIDNIGHT;
}

void zoom_in(float dir=1) { view.distance.add(0.5 * dir); }
void zoom_out() { zoom_in(-1); }
void pitch_up(float dir=1) { view.pitch.add(0.02 * dir); }
void pitch_down() { pitch_up(-1); }

void events()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            exit(EXIT_SUCCESS);
            break;

        case SDL_WINDOWEVENT:
            switch (e.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                resize();
                break;
            }
            break;

        case SDL_MOUSEMOTION:
            mouse.x = e.motion.x;
            mouse.y = e.motion.y;
            break;

        case SDL_KEYDOWN:
            switch (e.key.keysym.scancode) {
            case SDL_SCANCODE_W: move(0); break;
            case SDL_SCANCODE_E: move(1); break;
            case SDL_SCANCODE_D: move(2); break;
            case SDL_SCANCODE_S: move(3); break;
            case SDL_SCANCODE_A: move(4); break;
            case SDL_SCANCODE_Q: move(5); break;

            case SDL_SCANCODE_R: view.yaw++; break;
            default: break;
            }
            break;
        }
    }

    const uint8_t *keystate = SDL_GetKeyboardState(NULL);

    if (keystate[SDL_SCANCODE_F]) zoom_in();
    if (keystate[SDL_SCANCODE_V]) zoom_out();

    if (keystate[SDL_SCANCODE_T]) pitch_up();
    if (keystate[SDL_SCANCODE_G]) pitch_down();
}


int main()
{
    libs_assert(!SDL_Init(SDL_INIT_VIDEO));

    libs_assert(!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));
    libs_assert(!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));

    //set_msaa(1, 4);
    if (!(window = make_window())) {
        //set_msaa(0, 0);
        libs_assert((window = make_window()));
    }

    libs_assert(SDL_GL_CreateContext(window));
    check_gl_error();
    bool swap_interval = true;
    if (SDL_GL_SetSwapInterval(-1) < 0) {
        check_gl_error();
        if (SDL_GL_SetSwapInterval(1) < 0) {
            check_gl_error();
            fprintf(stderr, "Using sleep, expect poor performance\n");
            swap_interval = false;
        }
    }

    libs_assert(IMG_Init(IMG_INIT_PNG) == IMG_INIT_PNG);
    fprintf(stderr, "GL Vendor: %s\n", glGetString(GL_VENDOR));
    check_gl_error();
    fprintf(stderr, "GL Renderer: %s\n", glGetString(GL_RENDERER));
    check_gl_error();
    fprintf(stderr, "GL Version: %s\n", glGetString(GL_VERSION));
    check_gl_error();
    fprintf(stderr, "GLSL: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    check_gl_error();

    glewExperimental = 1;
    glewInit();
    check_gl_error();
    resize();
    program = gl3_load_program("program.vert", "program.frag");

    post_mesh = gl3_mesh(wf_mesh_from_file("post.obj"));
    assert(post_mesh.vao);
    top_materials = load_tex_mtls(top_texfiles);
    side_materials = load_tex_mtls(side_texfiles);

    cursor_mesh = gl3_mesh(wf_mesh_from_file("cursor.obj"));

    tile_generator tile_gen;
    float harmonics[] = { 7, 2, 1, 2, 3, 1 };
    tile_gen.harmonics = harmonics;
    tile_gen.num_harmonics = ARRAY_COUNT(harmonics);
    tile_gen.feature_size = 40;
    tiles_init(&tile_gen, 123);

    struct timeval starttime, frametime;
    float avg_fps = 30;
    float avg_tiles_count = 500;
    int i=0;

    while (!bail) {
        if (gettimeofday(&starttime, NULL) < 0) {
            perror("gettimeofday");
        }
        events();
        draw(tile_gen);

        SDL_GL_SwapWindow(window);
        if (!swap_interval) {
            SDL_Delay(15);
        }

        if (gettimeofday(&frametime, NULL) < 0) {
            perror("gettimeofday");
        }

        float dsec = frametime.tv_sec - starttime.tv_sec;
        suseconds_t dusec = frametime.tv_usec - starttime.tv_usec;

        float this_fps = 1/(dsec + dusec/1e6);
        avg_fps += (this_fps - avg_fps) * 0.1;
        avg_tiles_count += (draw_tile_count - avg_tiles_count) * 0.1;
        if (i++ % 600 == 0)
            printf("%g tiles -> %g fps\n", avg_tiles_count, avg_fps);
    }
}
