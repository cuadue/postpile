#include "render_post.hpp"

#define DIFFUSE_MAP_TEXTURE_INDEX 1
#define SHADOW_MAP_TEXTURE_INDEX 2

static GLuint program = INT_MAX;

void RenderPost::init(const RenderPost::Setup *setup)
{
    if (program == INT_MAX) {
        program = load_program("render_post.vert", "render_post.frag");
    }
    group = setup->group;
    group_count = setup->mesh->groups.at(group).count;
    uv_scale = setup->uv_scale;
    assert(program);

    vertex.init(program, "vertex", 4);
    normal.init(program, "normal", 3);
    uv.init(program, "vertex_uv", 2);
    position.init(program, "position", 3);
    position.instanced = true;
    visibility.init(program, "visibility", 1);
    visibility.instanced = true;
    uv_offset.init(program, "uv_offset", 2);
    uv_offset.instanced = true;

    view_matrix.init(program, "view_matrix");
    projection_matrix.init(program, "projection_matrix");
    //shadow_view_projection_matrix.init(program, "shadow_view_projection_matrix");
    //shadow_map.init(program, "shadow_map");

    diffuse_map.init(program, "diffuse_map");
    shader_uv_scale.init(program, "uv_scale");

    num_lights.init(program, "num_lights");
    light_vec.init(program, "light_vec");
    light_color.init(program, "light_color");

    position_buffer.init({}, true);
    visibility_buffer.init({}, true);
    uv_offset_buffer.init({}, true);

    vao.init();
    vao.bind();

    vertex.activate(setup->mesh->vertex_buffer);
    normal.activate(setup->mesh->normal_buffer);
    uv.activate(setup->mesh->uv_buffer);
    position.activate(position_buffer);
    visibility.activate(visibility_buffer);
    uv_offset.activate(uv_offset_buffer);
    setup->material->activate(DIFFUSE_MAP_TEXTURE_INDEX);

    GLuint index_buffer = setup->mesh->groups.at(group).index_buffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);

    vao.unbind();

    check_gl_error();
}

void RenderPost::prep()
{
    check_gl_error();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glUseProgram(program);
    check_gl_error();
}

void RenderPost::draw(const RenderPost::Drawlist &drawlist)
{
    vao.bind();

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
    std::vector<glm::vec2> uv_offsets;

    for (const auto &item : drawlist.items) {
        positions.push_back(item.position);
        visibilities.push_back(item.visibility);
        uv_offsets.push_back(item.uv_offset);
    }

    position_buffer.buffer_data_dynamic(positions);
    visibility_buffer.buffer_data_dynamic(visibilities);
    uv_offset_buffer.buffer_data_dynamic(uv_offsets);

    //shadow_view_projection_matrix.set(drawlist.shadow_view_projection);
    view_matrix.set(drawlist.view);
    projection_matrix.set(drawlist.projection);
    shader_uv_scale.set(uv_scale);

    glDrawElementsInstanced(GL_TRIANGLES, group_count, GL_UNSIGNED_INT, NULL,
                            drawlist.items.size());

    check_gl_error();
}
