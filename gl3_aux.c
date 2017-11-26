#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <GL/glew.h>
#include "gl_aux.h"

static int slurp_file(const char *path, char *buf, size_t size)
{
    int fd = open(path, O_RDONLY);
    int ret = 0;
    while (size > 0) {
        int n = read(fd, buf, size);
        if (!n) return ret;
        if (n < 0) {
            fprintf(stderr, "%s: ", path);
            perror("read");
            return n;
        }
        ret += n;
        buf += n;
        size -= n;
    }
    return ret;
}

static GLuint compile_shader(GLenum type, const char *path)
{
    GLuint ret = 0;
    const size_t buf_size = 1<<16;
    char *buf = malloc(buf_size);
    if (!buf) {
        perror("malloc");
        goto quit;
    }

    GLint has_shaders;
    glGetIntegerv(GL_SHADER_COMPILER, &has_shaders);
    if (!has_shaders) {
        fprintf(stderr, "Shaders not supported\n");
        goto quit;
    }

    const int size = slurp_file(path, buf, buf_size);
    GLuint shader_id = glCreateShader(type);
    glShaderSource(shader_id, 1, (const char **)&buf, &size);
    glCompileShader(shader_id);

    GLint ok;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        fprintf(stderr, "%s: Compilation failed\n", path);
    }

    GLint log_length;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length) {
        char *log = malloc(log_length);
        if (!log) {
            perror("malloc");
            goto quit;
        }
        glGetShaderInfoLog(shader_id, log_length, NULL, log);
        fprintf(stderr, "%s: %s\n", path, log);
        free(log);
    }

    ret = shader_id;
quit:
    check_gl_error();
    if (buf) free(buf);
    return ret;
}

GLuint load_program(const char *vert_file, const char *frag_file)
{
    GLuint ret = 0;
    GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_file);
    if (!vert) {
        goto quit;
    }

    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_file);
    if (!frag) {
        goto quit;
    }

    GLuint program = glCreateProgram();
    ret = program;
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        fprintf(stderr, "Failed to link shader program\n");
    }

    GLint log_length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length) {
        char *log = malloc(log_length);
        if (!log) {
            perror("malloc");
            ret = 0;
        }
        glGetProgramInfoLog(program, log_length, NULL, log);
        fprintf(stderr, "%s, %s: %s\n", vert_file, frag_file, log);
        free(log);
        ret = 0;
    }

quit:
    if (vert) {
        glDeleteShader(vert);
    }
    if (frag) {
        glDeleteShader(frag);
    }
    if (vert && frag) {
        glDetachShader(program, frag);
        glDetachShader(program, vert);
    }
    check_gl_error();

    return ret;
}

GLint get_uniform_location(GLuint program, const char *name)
{
    GLint ret = glGetUniformLocation(program, name);
    check_gl_error();
    if (ret < 0) {
        fprintf(stderr, "%s: No such uniform\n", name);
    }
    return ret;
}

GLint get_attrib_location(GLuint program, const char *name)
{
    GLint ret = glGetAttribLocation(program, name);
    check_gl_error();
    if (ret < 0) {
        fprintf(stderr, "%s: No such attribute\n", name);
    }
    return ret;
}

