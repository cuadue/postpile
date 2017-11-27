#include "gl3.hpp"
extern "C" {
#include "gl3_aux.h"
#include "gl_aux.h"
}
#include <glm/gtc/type_ptr.hpp>

using std::string;
using std::map;
using std::vector;
using glm::mat4;

gl3_material::gl3_material() {}

static void setup_material(const gl3_material &mat, const gl3_attributes &attribs)
{
    (void)mat;

    if (attribs.has_uv && attribs.uv >= 0) {
        glUniform1i(attribs.sampler, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mat.texture);
    }
}

static void teardown_material(const gl3_material &mat, const gl3_attributes &attribs)
{
    (void)mat;
    (void)attribs;
}

gl3_program gl3_load_program(const char *vert_file, const char *frag_file)
{
    gl3_program ret;
    ret.program = load_program(vert_file, frag_file);
    assert(ret.program);
    ret.attribs.vertex = get_attrib_location(ret.program, "vertex_modelspace");
    ret.attribs.uniform_mvp = get_uniform_location(ret.program, "mvp");
    ret.attribs.sampler = get_uniform_location(ret.program, "sampler");
    ret.attribs.has_normals = false;
    ret.attribs.uv = get_attrib_location(ret.program, "vertex_uv");
    ret.attribs.has_uv = true;
    return ret;
}

gl3_group::gl3_group(const wf_group& wf)
{
    assert(sizeof wf.triangle_indices[0] == 4);
    glGenBuffers(1, &index_buffer);
    count = wf.triangle_indices.size();
    unsigned int max = 0;
    for (auto i : wf.triangle_indices){
        max = std::max(max, i);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 count * 4,
                 &wf.triangle_indices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    check_gl_error();
}

void gl3_group::draw() const
{
    assert(index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, NULL);
    check_gl_error();
}

gl3_mesh::gl3_mesh(const wf_mesh& wf)
{
    assert(sizeof wf.vertex4[0] == 4);
    assert(sizeof wf.normal3[0] == 4);
    assert(sizeof wf.texture2[0] == 4);

    glGenVertexArrays(1, &vao);
    check_gl_error();
    assert(vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vertex_buffer);
    assert(vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER,
                 4 * wf.vertex4.size(),
                 &wf.vertex4[0], GL_STATIC_DRAW);
    check_gl_error();

    has_normals = wf.has_normals();
    if (has_normals) {
        glGenBuffers(1, &normal_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
        glBufferData(GL_ARRAY_BUFFER,
                     4 * wf.normal3.size(),
                     &wf.normal3[0], GL_STATIC_DRAW);
    }

    has_uv = wf.has_texture_coords();
    if (has_uv) {
        glGenBuffers(1, &uv_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
        glBufferData(GL_ARRAY_BUFFER,
                     4 * wf.texture2.size(),
                     &wf.texture2[0], GL_STATIC_DRAW);
    }

    for (const auto &pair : wf.groups) {
        for (const wf_group &g : pair.second) {
            groups[pair.first].push_back(gl3_group(g));
        }
    }
    glBindVertexArray(0);
    check_gl_error();
}

void gl3_mesh::setup_mesh_data(const gl3_attributes &attribs) const
{
    glBindVertexArray(vao);
    glEnableVertexAttribArray(attribs.vertex);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glVertexAttribPointer(attribs.vertex, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    check_gl_error();

    if (has_normals && attribs.has_normals) {
        glEnableVertexAttribArray(attribs.normals);
        glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
        glVertexAttribPointer(attribs.normals, 3, GL_FLOAT, GL_FALSE, 0, NULL);
        check_gl_error();
    }

    if (has_uv && attribs.has_uv) {
        glEnableVertexAttribArray(attribs.uv);
        glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
        glVertexAttribPointer(attribs.uv, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        check_gl_error();
    }
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

void gl3_draw(const Drawlist &drawlist, const gl3_program &program)
{
    glUseProgram(program.program);
    drawlist.mesh->setup_mesh_data(program.attribs);

    for (const auto &pair : drawlist.groups) {
        const string &group_name = pair.first;
        map<const gl3_material*, vector<glm::mat4>> sorted;

        for (const Drawlist::Model &model : pair.second) {
            sorted[model.material].push_back(model.model_matrix);
        }

        if (!drawlist.mesh->groups.count(group_name)) {
            fprintf(stderr, "No mesh group %s\n", group_name.c_str());
            abort();
        }

        for (const auto &pair : sorted) {
            assert(pair.first);
            setup_material(*pair.first, program.attribs);
            for (const glm::mat4 &model_matrix : pair.second) {
                mat4 mvp = drawlist.view_projection_matrix * model_matrix;
                drawlist.mesh->draw_group(mvp, group_name, program.attribs);
            }
            teardown_material(*pair.first, program.attribs);
        }
    }

    drawlist.mesh->teardown_mesh_data(program.attribs);
    check_gl_error();
}

static GLuint load_texture_2d(SDL_Surface *surf)
{
    GLuint ret;
    glGenTextures(1, &ret);
    glBindTexture(GL_TEXTURE_2D, ret);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    check_gl_error();

    float aniso;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
    if (aniso > 0) {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
    }
    else {
        fprintf(stderr, "Anisotropic filtering not supported\n");
    }

    SDL_Surface *converted = SDL_ConvertSurfaceFormat(
            surf, SDL_PIXELFORMAT_RGBA8888, 0);
    surf = NULL;

    if (converted) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h,
                     0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, converted->pixels);
        check_gl_error();
        SDL_FreeSurface(converted);
        converted = NULL;
    }
    else {
        fprintf(stderr, "Failed to convert texture: %s\n", SDL_GetError());
        ret = 0;
    }
    glGenerateMipmap(GL_TEXTURE_2D);

    return ret;
}

gl3_material::gl3_material(
    const wf_material &wf,
    SDL_Surface *(*load_texture)(const char *path))
{
    assert(load_texture);

    for (int i = 0; i < 4; i++) {
        diffuse[i] = wf.diffuse.color[i];
        specular[i] = wf.specular.color[i];
    }

    shininess = wf.specular_exponent;

    string texture_file = wf.diffuse.texture_file;

    if (texture_file.size()) {
        SDL_Surface *surf;
        if ((surf = load_texture(texture_file.c_str()))) {
            texture = load_texture_2d(surf);
            SDL_FreeSurface(surf);
        }
        else {
            perror(texture_file.c_str());
            texture = 0;
        }
    }
}
