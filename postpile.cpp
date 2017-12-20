#include <cstdio>
#include <map>
#include <set>
#include <algorithm>

#include <GL/glew.h>
#include <SDL.h>
#include <SDL_error.h>
#include <SDL_image.h>

#include <GLFW/glfw3.h>

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
#include "time.hpp"
#include "render_post.hpp"
#include "lmdebug.hpp"
#include "depthmap.hpp"

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define CLAMP(x, lil, big) MAX(MIN(big, x), lil)
#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

#define VIEW_MAX_PITCH (M_PI/2.01)
#define VIEW_MIN_PITCH (M_PI/5.0)
#define VIEW_MAX_DISTANCE 50
#define VIEW_MIN_DISTANCE 5

using namespace std;
using namespace glm;

int draw_tile_count = 0;

mat4 proj_matrix;
vec2 mouse;
map<char, gl3_material> top_materials;
map<char, gl3_material> side_materials;
gl3_material cursor_mtl;
LmDebug lmdebug;
int debug_show_lightmap = 0;
Depthmap depthmap;

struct Meshes {
    gl3_mesh post_mesh;
    gl3_mesh cursor_mesh;
    gl3_mesh lmdebug_mesh;
};
RenderPost render_post;

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

// scipy.signal.firwin(10, 0.01)
const vector<float> fast_filter_coeffs {
    0.01614987,  0.03792532,  0.09310069,  0.15590377,  0.19692034,
    0.19692034,  0.15590377,  0.09310069,  0.03792532,  0.01614987
};

#define MAX_VIEW_DISTANCE 8
#define CLIFF_HEIGHT 2

GLFWwindow *window;
struct {
    int yaw; // clock: 0->noon, 1->2o'clock, 2->4o'clock...
    filtered_value<float> pitch, distance;
    HexCoord<int> center;
    HexCoord<double> filtered_center;
} view = {
    .yaw = 0,
    .pitch = filtered_value<float>(fast_filter_coeffs,
        VIEW_MAX_PITCH, VIEW_MIN_PITCH),
    .distance = filtered_value<float>(fast_filter_coeffs,
        VIEW_MAX_DISTANCE, VIEW_MIN_DISTANCE),
    .center = {0, 0},
    .filtered_center = {0, 0},
};

#define HORIZON 0.2
const glm::vec3 sun_color(1, 0.9, 0.9);
const glm::vec3 moon_color(0.3, 0.4, 0.5);

#define LATITUDE 0.4

fir_filter<vec3> center_filter(view_filter_coeffs, vec3(0, 0, 0));
fir_filter<float> eye_angle_filter(view_filter_coeffs, M_PI/2.0);
fir_filter<float> filtered_elevation(view_filter_coeffs, 0);

Time game_time(view_filter_coeffs);

void libs_print_err() {
    fprintf(stderr, "SDL Error: %s\n", SDL_GetError());
    fprintf(stderr, "IMG Error: %s\n", IMG_GetError());
}

