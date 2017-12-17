#include "render_post.hpp"

RenderPost::RenderPost() {}

void RenderPost::init(const char *vert_file, const char *frag_file)
{
    program = load_program(vert_file, frag_file);
    assert(program);

    vertex.init(program, "vertex", 4);
    normal.init(program, "normal", 3);
    uv.init(program, "vertex_uv", 2);

    MVP.init(program, "MVP");
    N.init(program, "N");

    visibility.init(program, "visibility");
    diffuse_map.init(program, "diffuse_map");

    num_lights.init(program, "num_lights");
    light_vec.init(program, "light_vec");
    light_color.init(program, "light_color");
    check_gl_error();
}

void RenderPost::draw(const Drawlist &drawlist)
{
    glUseProgram(program);
    // Send this into the backbuffer
    glDrawBuffer(GL_BACK);

    drawlist.mesh->activate();
    vertex.activate(drawlist.mesh->vertex_buffer);
    normal.activate(drawlist.mesh->normal_buffer);
    uv.activate(drawlist.mesh->uv_buffer);

    glm::mat4 view_projection = drawlist.projection * drawlist.view;

    num_lights.set(drawlist.lights.direction.size());
    light_vec.set(drawlist.lights.direction);
    light_color.set(drawlist.lights.color);

    for (const auto &pair : drawlist.groups) {
        const std::string &group_name = pair.first;
        std::map<const gl3_material*, std::vector<const Drawlist::Model*>> grouped;

        for (const Drawlist::Model &model : pair.second) {
            grouped[model.material].push_back(&model);
        }

        if (!drawlist.mesh->groups.count(group_name)) {
            fprintf(stderr, "No mesh group %s\n", group_name.c_str());
            abort();
        }

        for (const auto &pair : grouped) {
            assert(pair.first);
            pair.first->setup(diffuse_map);
            for (const auto &model : pair.second) {
                MVP.set(view_projection * model->model_matrix);
                visibility.set(model->visibility);
                N.set(glm::mat3(1));
                drawlist.mesh->draw_group(group_name);
            }
        }
    }

    vertex.disable(drawlist.mesh->vertex_buffer);
    normal.disable(drawlist.mesh->normal_buffer);
    uv.disable(drawlist.mesh->uv_buffer);
    check_gl_error();
}
