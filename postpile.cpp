#include <cstdio>
#include <map>
#include <set>
#include <algorithm>
#include <array>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

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
//#include "lmdebug.hpp"
//#include "depthmap.hpp"
#include "intersect.hpp"

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define CLAMP(x, lil, big) MAX(MIN(big, x), lil)
#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

#define VIEW_MAX_PITCH (M_PI/2.01)
#define VIEW_MIN_PITCH (M_PI/5.0)
#define VIEW_MAX_DISTANCE 100
#define VIEW_MIN_DISTANCE 25
#define ZOOM_FACTOR 1.05

using namespace std;
using namespace glm;

int draw_tile_count = 0;

mat4 proj_matrix;
mat4 view_matrix;
vec2 mouse;
TexAtlas hex_textures;
gl3_material cursor_mtl, green, brown;
//LmDebug lmdebug;
int debug_show_lightmap = 0;
int enable_shadows = 1;
//Depthmap depthmap;

struct Meshes {
    gl3_mesh post_mesh;
    gl3_mesh pine_mesh;
    gl3_mesh cursor_mesh;
    //gl3_mesh lmdebug_mesh;
};
std::vector<Triangle> post_triangles;
RenderPost render_post;

// scipy.signal.firwin(20, 0.01)
const vector<float> view_filter_coeffs {
    0.00764156,  0.01005217,  0.01700178,  0.02776751,  0.04120037,
    0.05585069,  0.07012762,  0.0824749 ,  0.09154324,  0.09634015,
    0.09634015,  0.09154324,  0.0824749 ,  0.07012762,  0.05585069,
    0.04120037,  0.02776751,  0.01700178,  0.01005217,  0.00764156
};

// scipy.signal.firwin(10, 0.01)
const vector<float> fast_filter_coeffs {
    0.01614987,  0.03792532,  0.09310069,  0.15590377,  0.19692034,
    0.19692034,  0.15590377,  0.09310069,  0.03792532,  0.01614987
};

#define HEX_EXTENT 50
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

double cliff(double distance);
char float_index(const string &coll, float x);
Time game_time(view_filter_coeffs);

void error_callback(int, const char* msg)
{
    fprintf(stderr, "GLFW Error: %s\n", msg);
}

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

// Not space efficient but who cares
#define CACHE_SIZE (2 * HEX_EXTENT + 4)
static std::array<std::array<float, CACHE_SIZE>, CACHE_SIZE> tile_cache;
#undef CACHE_SIZE
tile_generator tile_gen;

float cached_tile_value(const HexCoord<int> &coord)
{
    int q = coord.q - view.center.q + HEX_EXTENT + 1;
    int r = coord.r - view.center.r + HEX_EXTENT + 1;
    try {
    return tile_cache.at(q).at(r);
    } catch(...) { fprintf(stderr, "%d %d\n",q,r); throw;}
}

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

float hex_elevation(HexCoord<int> coord, const tile_generator *tile_gen=NULL)
{
    float v;
    if (tile_gen) {
        Point<double> center = hex_to_pixel(coord);
        v = tile_value(tile_gen, center.x, center.y);
    }
    else {
        v = cached_tile_value(coord);
    }
    return -2.5 + 5 * v;
}

char hex_tile(const string &coll, HexCoord<int> coord)
{
    float elevation = cached_tile_value(coord);
    return float_index(coll, elevation);
}


vector<glm::mat4> pines_on_tile(HexCoord<int> coord)
{
    float elevation = cached_tile_value(coord);
    if (elevation < 0.3 || elevation > 0.7) {
        return {};
    }

    auto mod = [](float f, int seed, int m) -> float {
        return ((int)(f * seed) % m) / (float)m;
    };

    int count = 6 * mod(elevation, 734877, 6);

    vector<glm::mat4> ret;
    for (int i = 0; i < count; i++) {
        float e = elevation * i;
        float angle = 2 * M_PI * mod(e, 34989237, 99391);
        float dist = 0.3 + 0.5 * mod(e, 8476397, 17821);
        float s = 0.1 + 0.7 * mod(e, 34249, 948);
        glm::mat4 scale = glm::scale(glm::vec3(s, s, s));
        glm::mat4 xlate = glm::translate(glm::vec3(
            dist * cos(angle), dist * sin(angle), 0));
        float dx = 0.1 * mod(e, 434981, 943);
        float dy = 0.1 * mod(e, 474981, 1543);
        float dz = 1 + 0.1 * mod(e, 348987, 9847);
        float da = 2 * M_PI * mod(e, 9091381, 883);

        glm::mat4 rot = glm::rotate(da, vec3(dx, dy, dz));

        ret.push_back(xlate * scale * rot);
    }
    return ret;
}

