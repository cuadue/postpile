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

wf_mesh post_mesh;
map<char, gl2_material> top_materials;
map<char, gl2_material> side_materials;

extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include "osn.h"
#include "hex.h"
#include "tiles.h"

int bail = 0;
SDL_Window *window;
struct {
    float yaw, height, distance;
    struct hex_coord center;
} view = {
    .yaw = 0,
    .height = 5,
    .distance = 5,
    .center.q = 0,
    .center.r = 0,
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
    struct point c = hex_to_pixel(view.center);
    glm::vec3 eye(
        c.x + view.distance * cos(view.yaw),
        c.y + view.distance * sin(view.yaw),
        view.height);

    glm::vec3 center(c.x, c.y, 0);

    return glm::lookAt(eye, center, vec3(0, 0, 1));
}

mat4 hex_model_matrix(int q, int r, int z)
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

void draw(const tile_generator &tile_gen)
{
    struct drawitem {
        float elevation;
        mat4 modelview_matrix;
    };

    vector<drawitem> drawlist;
    mat4 vm = view_matrix();

    struct hex_coord mouse_hex = {0, 0};

    for (int r = -30; r < 30; r++) {
        for (int q = -30; q < 30; q++) {
            if (r != mouse_hex.r || q != mouse_hex.q) {
                struct hex_coord coord = { .q = q, .r = r };
                struct point center = hex_to_pixel(coord);

                drawitem item;
                item.elevation = tile_value(&tile_gen, center.x, center.y);
                mat4 mm = hex_model_matrix(q, r, item.elevation);
                item.modelview_matrix = vm * mm;

                drawlist.push_back(item);
            }
        }
    }

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
    mat4 proj = perspective<float>(fov, aspect, 1, 1e2);
    glLoadMatrixf(value_ptr(proj));
    check_gl_error();
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
        }
    }
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
