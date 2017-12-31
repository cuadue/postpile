#include "render_post.hpp"

#define DIFFUSE_MAP_TEXTURE_INDEX 1
#define SHADOW_MAP_TEXTURE_INDEX 2

RenderPost::RenderPost() {}

void RenderPost::init(const char *vert_file, const char *frag_file)
{
    program = load_program(vert_file, frag_file);
    assert(program);

    vertex.init(program, "vertex", 4);
    normal.init(program, "normal", 3);
    uv.init(program, "vertex_uv", 2);

    MVP.init(program, "MVP");
    shadow_MVP.init(program, "shadow_MVP");
    shadow_map.init(program, "shadow_map");

    visibility.init(program, "visibility");
    diffuse_map.init(program, "diffuse_map");

    num_lights.init(program, "num_lights");
    light_vec.init(program, "light_vec");
    light_color.init(program, "light_color");
    check_gl_error();
}

void RenderPost::draw(const Drawlist &drawlist)
{
    CHECK_DRAWLIST(drawlist);
    check_gl_error();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glUseProgram(program);
    check_gl_error();

    drawlist.mesh->activate();
    vertex.activate(drawlist.mesh->vertex_buffer);
    normal.activate(drawlist.mesh->normal_buffer);
    uv.activate(drawlist.mesh->uv_buffer);

    glm::mat4 view_projection = drawlist.projection * drawlist.view;

    num_lights.set(drawlist.lights.direction.size());
    light_vec.set(drawlist.lights.direction);
    light_color.set(drawlist.lights.color);

    if (drawlist.depth_map != UINT_MAX) {
        shadow_map.set(
            gl3_material(drawlist.depth_map).activate(
                SHADOW_MAP_TEXTURE_INDEX));
    }
    diffuse_map.set(DIFFUSE_MAP_TEXTURE_INDEX);

    std::map<const gl3_material*, std::vector<const Drawlist::Item*>> sorted;

    // Sorting by material reduces the relatively substantial overhead of
    // changing the texture.
    for (const Drawlist::Item &item : drawlist.items) {
        sorted[item.material].push_back(&item);
    }

    for (const auto &pair : sorted) {
        pair.first->activate(DIFFUSE_MAP_TEXTURE_INDEX);

        for (const auto &item : pair.second) {
            glm::mat4 mm = item->model_matrix;
            shadow_MVP.set(drawlist.shadow_view_projection * mm);
            MVP.set(view_projection * mm);
            visibility.set(item->visibility);
            drawlist.mesh->draw_group(item->group);
            check_gl_error();
        }
    }

    vertex.disable(drawlist.mesh->vertex_buffer);
    normal.disable(drawlist.mesh->normal_buffer);
    uv.disable(drawlist.mesh->uv_buffer);
    check_gl_error();
}
