// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include <fstream>
#include <vector>

#include "Open3D/IO/ClassIO/TriangleMeshIO.h"
#include "Open3D/Utility/Console.h"

namespace open3d {
namespace io {

bool ReadTriangleMeshFromOFF(const std::string &filename,
                             geometry::TriangleMesh &mesh) {
    std::ifstream file(filename.c_str(), std::ios::in);
    if (!file) {
        utility::PrintWarning("Read OFF failed: unable to open file: %s\n",
                              filename.c_str());
        return false;
    }

    std::string header;
    std::getline(file, header);
    if (header != "OFF" && header != "COFF" && header != "NOFF" &&
        header != "CNOFF") {
        utility::PrintWarning(
                "Read OFF failed: header keyword '%s' not supported.\n",
                header.c_str());
        return false;
    }

    std::string info;
    unsigned int num_of_vertices, num_of_faces, num_of_edges;
    std::getline(file, info);
    std::istringstream iss(info);
    if (!(iss >> num_of_vertices >> num_of_faces >> num_of_edges)) {
        utility::PrintWarning("Read OFF failed: could not read file info.\n");
        return false;
    }

    if (num_of_vertices == 0 || num_of_faces == 0) {
        utility::PrintWarning(
                "Read OFF failed: mesh has no vertices or faces.\n");
        return false;
    }

    mesh.Clear();
    mesh.vertices_.resize(num_of_vertices);
    bool parse_vertex_normals = false;
    bool parse_vertex_colors = false;
    if (header == "NOFF" || header == "CNOFF") {
        parse_vertex_normals = true;
        mesh.vertex_normals_.resize(num_of_vertices);
    }
    if (header == "COFF" || header == "CNOFF") {
        parse_vertex_colors = true;
        mesh.vertex_colors_.resize(num_of_vertices);
    }

    utility::ResetConsoleProgress(num_of_vertices + num_of_faces,
                                  "Reading OFF: ");

    std::string line;
    float vx, vy, vz;
    float nx, ny, nz;
    float r, g, b, alpha;
    for (int vidx = 0; vidx < num_of_vertices; vidx++) {
        std::getline(file, line);
        std::istringstream iss(line);
        if (!(iss >> vx >> vy >> vz)) {
            utility::PrintWarning(
                    "Read OFF failed: could not read all vertex values.\n");
            return false;
        }
        mesh.vertices_[vidx] = Eigen::Vector3d(vx, vy, vz);

        if (parse_vertex_normals) {
            if (!(iss >> nx >> ny >> nz)) {
                utility::PrintWarning(
                        "Read OFF failed: could not read all vertex normal "
                        "values.\n");
                return false;
            }
            mesh.vertex_normals_[vidx](0) = nx;
            mesh.vertex_normals_[vidx](1) = ny;
            mesh.vertex_normals_[vidx](2) = nz;
        }
        if (parse_vertex_colors) {
            if (!(iss >> r >> g >> b >> alpha)) {
                utility::PrintWarning(
                        "Read OFF failed: could not read all vertex color "
                        "values.\n");
                return false;
            }
            mesh.vertex_colors_[vidx] =
                    Eigen::Vector3d(r / 255, g / 255, b / 255);
        }

        utility::AdvanceConsoleProgress();
    }

    unsigned int n, vertex_index;
    std::vector<unsigned int> indices;
    for (int tidx = 0; tidx < num_of_faces; tidx++) {
        std::getline(file, line);
        std::istringstream iss(line);
        iss >> n;
        indices.clear();
        for (int vidx = 0; vidx < n; vidx++) {
            if (!(iss >> vertex_index)) {
                utility::PrintWarning(
                        "Read OFF failed: could not read all vertex "
                        "indices.\n");
                return false;
            }
            indices.push_back(vertex_index);
        }
        if (!AddTrianglesByEarClipping(mesh, indices)) {
            utility::PrintWarning(
                    "Read OFF failed: A polygon in the mesh could not be "
                    "decomposed into triangles.\n");
            return false;
        }
        utility::AdvanceConsoleProgress();
    }

    file.close();
    return true;
}

bool WriteTriangleMeshToOFF(const std::string &filename,
                            const geometry::TriangleMesh &mesh,
                            bool write_ascii /* = false*/,
                            bool compressed /* = false*/,
                            bool write_vertex_normals /* = true*/,
                            bool write_vertex_colors /* = true*/) {
    std::ofstream file(filename.c_str(), std::ios::out);
    if (!file) {
        utility::PrintWarning("Write OFF failed: unable to open file.\n");
        return false;
    }

    if (mesh.HasTriangleNormals()) {
        utility::PrintWarning("Write OFF cannot include triangle normals.\n");
    }

    size_t num_of_vertices = mesh.vertices_.size();
    size_t num_of_triangles = mesh.triangles_.size();
    if (num_of_vertices == 0 || num_of_triangles == 0) {
        utility::PrintWarning("Write OFF failed: empty file.\n");
        return false;
    }

    write_vertex_normals = write_vertex_normals && mesh.HasVertexNormals();
    write_vertex_colors = write_vertex_colors && mesh.HasVertexColors();
    if (write_vertex_colors) {
        file << "C";
    }
    if (write_vertex_normals) {
        file << "N";
    }
    file << "OFF\n";
    file << num_of_vertices << " " << num_of_triangles << " 0\n";

    utility::ResetConsoleProgress(num_of_vertices + num_of_triangles,
                                  "Writing OFF: ");
    for (int vidx = 0; vidx < num_of_vertices; ++vidx) {
        const Eigen::Vector3d &vertex = mesh.vertices_[vidx];
        file << vertex(0) << " " << vertex(1) << " " << vertex(2);
        if (write_vertex_normals) {
            const Eigen::Vector3d &normal = mesh.vertex_normals_[vidx];
            file << " " << normal(0) << " " << normal(1) << " " << normal(2);
        }
        if (write_vertex_colors) {
            const Eigen::Vector3d &color = mesh.vertex_colors_[vidx];
            file << " " << std::round(color(0) * 255.0) << " "
                 << std::round(color(1) * 255.0) << " "
                 << std::round(color(2) * 255.0) << " 255";
        }
        file << "\n";
        utility::AdvanceConsoleProgress();
    }

    for (int tidx = 0; tidx < num_of_triangles; ++tidx) {
        const Eigen::Vector3i &triangle = mesh.triangles_[tidx];
        file << "3 " << triangle(0) << " " << triangle(1) << " " << triangle(2)
             << "\n";
        utility::AdvanceConsoleProgress();
    }

    file.close();
    return true;
}

}  // namespace io
}  // namespace open3d