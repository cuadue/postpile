#include <cstdio>
#include <map>
#include <set>

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
#include "hex.h"
#include "tiles.h"
}

#include "wavefront.hpp"
#include "gl2.hpp"
#include "fir_filter.hpp"

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define CLAMP(x, lil, big) MAX(MIN(big, x), lil)
#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

#define VIEW_MAX_PITCH (M_PI/2.01)
#define VIEW_MIN_PITCH (M_PI/5.0)
#define VIEW_MAX_DISTANCE 80
#define VIEW_MIN_DISTANCE 10

using namespace std;
using namespace glm;

int draw_tile_count = 0;

mat4 proj_matrix;
vec2 mouse;
wf_mesh post_mesh;
map<char, gl2_material> top_materials;
map<char, gl2_material> side_materials;
gl2_material cursor_mtl;
wf_mesh cursor_mesh;

const vector<float> view_filter_coeffs {0.2, 0.3, 0.3, 0.2};
const vector<float> slow_view {1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 3, 4, 5, 6};

int bail = 0;
SDL_Window *window;
struct {
    int yaw; // clock: 0->noon, 1->2o'clock, 2->4o'clock...
    filtered_value<float> pitch, distance;
    fir_filter<float> radius;
    struct hex_coord center;
} view = {
    .yaw = 0,
    .pitch = filtered_value<float>(view_filter_coeffs,
        VIEW_MAX_PITCH, VIEW_MIN_PITCH),
    .distance = filtered_value<float>(view_filter_coeffs,
        VIEW_MAX_DISTANCE, VIEW_MIN_DISTANCE),
    .radius = fir_filter<float>(slow_view, 1),
    .center = {0, 0},
};

fir_filter<vec3> center_filter(view_filter_coeffs, vec3(0, 0, 0));
fir_filter<float> eye_angle_filter(view_filter_coeffs, M_PI/2.0);

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

map<char, gl2_material> load_tex_mtls(const map<char, string> &texfiles)
{
    int olddir = pushd("tex");

    map<char, gl2_material> ret;
    for (const auto &p : texfiles) {
        wf_material w;
        w.diffuse.color = vec4(1, 1, 1, 1);
        w.diffuse.texture_file = p.second;
        ret[p.first] = gl2_material(w, IMG_Load);
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
    struct point c = hex_to_pixel(view.center);
    float distance = view.distance.get();
    float pitch = view.pitch.get();
    vec3 relative_eye(-distance * cos(angle) * cos(pitch),
                      -distance * sin(angle) * cos(pitch),
                      distance * sin(pitch));

    glm::vec3 target_center(c.x, c.y, 0);
    vec3 center = center_filter.next(target_center);
    vec3 eye = center + relative_eye;

    return glm::lookAt(eye, center, vec3(0, 0, 1));
}

inline
mat4 hex_model_matrix(int q, int r, float z)
{
    struct hex_coord coord = { .q = q, .r = r };
    struct point center = hex_to_pixel(coord);
    return glm::translate(mat4(1), vec3(center.x, center.y, z));
}

char float_index(const string &coll, float x)
{
    auto size = coll.size();
    return coll.at(CLAMP(x * size, 0, size-1));
}

static inline
struct hex_coord hex_under_mouse(glm::mat4 mvp, glm::vec2 mouse)
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

    struct hex_coord ret = pixel_to_hex(x, y);
    return ret;
}

vec2 to_vec2(struct point p)
{
    return vec2(p.x, p.y);
}

/*
static inline
int cube_distance(const ivec3 &a, const ivec3 &b)
{
    const ivec3 d = abs(a - b);
    return (d.x + d.y + d.z) / 2;
}

static inline
ivec3 hex_to_cube(const ivec2 &v)
{
    return ivec3(v.x, v.y, -v.x - v.y);
}

static inline
int hex_distance(const ivec2 &a, const ivec2 &b)
{
    return cube_distance(hex_to_cube(a), hex_to_cube(b));
}
*/

vector<hex_coord> hex_range(int n, const hex_coord &center)
{
    vector<hex_coord> ret;
    for (int q = -n; q <= n; q++) {
        int start = std::max(-n, -n-q);
        int stop = std::min(n, -q+n);
        for (int r = start; r <= stop; r++) {
            ret.push_back({center.q + q, center.r + r});
        }
    }
    return ret;
}

