#include <string>
#include <map>
#include <vector>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include "wavefront.hpp"
#include "renderer.hpp"
#include "gl2.hpp"

// The arguments to glEnableVertexAttribArray and glVertexAttribPointer
struct gl3_attributes {
    GLuint vertex;
    bool has_normals;
    GLuint normals;
    bool has_uv;
    GLuint uv;
    GLint uniform_mvp;
};

struct gl3_program {
    GLuint program;
    gl3_attributes attribs;
};

struct gl3_group {
    GLuint index_buffer;
    size_t count;
    explicit gl3_group(const wf_group &wf);
    void draw() const;
};

struct gl3_mesh {
    GLuint vertex_buffer;
    bool has_normals;
    GLuint normal_buffer;
    bool has_uv;
    GLuint uv_buffer;

    std::map<std::string, std::vector<gl3_group>> groups;

    explicit gl3_mesh(const wf_mesh &wf);
    void draw_group(
        const glm::mat4 &matrix, const std::string &name, const gl3_attributes &attribs) const;
    void setup_mesh_data(const gl3_attributes &attribs) const;
    void teardown_mesh_data(const gl3_attributes &attribs) const;
};

gl3_program gl3_load_program(const char *vert_file, const char *frag_file);
void gl3_draw(const DrawlistGL3 &drawlist, const gl3_attributes &attribs);
