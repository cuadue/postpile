/* * Vertex data are:
 *      vertices (v)
 *      texture vertices (vt)
 *      vertex normals (vn)
 *
 * Elements are:
 *      faces (f)
 *
 * Smoothing groups apply only to elements (in this file, faces).
 * usemtl statements also only apply to elements (faces).
 * These rules are Good Things (tm) because they mean I only have one complex
 * data structure for faces, instead of complex data structures for each of
 * vertices, normals, textures.
 */

#include <cstdio>
#include <cassert>
#include <cstring>
#include <cctype>

#include <string>
#include <set>
#include <map>
#include <tuple>

#include "wavefront.hpp"

using std::vector;
using std::set;
using std::map;
using glm::vec4;
using glm::vec3;
using glm::vec2;
using std::pair;
using std::make_pair;
using std::string;

// A face is defined like "f v/t/n v/t/n v/t/n .." and this struct is one
// of those tokens which comprises a face
typedef struct {
    int v, t, n;
} FaceToken;

// only used after the face has been triangulated. A better name might be
// TriangulatedFace or something similarly verbose.
typedef struct {
    enum IndexType { VERTEX=0, TEXTURE=1, NORMAL=2 };

    int vertex_indices[3];
    int texture_indices[3];
    int normal_indices[3];
    int smooth;
    int material;
    int group;
} Face;

class Parse {
public:
    vector<vec4> vertices;
    vector<vec3> normals;
    vector<vec2> textures;
    vector<Face> faces;
    vector<string> materials = {""};
    vector<string> mtllibs;
    map<int, vector<int>> vert_to_faces;
    vector<string> geometry_groups = {""};
    int smoothing_group = -1;
    int geometry_group = 0;
    int material_index = 0;
    int read_face_token(const char *, FaceToken *);
    int parse_face(const char *);
    int parse_group(const char *);
    int parse_vertex(const char *);
    int parse_normal(const char *);
    int parse_texture(const char *);
    int parse_error(const char *);
    int parse_smoothing_group(const char *);
    int parse_use_material(const char *);
    int parse_mtllib(const char *);
    int which_corner(const Face &face, int vertex_index) const;
    vec4 face_vector(const Face &face, int from, int to) const;
    float angle_at_corner(const Face &face, int vertex_index) const;

    vec3 face_normal(const Face &) const;
    vec3 smooth_normal(int smoothing_group, int vertex_index) const;
    void smooth_face_normals(Face &face);
    void smooth_normals();
};

int index_of(const string &s, const vector<string> &v)
{
    for (unsigned i = 0; i < v.size(); i++) if (v.at(i) == s) return i;
    return -1;
}

int Parse::read_face_token(const char *token, FaceToken *face) {
    face->v = 0;
    face->t = 0;
    face->n = 0;
    if (3 == sscanf(token, "%d/%d/%d", &face->v, &face->t, &face->n)) {
    }
    else if (2 == sscanf(token, "%d//%d", &face->v, &face->n)) {
    }
    else if (2 == sscanf(token, "%d/%d", &face->v, &face->t)) {
    }
    else if (1 == sscanf(token, "%d", &face->v)) {
    }
    else {
        fprintf(stderr, "Invalid face: '%s'\n", token);
        return 0;
    }
    // Negative numbers have to be handled in the middle of parsing because -1
    // refers to the most recently defined vertex/texture/normal, not the last
    // one in the file.
    if (face->v < 0) face->v += vertices.size();
    if (face->t < 0) face->t += textures.size();
    if (face->n < 0) face->n += normals.size();

    // Convert 1-indexed element indices to zero-indexed C indices
    face->v--;
    face->t--;
    face->n--;

    return 1;
}

static void face_set_tok(Face *face, int index, FaceToken tok)
{
    face->vertex_indices[index] = tok.v;
    face->texture_indices[index] = tok.t;
    face->normal_indices[index] = tok.n;
}

static
vector<Face> triangulate_face(vector<FaceToken> untri) {
    if (untri.size() < 3) return {};
    FaceToken pivot = untri[0];
    vector<Face> ret;
    for (unsigned i = 2; i < untri.size(); i++) {
        Face face;
        face_set_tok(&face, 0, pivot);
        face_set_tok(&face, 1, untri[i-1]);
        face_set_tok(&face, 2, untri[i]);
        ret.push_back(face);
    }
    return ret;
}

