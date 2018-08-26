#include "render_post.hpp"

#define DIFFUSE_MAP_TEXTURE_INDEX 1
#define SHADOW_MAP_TEXTURE_INDEX 2

void RenderPost::init(const RenderPost::Setup *setup)
{
    program = load_program("render_post.vert", "render_post.frag");
    group = setup->group;
    group_count = setup->mesh->groups.at(group).count;
    assert(program);

    vertex.init(program, "vertex", 4);
    normal.init(program, "normal", 3);
    uv.init(program, "vertex_uv", 2);
    position.init(program, "position", 3);
    position.instanced = true;
    visibility.init(program, "visibility", 1);
    visibility.instanced = true;

    view_matrix.init(program, "view_matrix");
    projection_matrix.init(program, "projection_matrix");
    //shadow_view_projection_matrix.init(program, "shadow_view_projection_matrix");
    //shadow_map.init(program, "shadow_map");

    diffuse_map.init(program, "diffuse_map");

    num_lights.init(program, "num_lights");
    light_vec.init(program, "light_vec");
    light_color.init(program, "light_color");

    position_buffer.init({}, true);
    visibility_buffer.init({}, true);

    vao.init();
    vao.bind();

    vertex.activate(setup->mesh->vertex_buffer);
    normal.activate(setup->mesh->normal_buffer);
    uv.activate(setup->mesh->uv_buffer);
    position.activate(position_buffer);
    visibility.activate(visibility_buffer);
    setup->material->activate(DIFFUSE_MAP_TEXTURE_INDEX);

    GLuint index_buffer = setup->mesh->groups.at(group).index_buffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);

    vao.unbind();

    check_gl_error();
}

void RenderPost::draw(const RenderPost::Drawlist &drawlist)
{
    vao.bind();

    check_gl_error();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glUseProgram(program);
    check_gl_error();

    num_lights.set(drawlist.lights.direction.size());
    light_vec.set(drawlist.lights.direction);
    light_color.set(drawlist.lights.color);

    /*
    if (drawlist.depth_map != UINT_MAX) {
        shadow_map.set(
            gl3_material(drawlist.depth_map).activate(
                SHADOW_MAP_TEXTURE_INDEX));
    }
    */
    diffuse_map.set(DIFFUSE_MAP_TEXTURE_INDEX);

    std::vector<glm::vec3> positions;
    std::vector<float> visibilities;

    for (const auto &item : drawlist.items) {
        positions.push_back(item.position);
        visibilities.push_back(item.visibility);
    }

    position_buffer.buffer_data_dynamic(positions);
    visibility_buffer.buffer_data_dynamic(visibilities);

    //shadow_view_projection_matrix.set(drawlist.shadow_view_projection);
    view_matrix.set(drawlist.view);
    projection_matrix.set(drawlist.projection);

    glDrawElementsInstanced(GL_TRIANGLES, group_count, GL_UNSIGNED_INT, NULL,
                            drawlist.items.size());

    vao.unbind();

    check_gl_error();
}
