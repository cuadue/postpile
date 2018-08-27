#include "render_post.hpp"

#define DIFFUSE_MAP_TEXTURE_INDEX 1
#define SHADOW_MAP_TEXTURE_INDEX 2

void RenderPost::init(const RenderPost::Setup *setup)
{
    program = load_program("render_post.vert", "render_post.frag");
    uv_scale = setup->uv_scale;
    material = setup->material;
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

    for (const auto &pair : setup->mesh->groups) {
        PerGroupData d;
        d.vao.init();
        d.vao.bind();
        d.count = pair.second.count;

        vertex.point_to(setup->mesh->vertex_buffer);
        normal.point_to(setup->mesh->normal_buffer);
        uv.point_to(setup->mesh->uv_buffer);
        position.point_to(position_buffer);
        visibility.point_to(visibility_buffer);
        uv_offset.point_to(uv_offset_buffer);

        d.indices = &pair.second;

        d.vao.unbind();

        groups[pair.first] = d;
    }

    check_gl_error();
}

void RenderPost::draw(const RenderPost::Drawlist &drawlist)
{
    check_gl_error();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glUseProgram(program);
    material->activate(DIFFUSE_MAP_TEXTURE_INDEX);
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

    view_matrix.set(drawlist.view);
    projection_matrix.set(drawlist.projection);
    shader_uv_scale.set(uv_scale);

    std::vector<glm::vec3> positions;
    std::vector<float> visibilities;
    for (const auto &pair : drawlist.grouped_items) {
        for (const auto &item : pair.second) {
            positions.push_back(item.position);
            visibilities.push_back(item.visibility);
        }
        break;
    }

    position_buffer.buffer_data_dynamic(positions);
    visibility_buffer.buffer_data_dynamic(visibilities);

    for (const auto &pair : drawlist.grouped_items) {
        std::vector<glm::vec2> uv_offsets;

        // TODO buffer directly instead of copying
        for (const auto &item : pair.second) {
            uv_offsets.push_back(item.uv_offset);
        }

        uv_offset_buffer.buffer_data_dynamic(uv_offsets);

        //shadow_view_projection_matrix.set(drawlist.shadow_view_projection);

        const auto &render_group = groups[pair.first];
        render_group.vao.bind();
        render_group.indices->bind_elements();
        render_group.indices->draw_instanced(positions.size());
    }

    //VertexArrayObject::unbind();

    check_gl_error();
}