vector<hex_coord> visible_hexes()
{
    return hex_range(16, view.center);
}

void light()
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glShadeModel(GL_SMOOTH);
    float position[] = {0, 10, 10, 0};
    float ambient_color[] = {0.25, 0.25, 0.25, 1};
    float diffuse_color[] = {0.75, 0.75, 0.75, 1};
    float material[] = {1, 1, 1, 1};
    float specular_color[] = {0, 0, 0, 0};
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse_color);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient_color);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular_color);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, material);
}

void draw_mouse_cursor(const tile_generator &tile_gen)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    mat4 vm = view_matrix();
    mat4 view_proj = proj_matrix * vm;
    int window_h = 0, window_w = 0;
    SDL_GetWindowSize(window, &window_w, &window_h);
    glm::vec2 offset_mouse(2 * mouse.x / (float)window_w - 1,
                           2 * (window_h-mouse.y) / (float)window_h - 1);
    struct hex_coord mouse_hex = hex_under_mouse(view_proj, offset_mouse);
    struct point center = hex_to_pixel(mouse_hex);
    float elevation = tile_value(&tile_gen, center.x, center.y);

    Drawlist drawlist;
    drawlist.mesh = &cursor_mesh;
    drawlist.view_projection_matrix = view_proj;
    Drawlist::Model dlm;
    dlm.model_matrix = hex_model_matrix(mouse_hex.q, mouse_hex.r, elevation-0.4);
    dlm.material = &cursor_mtl;

    drawlist.groups["cursor"].push_back(dlm);
    gl2_draw_drawlist(drawlist);
}

void draw_drawlist(const Drawlist& drawlist);

void draw(const tile_generator &tile_gen)
{
    light();

    Drawlist drawlist;
    drawlist.mesh = &post_mesh;
    drawlist.view_projection_matrix = proj_matrix * view_matrix();

    // For benchmarking
    draw_tile_count = 0;

    for (const hex_coord coord : visible_hexes()) {
            Drawlist::Model top;
            Drawlist::Model side;
            struct point center = hex_to_pixel(coord);

            float elevation = tile_value(&tile_gen, center.x, center.y);
            mat4 mm = hex_model_matrix(coord.q, coord.r, elevation-0.5);
            char top_tile = float_index(top_tileset, elevation);
            char side_tile = float_index(side_tileset, elevation);

            top.model_matrix = mm;
            side.model_matrix = mm;
            top.material = &top_materials.at(top_tile);
            side.material = &side_materials.at(side_tile);

            drawlist.groups["hex_top"].push_back(top);
            drawlist.groups["side"].push_back(side);
            draw_tile_count++;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_mouse_cursor(tile_gen);

    gl2_draw_drawlist(drawlist);
}


void resize()
{
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    glViewport(0, 0, w, h);
    float fov = M_PI * 50.0 / 180.8;
    float aspect = w / (float)h;
    proj_matrix = perspective<float>(fov, aspect, 1, 1e2);
    check_gl_error();
}

void move(int n)
{
    view.center = hex_add(view.center, adjacent_hex(view.yaw + n));
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
    libs_assert(!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2));
    libs_assert(!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1));

    //set_msaa(1, 4);
    if (!(window = make_window())) {
        //set_msaa(0, 0);
        libs_assert((window = make_window()));
    }

    libs_assert(SDL_GL_CreateContext(window));
    bool swap_interval = true;
    if (SDL_GL_SetSwapInterval(-1) < 0) {
        if (SDL_GL_SetSwapInterval(1) < 0) {
            fprintf(stderr, "Using sleep, expect poor performance\n");
            swap_interval = false;
        }
    }

    libs_assert(IMG_Init(IMG_INIT_PNG) == IMG_INIT_PNG);
    fprintf(stderr, "GL Vendor: %s\n", glGetString(GL_VENDOR));
    fprintf(stderr, "GL Renderer: %s\n", glGetString(GL_RENDERER));
    fprintf(stderr, "GL Version: %s\n", glGetString(GL_VERSION));
    fprintf(stderr, "GLSL: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    glewInit();
    resize();

    post_mesh = wf_mesh_from_file("post.obj");
    top_materials = load_tex_mtls(top_texfiles);
    side_materials = load_tex_mtls(side_texfiles);

    cursor_mtl.set_diffuse({1, 0, 0, 0.5});
    cursor_mesh = wf_mesh_from_file("cursor.obj");

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
