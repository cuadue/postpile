#include <cstdio>
#include <map>

#include <GL/glew.h>
#include <SDL.h>
#include <SDL_video.h>
#include <SDL_opengl.h>
#include <SDL_error.h>
#include <SDL_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "wavefront.hpp"
#include "gl2.hpp"

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define CLAMP(x, lil, big) MAX(MIN(big, x), lil)
#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

using namespace std;
using namespace glm;

mat4 proj_matrix;
vec2 mouse;
wf_mesh post_mesh;
map<char, gl2_material> top_materials;
map<char, gl2_material> side_materials;

extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include "osn.h"
#include "hex.h"
#include "tiles.h"
}

int bail = 0;
SDL_Window *window;
struct {
    float yaw, height, distance;
    struct point center;
} view = {
    .yaw = 0,
    .height = 5,
    .distance = 5,
    .center.x = 0,
    .center.y = 0,
};

void libs_print_err() {
    fprintf(stderr, "SDL Error: %s\n", SDL_GetError());
    fprintf(stderr, "IMG Error: %s\n", IMG_GetError());
}

#define libs_assert(x) if (!(x)) \
    {fprintf(stderr, "%s:%d Assert failed: %s\n", __FILE__, __LINE__, #x); \
        libs_print_err(); exit(1); }

static void set_msaa(int buf, int samp)
{
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, buf);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, samp);
}