glm::mat4 step_view_matrix(const tile_generator &tile_gen)
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

    float view_elevation = hex_elevation(view.center, &tile_gen);
    float elevation_offset = filtered_elevation.next(view_elevation);

    glm::vec3 target_center(c.x, c.y, elevation_offset);
    vec3 center = center_filter.next(target_center);
    view.filtered_center = pixel_to_hex_double(center.x, center.y);
    vec3 eye = center + relative_eye;

    return glm::lookAt(eye, center, vec3(0, 0, 1));
}

glm::vec3 hex_position(HexCoord<int> coord)
{
    double distance = hex_distance(
        HexCoord<double>::from(coord),
        view.filtered_center);
    float elevation = hex_elevation(coord);
    float z = elevation - CLIFF_HEIGHT * cliff(distance);

    Point<double> center = hex_to_pixel(coord);
    return vec3(center.x, center.y, z);
}

char float_index(const string &coll, float x)
{
    auto size = coll.size();
    return coll.at(CLAMP(x * size, 0, size-1));
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
    // +1 makes the vertically sliding tiles on the margin visible
    return hex_range(HEX_EXTENT+1, view.center);
}

vector<HexCoord<int>> selectable_hexes()
{
    // No +1 hides the vertically sliding tiles on the margin
    return hex_range(HEX_EXTENT, view.center);
}

float min_distance_to_hex(
    const Ray &ray,
    const HexCoord<int> coord)
{
    vec3 position = hex_position(coord);
    glm::mat4 mv = view_matrix * glm::translate(glm::mat4(1), position);
    float ret = INFINITY;
    for (Triangle triangle : post_triangles) {
        for (int i = 0; i < 3; i++) {
            glm::vec4 v = mv * glm::vec4(triangle.vertices[i], 1);
            triangle.vertices[i] = glm::vec3(v);
        }

        ret = std::min(ret, ray_intersects_triangle(ray, triangle));
    }
    return ret;
}

// TODO measure the cost of this computation. It's a great candidate for
// threading.
static inline
HexCoord<int> hex_under_mouse_inner(glm::vec2 mouse)
{
    // All mouse calculations are in model-view space, without projection.
    glm::mat4 inv_projection = glm::inverse(proj_matrix);
    glm::vec4 o = inv_projection * glm::vec4{mouse.x, mouse.y, 0, 1};
    glm::vec4 d = inv_projection * glm::vec4{mouse.x, mouse.y, 1, 1};

    Ray ray = {
        .origin = glm::vec3(o / o.w),
        .direction = glm::vec3(d / d.w)
    };

    float min_distance = INFINITY;
    HexCoord<int> ret {INT_MAX, INT_MAX};

    for (const HexCoord<int>& coord : selectable_hexes()) {
        float distance = min_distance_to_hex(ray, coord);

        if (distance < min_distance) {
            min_distance = distance;
            ret = coord;
        }
    }

    return ret;
}

HexCoord<int> hex_under_mouse()
{
    int window_h = 0, window_w = 0;
    glfwGetWindowSize(window, &window_w, &window_h);
    glm::vec2 offset_mouse(2 * mouse.x / (float)window_w - 1,
                           2 * (window_h-mouse.y) / (float)window_h - 1);
    return hex_under_mouse_inner(offset_mouse);
}

float astro_bias()
{
    return LATITUDE * sin(game_time.fractional_year());
}

Light astro_light(float t, glm::vec3 base_color, int bias_sign)
{
    float angle = 2.0 * M_PI * t;
    float x = sin(angle);
    float z = cos(angle) + bias_sign * astro_bias();
    glm::vec3 direction(x, 0.17, z);

    float brightness = (z - HORIZON) / HORIZON;
    glm::vec3 color = CLAMP(brightness, 0, 1) * base_color;
    return Light { .direction = direction, .color = color };
}