static
const char *next_token(const char *s) {
    while (*s && !isspace(*s)) s++;
    while (*s && isspace(*s)) s++;
    return s;
}

int Parse::parse_face(const char *token) {
    vector<FaceToken> facetoks;
    if (token[0] == 'f' && isspace(token[1])) {
        FaceToken facetok;
        while (*token) {
            token = next_token(token);
            if (*token) {
                if (read_face_token(token, &facetok)) {
                    facetoks.push_back(facetok);
                }
            }
        }

        for (auto face : triangulate_face(facetoks)) {
            face.smooth = smoothing_group;
            face.material = material_index;
            face.group = geometry_group;
            int face_index = faces.size();
            for (int i = 0; i < 3; i++) {
                vert_to_faces[face.vertex_indices[i]].push_back(face_index);
            }
            faces.push_back(face);
        }

        return 1;
    }
    else {
        return 0;
    }
}

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

int Parse::parse_group(const char *line) {
    if (line[0] == 'g') {
        const string group = strip_left(line+1);
        geometry_group = index_of(group, geometry_groups);
        if (geometry_group < 0) {
            geometry_group = geometry_groups.size();
            geometry_groups.push_back(group);
        }
        return 1;
    }
    else {
        return 0;
    }
}

int Parse::parse_vertex(const char *line) {
    float x, y, z, w;
    w = 1.0f;
    if (4 == sscanf(line, "v %f %f %f %f", &x, &y, &z, &w)) {
    }
    else if (3 == sscanf(line, "v %f %f %f", &x, &y, &z)) {
    }
    else {
        return 0;
    }

    vertices.push_back(vec4(x, y, z, w));
    return 1;
}

int Parse::parse_normal(const char *line) {
    float x, y, z;
    if (3 == sscanf(line, "vn %f %f %f", &x, &y, &z)) {
        normals.push_back(vec3(x, y, z));
        return 1;
    }
    else {
        return 0;
    }
}

int Parse::parse_texture(const char *line) {
    float s, t;
    if (2 == sscanf(line, "vt %f %f", &s, &t)) {
    }
    else {
        return 0;
    }
    textures.push_back(vec2(s, t));
    return 1;
}

int Parse::parse_error(const char *line) {
    fprintf(stderr, "Syntax error: %s\n", line);
    return 1;
}

int Parse::parse_smoothing_group(const char *line)
{
    if (1 == sscanf(line, "s %d", &smoothing_group)) {
    }
    else if (line[0] == 's'  && !strcmp(next_token(line), "off")) {
    }
    else {
        return 0;
    }
    return 1;
}

int Parse::parse_use_material(const char *line)
{
    const char *key = "usemtl";
    if (!strncmp(line, key, strlen(key))) {
        const char *mtl_name = next_token(line);
        material_index = index_of(mtl_name, materials);
        if (material_index < 0) {
            material_index = materials.size();
            materials.push_back(mtl_name);
        }
        return 1;
    }
    else {
        return 0;
    }
}

int Parse::parse_mtllib(const char *line)
{
    const char *key = "mtllib";
    if (!strncmp(line, key, strlen(key))) {
        const char *libname = next_token(line);

        if (index_of(libname, mtllibs) < 0) {
            mtllibs.push_back(libname);
        }
        return 1;
    }
    else {
        return 0;
    }
}

template<Face::IndexType index_type, typename T>
static
vector<T> deref_triangles(
    const vector<Face> &faces,
    const vector<T> &indices)
{
    vector<T> ret;
    for (const auto &f : faces) {
        for (int i = 0; i < 3; i++) {
            unsigned deref_index;
            switch (index_type) {
            case Face::VERTEX:  deref_index = f.vertex_indices[i];  break;
            case Face::TEXTURE: deref_index = f.texture_indices[i]; break;
            case Face::NORMAL:  deref_index = f.normal_indices[i];  break;
            }

            if (deref_index >= indices.size()) {
                fprintf(stderr, "Invalid face index %d %lu\n", deref_index,
                indices.size());
                return {};
            }

            ret.push_back(indices.at(deref_index));
        }
    }
    return ret;
}

template<typename T>
vector<float> to_floats(const vector<T> &inp)
{
    vector<float> ret;
    if (!inp.size()) return ret;
    glm::length_t len = inp[0].length();
    for (const auto &x : inp) {
        for (glm::length_t i = 0; i < len; i++) {
            ret.push_back(x[i]);
        }
    }
    return ret;
}

