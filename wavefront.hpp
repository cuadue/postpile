#pragma once
#include <vector>
#include <map>
#include <string>

#include "wavefront_mtl.hpp"

struct wf_group {
    std::vector<unsigned> triangle_indices;
    std::string material;
};

struct wf_mesh {
    std::vector<float> vertex4;
    std::vector<float> texture2;
    std::vector<float> normal3;

    /* Maps geometry group name to multiple drawable entities, each of which
     * uses a different material */
    std::map<std::string, std::vector<wf_group>> groups;
    std::vector<std::string> mtllibs;

    bool has_texture_coords() const;
    bool has_normals() const;
};

struct wf_mesh wf_mesh_from_file(const char *path);
void dump_mesh(const wf_mesh &);