double cliff(double distance)
{
    distance = std::abs(distance);
    if (distance <= HEX_EXTENT) return 0;
    if (distance >= HEX_EXTENT + 1) return 1;
    double ret = 1 - cos(0.5 * M_PI * (distance - HEX_EXTENT));
    if (ret < 0) {
        fprintf(stderr, "dist %g -> cliff %g\n", distance, ret);
    }
    assert(ret >= 0);
    assert(ret <= 1);
    return ret;
}

void draw()
{
    RenderPost::Drawlist hex_drawlist;
    hex_drawlist.view = view_matrix;
    hex_drawlist.projection = proj_matrix;

    auto sun = astro_light(game_time.fractional_day(), sun_color, 1);
    auto moon = astro_light(game_time.fractional_night(), moon_color, -1);

    // Index 0 is the shadow
    if (sun.direction.z > 0) {
        hex_drawlist.lights.put(sun);
        hex_drawlist.lights.put(moon);
    }
    else {
        hex_drawlist.lights.put(moon);
        hex_drawlist.lights.put(sun);
    }

    //Drawlist pine_drawlist = hex_drawlist;
    //pine_drawlist.mesh = &meshes.pine_mesh;

    draw_tile_count = 0;
    //HexCoord<int> cursor = hex_under_mouse();
    auto& side_items = hex_drawlist.grouped_items["side"];
    auto& top_items = hex_drawlist.grouped_items["hex_top"];

    std::array<glm::vec2, CHAR_MAX> top_offsets;
    std::array<glm::vec2, CHAR_MAX> side_offsets;
    for (const auto &pair : top_texfiles) {
        top_offsets[pair.first] = hex_textures.offset[pair.second];
    }
    for (const auto &pair : side_texfiles) {
        side_offsets[pair.first] = hex_textures.offset[pair.second];
    }

    for (const HexCoord<int>& coord : visible_hexes()) {
        RenderPost::Drawlist::Item top, side;
        vec3 position = hex_position(coord);

        char top_tile = hex_tile(top_tileset, coord);
        top.uv_offset = top_offsets[top_tile];
        top.position = position;

        char side_tile = hex_tile(side_tileset, coord);
        side.uv_offset = side_offsets[side_tile];
        side.position = position;

        double distance = hex_distance(
            HexCoord<double>::from(coord),
            view.filtered_center);
        top.visibility = 1 - cliff(distance);
        side.visibility = top.visibility;

        /*
        if (coord.equals(cursor)) {
            top.material = &cursor_mtl;
            side.material = &cursor_mtl;
        }
        */

        top_items.push_back(top);
        side_items.push_back(side);

        /*
        for (const glm::mat4 mat : pines_on_tile(coord)) {
            Drawlist::Item canopy;
            Drawlist::Item trunk;
            canopy.visibility = top.visibility;
            trunk.visibility = top.visibility;
            canopy.material = &green;

            canopy.model_matrix = mm * mat;
            trunk = canopy; trunk.material = &brown;
            trunk.group = "trunk";
            canopy.group = "canopy";

            //pine_drawlist.items.push_back(canopy);
            //pine_drawlist.items.push_back(trunk);
        }
        */

        draw_tile_count++;
    }

    /*
    if (enable_shadows) {
        depthmap.begin();
        // TODO get rid of this offset
        depthmap.render(hex_drawlist,
            hex_model_matrix(view.center) *
            glm::scale(glm::vec3(-1.0, -1.0, 1.0)));
        depthmap.render(pine_drawlist,
            hex_model_matrix(view.center) *
            glm::scale(glm::vec3(-1.0, -1.0, 1.0)));
        hex_drawlist.depth_map = depthmap.texture_target;
        //pine_drawlist.depth_map = depthmap.texture_target;

        hex_drawlist.shadow_view_projection = depthmap.view_projection;
        // This makes global self shadows on the trees. Doesn't look great...
        //pine_drawlist.shadow_view_projection = depthmap.view_projection;
    }
    */

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glDrawBuffer(GL_BACK);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    render_post.draw(hex_drawlist);
    //render_obj.draw(pine_drawlist);
}

/*
void lmdebug_draw(const gl3_mesh &mesh)
{
    if (!debug_show_lightmap) return;
    Drawlist drawlist;
    drawlist.mesh = &mesh;
    Drawlist::Item item;
    item.group = "plane";
    drawlist.items.push_back(item);
    lmdebug.draw(drawlist, depthmap.texture_target);
}
*/