static vec3 hetero(const vec4 &v)
{
    return vec3(v.x, v.y, v.z) / v.w;
}

vec3 Parse::face_normal(const Face &f) const
{
    int ia = f.vertex_indices[0];
    int ib = f.vertex_indices[1];
    int ic = f.vertex_indices[2];

    const vec4 a = vertices.at(ia);
    const vec4 b = vertices.at(ib);
    const vec4 c = vertices.at(ic);

    const vec3 x = hetero(b) - hetero(a);
    const vec3 y = hetero(c) - hetero(a);

    vec3 ret = glm::cross(x, y);

    return ret;
}

float mag2(const vec3 v)
{
    return v.x * v.x + v.y + v.y + v.z + v.z;
}

/*
Handedness Parse::face_handedness(const Face &f)
{
    // If the (magnitude squared of the (face normal vector plus the specified
    // normal vector)) is greater than 2, then they are pointing in the "same
    // direction." Otherwise, the face normal should be computed in the
    // opposite handedness to match the other specified normals.
}
*/

int Parse::which_corner(const Face &face, int vertex_index) const {
    for (int i = 0; i < 3; i++) {
        if (face.vertex_indices[i] == vertex_index) {
            return i;
        }
    }
    return -1;
}

vec4 Parse::face_vector(const Face &face, int from, int to) const {
    assert(from >= 0 && from < 3);
    assert(to >= 0 && to < 3);
    assert(from != to);
    vec4 fv = vertices.at(face.vertex_indices[from]);
    vec4 tv = vertices.at(face.vertex_indices[to]);
    return tv - fv;
}

float Parse::angle_at_corner(const Face &face, int vertex_index) const {
    int origin = which_corner(face, vertex_index);
    assert(origin >= 0 && origin < 3);
    int corner_a, corner_b;
    switch (origin) {
    case 0: corner_a = 1; corner_b = 2; break;
    case 1: corner_a = 0; corner_b = 2; break;
    case 2: corner_a = 0; corner_b = 1; break;
    }

    vec4 a = face_vector(face, origin, corner_a);
    vec4 b = face_vector(face, origin, corner_b);

    return acos(glm::dot(a, b) / (glm::length(a) * glm::length(b)));
}

vec3 Parse::smooth_normal(int smoothing_group, int vertex_index) const {
    // Weight the normals in proportion to the angle of the face which
    // shares the vertex. This lets the cube have homogenous smoothed
    // normals regardless of how it's triangulated
    vec3 sum;
    const auto face_indices = vert_to_faces.at(vertex_index);
    assert(face_indices.size() > 0);

    for (int adjacent_index : face_indices) {
        Face adjacent = faces[adjacent_index];
        if (smoothing_group == adjacent.smooth) {
            float angle = fabs(angle_at_corner(adjacent, vertex_index));
            if (angle > 0) {
                sum += angle * glm::normalize(face_normal(adjacent));
            }
        }
    }
    return sum;
}

void Parse::smooth_face_normals(Face &face) {
    for (int corner = 0; corner < 3; corner++) {
        int normal_index = face.normal_indices[corner];
        if (normal_index < 0 || normal_index >= (int)normals.size()) {
            int vertex_index = face.vertex_indices[corner];
            vec3 normal;

            if (face.smooth > 0) {
                normal = smooth_normal(face.smooth, vertex_index);
            }
            else {
                normal = face_normal(face);
            }

            face.normal_indices[corner] = normals.size();
            normals.push_back(vec3(normal.x, normal.y, normal.z));
        }
    }
}

void Parse::smooth_normals()
{
    for (Face &face : faces) {
        smooth_face_normals(face);
    }

    for (vec3 &normal : normals) {
        normal = glm::normalize(normal);
    }
}

