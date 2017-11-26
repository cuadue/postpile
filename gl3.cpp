#include "gl3.hpp"
extern "C" {
#include "gl3_aux.h"
}
#include <glm/gtc/type_ptr.hpp>

using std::string;
using std::map;
using std::vector;
using glm::mat4;

GLuint global_program;

static void setup_material(const gl2_material &mat)
{
    (void)mat;
}

static void teardown_material(const gl2_material &mat)
{
    (void)mat;
}

gl3_program gl3_load_program(const char *vert_file, const char *frag_file)
{
    gl3_program ret;
    ret.program = load_program(vert_file, frag_file);
    ret.attribs.vertex = get_attrib_location(ret.program, "vertex_modelspace");
    ret.attribs.uniform_mvp = get_uniform_location(ret.program, "mvp");
    ret.attribs.has_normals = false;
    ret.attribs.has_uv = false;
    return ret;
}

gl3_group::gl3_group(const wf_group& wf)
{
    glGenBuffers(1, &index_buffer);
    count = wf.triangle_indices.size();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 count * sizeof(wf.triangle_indices[0]),
                 &wf.triangle_indices[0], GL_STATIC_DRAW);
    check_gl_error();
}

void gl3_group::draw() const
{
    // TODO Setup uniform model matrix
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glDrawArrays(GL_TRIANGLES, 0, count);
    check_gl_error();
}

gl3_mesh::gl3_mesh(const wf_mesh& wf)
{
    assert(sizeof wf.vertex4[0] == 4);
    assert(sizeof wf.normal3[0] == 4);
    assert(sizeof wf.texture2[0] == 4);

    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER,
                 4 * wf.vertex4.size(),
                 &wf.vertex4[0], GL_STATIC_DRAW);

    has_normals = wf.has_normals();
    if (has_normals) {
        glGenBuffers(1, &normal_buffer);
        glBufferData(GL_ARRAY_BUFFER,
                     4 * wf.normal3.size(),
                     &wf.normal3[0], GL_STATIC_DRAW);
    }

    has_uv = wf.has_texture_coords();
    if (has_uv) {
        glGenBuffers(1, &normal_buffer);
        glBufferData(GL_ARRAY_BUFFER,
                     4 * wf.texture2.size(),
                     &wf.texture2[0], GL_STATIC_DRAW);
    }

    for (const auto &pair : wf.groups) {
        for (const wf_group &g : pair.second) {
            groups[pair.first].push_back(gl3_group(g));
        }
    }
    check_gl_error();
}

void gl3_mesh::setup_mesh_data(const gl3_attributes &attribs) const
{
    glEnableVertexAttribArray(attribs.vertex);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glVertexAttribPointer(attribs.vertex, 4, GL_FLOAT, GL_FALSE, 0, NULL);

    if (has_normals && attribs.has_normals) {
        glEnableVertexAttribArray(attribs.normals);
        glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
        glVertexAttribPointer(attribs.normals, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    }

    if (has_uv && attribs.has_uv) {
        glEnableVertexAttribArray(attribs.uv);
        glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
        glVertexAttribPointer(attribs.uv, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    }
    check_gl_error();
}

void gl3_mesh::teardown_mesh_data(const gl3_attributes &attribs) const
{
    glDisableVertexAttribArray(attribs.vertex);
    if (has_normals && attribs.has_normals) {
        glDisableVertexAttribArray(attribs.normals);
    }
    if (has_uv && attribs.has_uv) {
        glDisableVertexAttribArray(attribs.uv);
    }
    check_gl_error();
}

void gl3_mesh::draw_group(
    const mat4 &matrix, const string &name, const gl3_attributes &attribs) const
{
    if (attribs.uniform_mvp >= 0) {
        glUniformMatrix4fv(attribs.uniform_mvp, 1, GL_FALSE,
                           glm::value_ptr(matrix));
        check_gl_error();
    }

    assert(groups.count(name));
    for (const gl3_group &group : groups.at(name)) {
        group.draw();
    }
}

void gl3_draw(const DrawlistGL3 &drawlist, const gl3_program &program)
{
    glUseProgram(program.program);
    drawlist.mesh->setup_mesh_data(program.attribs);

    for (const auto &pair : drawlist.groups) {
        const string &group_name = pair.first;
        map<const gl2_material*, vector<glm::mat4>> sorted;

        for (const DrawlistGL3::Model &model : pair.second) {
            sorted[model.material].push_back(model.model_matrix);
        }

        if (!drawlist.mesh->groups.count(group_name)) {
            fprintf(stderr, "No mesh group %s\n", group_name.c_str());
            abort();
        }

        for (const auto &pair : sorted) {
            assert(pair.first);
            setup_material(*pair.first);
            for (const glm::mat4 &model_matrix : pair.second) {
                mat4 mvp = drawlist.view_projection_matrix * model_matrix;
                drawlist.mesh->draw_group(mvp, group_name, program.attribs);
            }
            teardown_material(*pair.first);
        }
    }

    drawlist.mesh->teardown_mesh_data(program.attribs);
    check_gl_error();
}