void error_callback(int, const char* msg)
{
    fprintf(stderr, "GLFW Error: %s\n", msg);
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

static GLFWwindow *make_window()
{
    return glfwCreateWindow(800, 600, "postpile", NULL, NULL);
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

// TODO This function is not idempotent, in particular it advances several
// filters each time it's called. Yikes.
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

void draw_mouse_cursor(const tile_generator &tile_gen, const Meshes &meshes)
{
    check_gl_error();
    mat4 vm = view_matrix();
    mat4 view_proj = proj_matrix * vm;
    int window_h = 0, window_w = 0;
    glfwGetWindowSize(window, &window_w, &window_h);
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
    drawlist.mesh = &meshes.cursor_mesh;
    drawlist.view = view_matrix();
    drawlist.projection = proj_matrix;
    Drawlist::Model dlm;
    dlm.model_matrix = hex_model_matrix(mouse_hex.q, mouse_hex.r, elevation-0.4);
    dlm.material = &cursor_mtl;

    drawlist.groups["cursor"].push_back(dlm);
    // TODO draw cursor
}

float astro_bias()
{
    return LATITUDE * sin(game_time.fractional_year());
}

Light astro_light(float t, glm::vec3 base_color, int bias_sign)
{
    float angle = 2.0 * M_PI * t;
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

void draw(const tile_generator &tile_gen, const Meshes &meshes)
{
    // TODO gl3_light();

    Drawlist drawlist;
    drawlist.mesh = &meshes.post_mesh;
    drawlist.view = view_matrix();
    drawlist.projection = proj_matrix;

    drawlist.lights.put(astro_light(game_time.fractional_day(), sun_color, 1));
    drawlist.lights.put(astro_light(game_time.fractional_night(), moon_color, -1));

    // For benchmarking
    draw_tile_count = 0;

    Point<double> view_center = hex_to_pixel(view.center);
    float center_elevation = -5 * tile_value(&tile_gen,
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
            5 * elevation - 0.5 - CLIFF_HEIGHT * cliff(distance) +
            elevation_offset);

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

    // TODO get rid of this offset
    depthmap.render(drawlist, hex_model_matrix(-view.center.q, -view.center.r, 0));

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glDrawBuffer(GL_BACK);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_mouse_cursor(tile_gen, meshes);
    render_post.draw(drawlist);
}

void lmdebug_draw(const gl3_mesh &mesh)
{
    if (!debug_show_lightmap) return;
    Drawlist drawlist;
    drawlist.mesh = &mesh;
    drawlist.groups["plane"].push_back(Drawlist::Model());
    lmdebug.draw(drawlist, depthmap.texture_target);
}

void resize()
{
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    float fov = M_PI * 50.0 / 180.8;
    float aspect = w / (float)h;
    proj_matrix = perspective<float>(fov, aspect, 1, 1e2);
}

void move(int n)
{
    view.center = hex_add(view.center, adjacent_hex(view.yaw + n));
    game_time.advance_hour();
}

void zoom_in(float dir=1) { view.distance.add(0.5 * dir); }
void zoom_out() { zoom_in(-1); }
void pitch_up(float dir=1) { view.pitch.add(0.02 * dir); }
void pitch_down() { pitch_up(-1); }

void window_size_callback(GLFWwindow *, int, int)
{
    resize();
}

void cursor_pos_callback(GLFWwindow *, double xpos, double ypos)
{
    mouse.x = xpos;
    mouse.y = ypos;
}

void key_callback(GLFWwindow *, int key, int , int action, int )
{
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) {
            case GLFW_KEY_W: move(0); break;
            case GLFW_KEY_E: move(1); break;
            case GLFW_KEY_D: move(2); break;
            case GLFW_KEY_S: move(3); break;
            case GLFW_KEY_A: move(4); break;
            case GLFW_KEY_Q: move(5); break;

            case GLFW_KEY_R: view.yaw++; break;

            case GLFW_KEY_F1: debug_show_lightmap ^= 1; break;
        }
    }
}

int key_is_pressed(int key)
{
    return glfwGetKey(window, key) == GLFW_PRESS;
}

void handle_static_keys()
{
    if (key_is_pressed(GLFW_KEY_F)) zoom_in();
    if (key_is_pressed(GLFW_KEY_V)) zoom_out();

    if (key_is_pressed(GLFW_KEY_T)) pitch_up();
    if (key_is_pressed(GLFW_KEY_G)) pitch_down();
}

void tick()
{
    view.pitch.step();
    view.distance.step();
    game_time.step();
}

int main()
{
    libs_assert(glfwInit());
    glfwSetErrorCallback(error_callback);

    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    //set_msaa(1, 4);
    if (!(window = make_window())) {
        //set_msaa(0, 0);
        libs_assert((window = make_window()));
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetFramebufferSizeCallback(window, window_size_callback);

    check_gl_error();

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

    render_post.init("render_post.vert", "render_post.frag");
    check_gl_error();

    lmdebug.init("lmdebug.vert", "lmdebug.frag");
    check_gl_error();

    depthmap.init("depthmap.vert", "depthmap.frag");
    check_gl_error();

    Meshes meshes;
    meshes.post_mesh = gl3_mesh(wf_mesh_from_file("post.obj"));
    assert(meshes.post_mesh.vao.location);
    top_materials = load_tex_mtls(top_texfiles);
    side_materials = load_tex_mtls(side_texfiles);

    meshes.cursor_mesh = gl3_mesh(wf_mesh_from_file("cursor.obj"));
    meshes.lmdebug_mesh = gl3_mesh(wf_mesh_from_file("lmdebug.obj"));
    tile_generator tile_gen;
    float harmonics[] = { 7, 2, 1, 2, 3, 1 };
    tile_gen.harmonics = harmonics;
    tile_gen.num_harmonics = ARRAY_COUNT(harmonics);
    tile_gen.feature_size = 40;
    tiles_init(&tile_gen, 123);

    struct timeval starttime, frametime;
    float avg_frametime = 16;
    float avg_tiles_count = 500;
    int i=0;

    while (!glfwWindowShouldClose(window)) {
        if (gettimeofday(&starttime, NULL) < 0) {
            perror("gettimeofday");
        }
        glfwPollEvents();
        handle_static_keys();
        tick();
        draw(tile_gen, meshes);
        lmdebug_draw(meshes.lmdebug_mesh);

        if (gettimeofday(&frametime, NULL) < 0) {
            perror("gettimeofday");
        }

        glfwSwapBuffers(window);

        float dsec = frametime.tv_sec - starttime.tv_sec;
        suseconds_t dusec = frametime.tv_usec - starttime.tv_usec;

        float this_frametime = 1e3*dsec + dusec/1e3;
        avg_frametime += (this_frametime - avg_frametime) * 0.1;
        avg_tiles_count += (draw_tile_count - avg_tiles_count) * 0.1;
        if (i++ % 600 == 0)
            printf("%g tiles -> %g ms\n", avg_tiles_count, avg_frametime);
    }
}
