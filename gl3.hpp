#include <string>
#include <map>
#include <vector>

#include <SDL.h>
#include <GL/glew.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include "wavefront.hpp"

// The arguments to glEnableVertexAttribArray and glVertexAttribPointer
struct gl3_attributes {
    // Inputs
    GLint vertex;
    bool has_normals;
    GLint normals;
    bool has_uv;
    GLint uv;

    // Uniforms
    GLint mvp;
    GLint sampler;
    GLint model_matrix;
    GLint light_direction;
    GLint light_color;
    GLint num_lights;
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

struct gl3_material {
    gl3_material();
    gl3_material(
        const wf_material &,
        SDL_Surface *(*load_texture)(const char *));

    GLfloat shininess;
    GLfloat specular[4], diffuse[4];
    GLuint texture;
    void set_diffuse(const std::vector<GLfloat> &);
};

struct gl3_mesh {
    GLuint vertex_buffer;
    bool has_normals;
    GLuint normal_buffer;
    bool has_uv;
    GLuint uv_buffer;
    GLuint vao;

    std::map<std::string, std::vector<gl3_group>> groups;

    gl3_mesh() {}
    explicit gl3_mesh(const wf_mesh &wf);
    void draw_group(
        const glm::mat4 &model, const glm::mat4 &mvp, const std::string &name,
        const gl3_attributes &attribs) const;
    void setup_mesh_data(const gl3_attributes &attribs) const;
    void teardown_mesh_data(const gl3_attributes &attribs) const;
};

gl3_program gl3_load_program(const char *vert_file, const char *frag_file);

struct Lights {
    std::vector<glm::vec3> direction;
    std::vector<glm::vec3> color;
};

struct Drawlist {
    const gl3_mesh *mesh;
    glm::mat4 view_projection_matrix;

    struct Model {
        const gl3_material *material;
        glm::mat4 model_matrix;
    };
    struct Group {
        const char *group_name;
        std::vector<Model> models;
    };
    // Map group name to a bunch of models drawing that group
    std::map<std::string, std::vector<Model>> groups;
    Lights lights;
};

void gl3_draw(const Drawlist &drawlist, const gl3_program &program);