static SDL_Window *make_window()
{
    return SDL_CreateWindow("postpile",
                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
}

// low elevation -> high elevation
// The distribution tends to be gaussian, so fudge a slightly more uniform
// distribution by adding more instances of tiles to the edges
static const string top_tileset {"~~~~++..**%:!^^$$>>>"};
static const map<char, string> top_texfiles {
    {'~', "horiz-water.jpg"},
    {'+', "horiz-waterplants.jpg"},
    {'.', "horiz-sand.jpg"},
    {'*', "horiz-rock.jpg"},
    {'%', "horiz-grass-rock.jpg"},
    {':', "horiz-bush.jpg"},
    {'!', "horiz-grass-fine.jpg"},
    {'^', "horiz-trees.jpg"},
    {'$', "horiz-grass-frozen.jpg"},
    {'>', "horiz-ice.jpg"}
};

// just kind of special case the water and waterplants out eventually
static const string side_tileset {"...===--&&^^^>>>>"};
static const map<char, string> side_texfiles {
    {'.', "vertical-sand-light1.jpg"},
    {':', "vertical-sand-dark.jpg"},
    {'=', "vertical-brick-darktop1.jpg"},
    {'-', "vertical-brick-greentop1.jpg"},
    {'&', "vertical-soil-greentop1.jpg"},
    {'^', "vertical-ivy1.jpg"},
    {'>', "vertical-rock.jpg"}
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
    int olddir = pushd("img");

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
    struct point c = hex_to_pixel(pixel_to_hex(view.center.x, view.center.y));
    glm::vec3 eye(
        c.x + view.distance * cos(view.yaw),
        c.y + view.distance * sin(view.yaw),
        view.height);

    glm::vec3 center(c.x, c.y, 0);

    return glm::lookAt(eye, center, vec3(0, 0, 1));
}

mat4 hex_model_matrix(int q, int r, float z)
{
    struct hex_coord coord = { .q = q, .r = r };
    struct point center = hex_to_pixel(coord);
    glm::mat4 model = glm::translate(mat4(1), vec3(center.x, center.y, z));
    return glm::scale(model, glm::vec3(0.9, 0.9, 0.9));
}

char float_index(const string &coll, float x)
{
    auto size = coll.size();
    return coll.at(CLAMP(x * size, 0, size-1));
}

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

void draw(const tile_generator &tile_gen)
{
    struct drawitem {
        float elevation;
        mat4 modelview_matrix;
    };

    vector<drawitem> drawlist;
    mat4 vm = view_matrix();

    int window_h = 0, window_w = 0;
    SDL_GetWindowSize(window, &window_w, &window_h);
    glm::vec2 offset_mouse(2 * mouse.x / (float)window_w - 1,
                           2 * (window_h-mouse.y) / (float)window_h - 1);
    mat4 view_proj = proj_matrix * vm;
    struct hex_coord mouse_hex = hex_under_mouse(view_proj, offset_mouse);

    for (int r = -3; r < 3; r++) {
        for (int q = -3; q < 3; q++) {
            struct hex_coord coord = {
                .q = q + mouse_hex.q,
                .r = r + mouse_hex.r
            };
            struct point center = hex_to_pixel(coord);

            drawitem item;
            item.elevation = tile_value(&tile_gen, center.x, center.y);
            mat4 mm = hex_model_matrix(coord.q, coord.r, item.elevation-0.5);
            item.modelview_matrix = vm * mm;

            drawlist.push_back(item);
        }
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);

    gl2_setup_mesh_data(post_mesh);
    for (const auto &item : drawlist) {
        glLoadMatrixf(value_ptr(item.modelview_matrix));
        char tile = float_index(top_tileset, item.elevation);
        gl2_setup_material(top_materials.at(tile));
        gl2_draw_mesh_group(post_mesh, "top");
    }

    for (const auto &item : drawlist) {
        glLoadMatrixf(value_ptr(item.modelview_matrix));
        char tile = float_index(side_tileset, item.elevation);
        gl2_setup_material(side_materials.at(tile));
        gl2_draw_mesh_group(post_mesh, "side");
    }

    gl2_teardown_mesh_data();
    gl2_teardown_material();
}

void resize()
{
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    float fov = M_PI * 50.0 / 180.8;
    float aspect = w / (float)h;
    proj_matrix = perspective<float>(fov, aspect, 1, 1e2);
    glLoadMatrixf(value_ptr(proj_matrix));
    check_gl_error();
}

void forward(float x=1.0)
{
    view.center.x += -0.1f * x * cos(view.yaw);
    view.center.y += -0.1f * x * sin(view.yaw);
}

void left(float x=1.0)
{
    view.center.x += -0.1f * x * cos(view.yaw + M_PI/2.0);
    view.center.y += -0.1f * x * sin(view.yaw + M_PI/2.0);
}

void zoom_in(float x=1.0)
{
    view.distance = CLAMP(view.distance + 0.1 * x, 1, 10);
}

void up(float x=1.0)
{
    view.height += 0.1*x;
}

void backward() { forward(-1); }
void right() { left(-1); }
void zoom_out() { zoom_in(-1); }
void down() { up(-1); }

void rotate(float dir)
{
    view.yaw += 0.05f * dir;
}


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
        }
    }

    const uint8_t *keystate = SDL_GetKeyboardState(NULL);

    if (keystate[SDL_SCANCODE_W]) forward();
    if (keystate[SDL_SCANCODE_S]) backward();
    if (keystate[SDL_SCANCODE_A]) left();
    if (keystate[SDL_SCANCODE_D]) right();

    if (keystate[SDL_SCANCODE_Q]) rotate(1);
    if (keystate[SDL_SCANCODE_E]) rotate(-1);

    if (keystate[SDL_SCANCODE_R]) zoom_in();
    if (keystate[SDL_SCANCODE_F]) zoom_out();

    if (keystate[SDL_SCANCODE_T]) up();
    if (keystate[SDL_SCANCODE_G]) down();
}


int main()
{
    libs_assert(!SDL_Init(SDL_INIT_VIDEO));
    libs_assert(!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2));
    libs_assert(!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1));

    set_msaa(1, 4);
    if (!(window = make_window())) {
        set_msaa(0, 0);
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

    tile_generator tile_gen;
    float harmonics[] = { 7, 2, 1, 2, 3, 1 };
    tile_gen.harmonics = harmonics;
    tile_gen.num_harmonics = ARRAY_COUNT(harmonics);
    tile_gen.feature_size = 40;
    tiles_init(&tile_gen, 123);

    while (!bail) {
        events();
        draw(tile_gen);

        SDL_GL_SwapWindow(window);
        if (!swap_interval) {
            SDL_Delay(15);
        }
    }
}