void resize()
{
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    float fov = M_PI * 50.0 / 180.8;
    float aspect = w / (float)h;
    proj_matrix = perspective<float>(fov, aspect, 1, 1e3);
}

void freshen_tile_cache(const tile_generator &tile_gen)
{
    for (int dq = -HEX_EXTENT-1; dq <= HEX_EXTENT+1; dq++) {
        for (int dr = -HEX_EXTENT-1; dr <= HEX_EXTENT+1; dr++) {
            HexCoord<int> coord;
            coord.q = view.center.q + dq;
            coord.r = view.center.r + dr;
            int q = dq + HEX_EXTENT + 1;
            int r = dr + HEX_EXTENT + 1;
            Point<double> center = hex_to_pixel(coord);
            tile_cache.at(q).at(r) = tile_value(&tile_gen, center.x, center.y);
        }
    }
}

void move(int n)
{
    view.center = hex_add(view.center, adjacent_hex(view.yaw + n));
    game_time.advance_hour();
    freshen_tile_cache(tile_gen);
}

void zoom_in() {
    view.distance.multiply(1 / ZOOM_FACTOR);
}

void zoom_out() {
    view.distance.multiply(ZOOM_FACTOR);

}
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

void scroll_callback(GLFWwindow *, double, double yoffset)
{
    if (std::abs(yoffset) > 1e-6) {
        double factor = 1 + 0.1 * std::abs(yoffset) * ZOOM_FACTOR;
        if (yoffset < 0) {
            factor = 1.0 / factor;
        }
        view.distance.multiply(factor);
    }
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

            case GLFW_KEY_SPACE: game_time.advance_hour(); break;

            case GLFW_KEY_R: view.yaw++; break;

            case GLFW_KEY_F1: debug_show_lightmap ^= 1; break;
            //case GLFW_KEY_F2: depthmap.shrink_texture(); break;
            //case GLFW_KEY_F3: depthmap.grow_texture(); break;
            case GLFW_KEY_F4: enable_shadows ^= 1; break;
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

void tick(const tile_generator &tile_gen)
{
    view.pitch.step();
    view.distance.step();
    game_time.step();
    view_matrix = step_view_matrix(tile_gen);
}

int main()
{
    assert(glfwInit());
    glfwSetErrorCallback(error_callback);

    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    assert(window = make_window());

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetFramebufferSizeCallback(window, window_size_callback);

    check_gl_error();

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

    //lmdebug.init("lmdebug.vert", "lmdebug.frag");
    check_gl_error();

    //depthmap.init("depthmap.vert", "depthmap.frag");
    check_gl_error();

    Meshes meshes;
    meshes.post_mesh.init(wf_mesh_from_file("post.obj"));
    // These are used for mouse cursor selection
    post_triangles = wf_triangles_from_file("post.obj");

    meshes.pine_mesh.init(wf_mesh_from_file("pine.obj"));

    hex_textures.init("hex_atlas.png", "hex_atlas.png.almanac");
    cursor_mtl = gl3_material::solid_color({1, 0, 0});
    green = gl3_material::solid_color({.1, .7, .2});
    brown = gl3_material::solid_color({.6, .3, .2});

    RenderPost::Setup post_setup;
    post_setup.mesh = &meshes.post_mesh;
    post_setup.material = &hex_textures.material;
    post_setup.uv_scale = hex_textures.scale;
    render_post.init(&post_setup);

    meshes.cursor_mesh.init(wf_mesh_from_file("cursor.obj"));
    //meshes.lmdebug_mesh.init(wf_mesh_from_file("lmdebug.obj"));
    float harmonics[] = { 7, 2, 1, 2, 3, 1 };
    tile_gen.harmonics = harmonics;
    tile_gen.num_harmonics = ARRAY_COUNT(harmonics);
    tile_gen.feature_size = 40;
    tiles_init(&tile_gen, 123);

    freshen_tile_cache(tile_gen);

    struct timeval starttime, frametime;
    float avg_frametime = 16;
    float avg_tiles_count = 500;
    int i=0;

    glfwSwapBuffers(window);

    while (!glfwWindowShouldClose(window)) {
        if (gettimeofday(&starttime, NULL) < 0) {
            perror("gettimeofday");
        }
        glfwPollEvents();
        handle_static_keys();
        tick(tile_gen);
        draw();
        //lmdebug_draw(meshes.lmdebug_mesh);

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