static void groupify(const Parse &parse, map<string, vector<wf_group>> &ret)
{
    map<pair<string, string>, wf_group> flat;

    // The wf_mesh::vertices array is in face order. The first triangle will
    // have indices 0,1,2
    // Each face belongs to exactly one group and has exactly one material.
    for (unsigned i = 0; i < parse.faces.size(); i++) {
        const Face &face = parse.faces.at(i);

        const string group_name = parse.geometry_groups[face.group];
        const string mtl_name = face.material >= 0 ?
                                parse.materials[face.material] :
                                "";
        wf_group &group = flat[make_pair(group_name, mtl_name)];

        group.material = mtl_name;

        for (int k = 0; k < 3; k++) {
            group.triangle_indices.push_back(i*3 + k);
        }
    }

    for (const pair<pair<string, string>, wf_group> &p : flat) {
        const string group_name = p.first.first;
        const wf_group &group = p.second;
        vector<wf_group> &dest = ret[group_name];
        dest.push_back(group);
    }
}

static wf_mesh to_mesh(const Parse &parse)
{
    wf_mesh ret;
    ret.vertex4 = to_floats<vec4>(
        deref_triangles<Face::VERTEX>(
            parse.faces, parse.vertices));

    ret.texture2 = to_floats<vec2>(
        deref_triangles<Face::TEXTURE>(
            parse.faces, parse.textures));

    ret.normal3  = to_floats<vec3>(
        deref_triangles<Face::NORMAL>(
            parse.faces, parse.normals));

    ret.mtllibs = parse.mtllibs;
    groupify(parse, ret.groups);
    return ret;
}

void dump_parse(const Parse &parse)
{
    int i = 0;
    for (auto v : parse.vertices) {
        printf("%d vert(%g, %g, %g, %g)\n", i++, v.x, v.y, v.z, v.w);
    }
    i = 0;
    for (auto v : parse.normals) {
        printf("%d norm(%g, %g, %g)\n", i++, v.x, v.y, v.z);
    }

    for (auto v : parse.textures) {
        printf("tex(%g, %g)\n", v.s, v.t);
    }

    i = 0;
    for (auto f : parse.faces) {
        printf("%d face", i++);
        for (int i = 0; i < 3; i++) {
            printf(" %d/%d/%d", f.vertex_indices[i], f.texture_indices[i],
                                f.normal_indices[i]);
        }
        printf("\n");
    }

    for (const auto &pair : parse.vert_to_faces) {
        printf("Vertex %d used by faces:", pair.first);
        for (const auto &index : pair.second) {
            printf(" %d", index);
        }
        printf("\n");
    }
}

static Parse parse_objfile(FILE *fp)
{
    Parse parse;
    char buf[2048];

    while (fgets(buf, sizeof(buf), fp)) {
        strip_right(buf);
        const char *line = strip_left(buf);

        if (!*line || *line == '#' || *line == '\n') {
            continue;
        }

        assert(
            parse.parse_mtllib(line) ||
            parse.parse_use_material(line) ||
            parse.parse_smoothing_group(line) ||
            parse.parse_group(line) ||
            parse.parse_vertex(line) ||
            parse.parse_normal(line) ||
            parse.parse_texture(line) ||
            parse.parse_face(line) ||
            parse.parse_error(line)
        );
    }

    parse.smooth_normals();
    return parse;
}

void dump_mesh(const wf_mesh &o)
{
    for (unsigned i = 0; i < o.vertex4.size(); i += 4) {
        const float *v = &o.vertex4[i];
        printf("v %g, %g, %g, %g\n", v[0], v[1], v[2], v[3]);
    }

    printf("Vertex float array:");
    for (float x : o.vertex4) {
        printf(" %g", x);
    }
    printf("\n");

    for (unsigned i = 0; i < o.texture2.size(); i += 2) {
        const float *t = &o.texture2[i];
        printf("t %g, %g\n", t[0], t[1]);
    }

    printf("Texture float array:");
    for (float x : o.texture2) {
        printf(" %g", x);
    }
    printf("\n");

    for (unsigned i = 0; i < o.normal3.size(); i += 3){
        const float *n = &o.normal3[i];
        printf("n %g, %g, %g\n", n[0], n[1], n[2]);
    }

    printf("Normal float array:");
    for (float x : o.normal3) {
        printf(" %g", x);
    }
    printf("\n");
}

struct wf_mesh wf_mesh_from_file(const char *path)
{
    FILE* fp = fopen(path, "r");
    if (!fp) {
        perror(path);
        return wf_mesh();
    }

    return to_mesh(parse_objfile(fp));
}

#ifdef TEST
int main()
{
    Parse p = parse_objfile(stdin);
    wf_mesh o = to_mesh(p);

    dump_parse(p);
    dump_mesh(o);
    return 0;
}
#endif
