#include "render_obj.hpp"

#define DIFFUSE_MAP_TEXTURE_INDEX 1
#define SHADOW_MAP_TEXTURE_INDEX 2

RenderObj::RenderObj() {}

void RenderObj::init(const char *vert_file, const char *frag_file)
{
    program = load_program(vert_file, frag_file);
    assert(program);

    vertex.init(program, "vertex", 4);
    normal.init(program, "normal", 3);
    uv.init(program, "vertex_uv", 2);
    model_matrix.init(program, "model_matrix");
    model_matrix.instanced = true;
    visibility.init(program, "visibility", 1);
    visibility.instanced = true;

    view_matrix.init(program, "view_matrix");
    projection_matrix.init(program, "projection_matrix");
    shadow_view_projection_matrix.init(program, "shadow_view_projection_matrix");
    shadow_map.init(program, "shadow_map");

    diffuse_map.init(program, "diffuse_map");

    num_lights.init(program, "num_lights");
    light_vec.init(program, "light_vec");
    light_color.init(program, "light_color");

    model_matrix_buffer.init({}, true);
    visibility_buffer.init({}, true);

    check_gl_error();
}

void RenderObj::draw(const Drawlist &drawlist)
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

    num_lights.set(drawlist.lights.direction.size());
    light_vec.set(drawlist.lights.direction);
    light_color.set(drawlist.lights.color);

    if (drawlist.depth_map != UINT_MAX) {
        shadow_map.set(
            gl3_material(drawlist.depth_map).activate(
                SHADOW_MAP_TEXTURE_INDEX));
    }
    diffuse_map.set(DIFFUSE_MAP_TEXTURE_INDEX);

    std::map<const gl3_material*,
             std::map<std::string,
                      std::vector<const Drawlist::Item*>>> instances;

    // Sorting by material reduces the relatively substantial overhead of
    // changing the texture.
    for (const Drawlist::Item &item : drawlist.items) {
        instances[item.material][item.group].push_back(&item);
    }

    for (const auto &material_pair : instances) {
        material_pair.first->activate(DIFFUSE_MAP_TEXTURE_INDEX);
        for (const auto &group_pair : material_pair.second) {
            std::vector<glm::mat4> model_matrices;
            std::vector<float> visibilities;
            for (const auto &item : group_pair.second) {
                model_matrices.push_back(item->model_matrix);
                visibilities.push_back(item->visibility);
            }

            model_matrix_buffer.buffer_data_dynamic(model_matrices);
            visibility_buffer.buffer_data_dynamic(visibilities);

            shadow_view_projection_matrix.set(drawlist.shadow_view_projection);
            view_matrix.set(drawlist.view);
            projection_matrix.set(drawlist.projection);
            model_matrix.activate(model_matrix_buffer);
            visibility.activate(visibility_buffer);
            drawlist.mesh->draw_group_instanced(
                group_pair.first, group_pair.second.size());
            check_gl_error();
        }
    }

    vertex.disable(drawlist.mesh->vertex_buffer);
    normal.disable(drawlist.mesh->normal_buffer);
    uv.disable(drawlist.mesh->uv_buffer);
    check_gl_error();
}
