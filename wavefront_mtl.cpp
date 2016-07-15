/* (C) 2016 Wes Waugh
 * GPLv3 License: respect Stallman because he is right.
 */
#include <cstring>
#include <cstdio>
#include <map>
#include "wavefront_mtl.hpp"
/* Much simpler than the .obj files */

using std::vector;
using std::string;
using std::map;
using glm::vec2;
using glm::vec3;
using glm::vec4;

struct wf_materialParse {
    wf_material mat;
    vector<wf_material> materials;
    int parse_newmtl(const char *line);
    int parse_color(const char *line);
    int parse_texture(const char *line);
    int parse_spec_exponent(const char *line);
};

static
const char *strip_left(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    return line;
}

static
void strip_right(char *line) {
    while (*line && *line != '\n') line++;
    *line = 0;
}

static
const char *next_token(const char *s) {
    while (*s && !isspace(*s)) s++;
    while (*s && isspace(*s)) s++;
    return s;
}

int parse_error(const char *line) {
    fprintf(stderr, "Syntax error: %s\n", line);
    return 1;
}

int wf_materialParse::parse_newmtl(const char *line)
{
    const char *key = "newmtl";
    if (!strncmp(line, key, strlen(key))) {
        if (mat.name.size()) {
            materials.push_back(mat);
            mat = wf_material();
        }
        mat.name = next_token(line);
        return 1;
    }
    return 0;
}

int wf_materialParse::parse_color(const char *line)
{
    float r, g, b, a;
    a = 1;
    if (3 <= sscanf(line, "Ka %f %f %f %f", &r, &g, &b, &a)) {
        mat.ambient.color = vec4(r, g, b, a);
    }
    else if (3 <= sscanf(line, "Kd %f %f %f %f", &r, &g, &b, &a)) {
        mat.diffuse.color = vec4(r, g, b, a);
    }
    else if (3 <= sscanf(line, "Ks %f %f %f %f", &r, &g, &b, &a)) {
        mat.specular.color = vec4(r, g, b, a);
    }
    else {
        return 0;
    }

    return 1;
}

int wf_materialParse::parse_texture(const char *line)
{
    if (!strncmp(line, "map_Ka", 6)) {
        mat.ambient.texture_file = next_token(line);
    }
    else if (!strncmp(line, "map_Kd", 6)) {
        mat.diffuse.texture_file = next_token(line);
    }
    else if (!strncmp(line, "map_Ks", 6)) {
        mat.specular.texture_file = next_token(line);
    }
    else {
        return 0;
    }
    return 1;
}

int wf_materialParse::parse_spec_exponent(const char *line)
{
    return 1 == sscanf(line, "Ns %g", &mat.specular_exponent);
}

map<string, wf_material> wf_mtllib_from_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror(path);
        return {};
    }

    wf_materialParse p;
    char buf[2048];
    while (fgets(buf, sizeof(buf), fp)) {
        strip_right(buf);
        const char *line = strip_left(buf);

        if (!*line || *line == '#' || *line == '\n') {
            continue;
        }

        assert(
            p.parse_newmtl(line) ||
            p.parse_color(line) ||
            p.parse_texture(line) ||
            p.parse_spec_exponent(line) ||
            parse_error(line)
        );
    }

    if (p.mat.name.size()) {
        p.materials.push_back(p.mat);
    }

    map<string, wf_material> ret;
    for (const wf_material &mat : p.materials) {
        ret[mat.name] = mat;
    }
    return ret;
}

void wf_mtl_component::dump() const
{
    printf("Color: %g, %g, %g, %g\n", color.x, color.y, color.z, color.w);
    printf("Texture file: %s\n", texture_file.c_str());
}

void wf_material::dump() const
{
    printf("wf_material \"%s\"\n", name.c_str());
    printf("Specular exponent %g\n", specular_exponent);

    printf("Ambient:");
    ambient.dump();
    printf("diffuse:");
    diffuse.dump();
    printf("specular:");
    specular.dump();
}

#ifdef TEST
int main(int argc, char **argv)
{
    (void)argc;
    if (argv[1]) {
        for (auto m : from_file(argv[1])) {
            m.dump();
        }
        exit(EXIT_SUCCESS);
    }
    else {
        fprintf(stderr, "Usage: %s mtl\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}
#endif
