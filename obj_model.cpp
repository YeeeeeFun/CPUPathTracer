// obj_model.cpp
#include "obj_model.h"

#include "bvh.h"
#include "hittable_list.h"
#include "material.h"
#include "triangle.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "external/tiny_obj_loader.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct normal_key {
    int vertex_index;
    unsigned int smoothing_group;

    bool operator<(const normal_key& other) const {
        if (vertex_index != other.vertex_index)
            return vertex_index < other.vertex_index;
        return smoothing_group < other.smoothing_group;
    }
};

std::filesystem::path resolve_mtl_texture_path(
    const std::filesystem::path& model_directory,
    const std::string& texture_name) {
    if (texture_name.empty())
        return {};

    std::string normalized_name = texture_name;
    std::replace(normalized_name.begin(), normalized_name.end(), '\\', '/');
    const std::filesystem::path referenced_path(normalized_name);

    std::vector<std::filesystem::path> candidates;
    if (referenced_path.is_absolute())
        candidates.push_back(referenced_path);
    else
        candidates.push_back(model_directory / referenced_path);
    candidates.push_back(model_directory / referenced_path.filename());

    for (const auto& candidate : candidates) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error) && !error)
            return candidate;
    }
    return {};
}

std::vector<std::shared_ptr<material>> make_mtl_materials(
    const std::vector<tinyobj::material_t>& source_materials,
    const std::filesystem::path& model_directory) {
    std::vector<std::shared_ptr<material>> result;
    result.reserve(source_materials.size());

    for (const auto& source : source_materials) {
        const color diffuse_color(
            source.diffuse[0], source.diffuse[1], source.diffuse[2]);
        std::shared_ptr<texture> albedo =
            std::make_shared<solid_color>(diffuse_color);

        if (!source.diffuse_texname.empty()) {
            const auto texture_path = resolve_mtl_texture_path(
                model_directory, source.diffuse_texname);
            if (texture_path.empty()) {
                std::clog << "MTL warning: diffuse texture not found for material '"
                          << source.name << "': " << source.diffuse_texname << '\n';
            } else {
                auto image = std::make_shared<image_texture>(texture_path.string().c_str());
                if (image->width() > 0 && image->height() > 0) {
                    albedo = std::make_shared<tinted_texture>(image, diffuse_color);
                } else {
                    std::clog << "MTL warning: unsupported or unreadable diffuse texture for material '"
                              << source.name << "': " << texture_path.string() << '\n';
                }
            }
        }

        result.push_back(std::make_shared<lambertian>(albedo));
    }
    return result;
}

unsigned int effective_smoothing_group(
    const tinyobj::shape_t& shape,
    std::size_t face,
    obj_missing_normal_mode mode) {
    if (mode == obj_missing_normal_mode::smooth)
        return 1;
    if (mode == obj_missing_normal_mode::smoothing_groups
        && face < shape.mesh.smoothing_group_ids.size()) {
        return shape.mesh.smoothing_group_ids[face];
    }
    return 0;
}

int append_normal(tinyobj::attrib_t& attributes, const vec3& area_normal) {
    const vec3 normal = unit_vector(area_normal);
    const int normal_index = static_cast<int>(attributes.normals.size() / 3);
    attributes.normals.push_back(static_cast<tinyobj::real_t>(normal.x()));
    attributes.normals.push_back(static_cast<tinyobj::real_t>(normal.y()));
    attributes.normals.push_back(static_cast<tinyobj::real_t>(normal.z()));
    return normal_index;
}

vec3 read_obj_position(
    const tinyobj::attrib_t& attributes,
    int vertex_index,
    const std::filesystem::path& filename) {
    if (vertex_index < 0)
        throw std::runtime_error("OBJ face has no vertex index: " + filename.string());

    const auto offset = static_cast<std::size_t>(3 * vertex_index);
    if (offset + 2 >= attributes.vertices.size())
        throw std::runtime_error("OBJ vertex index is out of range: " + filename.string());

    return point3(
        attributes.vertices[offset],
        attributes.vertices[offset + 1],
        attributes.vertices[offset + 2]);
}

std::size_t generate_missing_normals(
    tinyobj::attrib_t& attributes,
    std::vector<tinyobj::shape_t>& shapes,
    obj_missing_normal_mode mode,
    const std::filesystem::path& filename) {
    std::size_t generated_count = 0;

    for (auto& shape : shapes) {
        const std::size_t face_count = shape.mesh.num_face_vertices.size();
        std::vector<std::size_t> face_offsets(face_count);
        std::vector<vec3> face_normals(face_count);
        std::map<normal_key, vec3> accumulated_normals;

        std::size_t index_offset = 0;
        for (std::size_t face = 0; face < face_count; ++face) {
            face_offsets[face] = index_offset;
            const std::size_t vertex_count = shape.mesh.num_face_vertices[face];
            if (vertex_count >= 3) {
                const point3 p0 = read_obj_position(
                    attributes,
                    shape.mesh.indices[index_offset].vertex_index,
                    filename);
                vec3 area_normal;
                for (std::size_t vertex = 1; vertex + 1 < vertex_count; ++vertex) {
                    const point3 p1 = read_obj_position(
                        attributes,
                        shape.mesh.indices[index_offset + vertex].vertex_index,
                        filename);
                    const point3 p2 = read_obj_position(
                        attributes,
                        shape.mesh.indices[index_offset + vertex + 1].vertex_index,
                        filename);
                    area_normal += cross(p1 - p0, p2 - p0);
                }
                face_normals[face] = area_normal;

                const unsigned int smoothing_group = effective_smoothing_group(
                    shape, face, mode);

                if (smoothing_group != 0 && !area_normal.near_zero()) {
                    for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
                        const auto& index = shape.mesh.indices[index_offset + vertex];
                        if (index.normal_index < 0) {
                            accumulated_normals[
                                normal_key{index.vertex_index, smoothing_group}] += area_normal;
                        }
                    }
                }
            }
            index_offset += vertex_count;
        }

        std::map<normal_key, int> generated_indices;
        for (const auto& entry : accumulated_normals) {
            if (entry.second.near_zero())
                continue;
            generated_indices[entry.first] = append_normal(attributes, entry.second);
            ++generated_count;
        }

        for (std::size_t face = 0; face < face_count; ++face) {
            const std::size_t vertex_count = shape.mesh.num_face_vertices[face];
            const std::size_t offset = face_offsets[face];
            const unsigned int smoothing_group = effective_smoothing_group(
                shape, face, mode);

            int flat_normal_index = -1;
            for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
                auto& index = shape.mesh.indices[offset + vertex];
                if (index.normal_index >= 0)
                    continue;

                const auto smooth_normal = generated_indices.find(
                    normal_key{index.vertex_index, smoothing_group});
                if (smoothing_group != 0 && smooth_normal != generated_indices.end()) {
                    index.normal_index = smooth_normal->second;
                    continue;
                }

                if (flat_normal_index < 0 && !face_normals[face].near_zero()) {
                    flat_normal_index = append_normal(attributes, face_normals[face]);
                    ++generated_count;
                }
                index.normal_index = flat_normal_index;
            }
        }
    }

    return generated_count;
}

triangle_vertex read_vertex(
    const tinyobj::attrib_t& attributes,
    const tinyobj::index_t& index,
    double scale,
    const vec3& translation,
    const displacement_map& displacement,
    const std::filesystem::path& filename) {
    if (index.vertex_index < 0) {
        throw std::runtime_error("OBJ face has no vertex index: " + filename.string());
    }

    const auto position_offset = static_cast<std::size_t>(3 * index.vertex_index);
    if (position_offset + 2 >= attributes.vertices.size()) {
        throw std::runtime_error("OBJ vertex index is out of range: " + filename.string());
    }

    triangle_vertex vertex;
    vertex.position = point3(
        attributes.vertices[position_offset],
        attributes.vertices[position_offset + 1],
        attributes.vertices[position_offset + 2]) * scale + translation;

    if (index.normal_index >= 0) {
        const auto normal_offset = static_cast<std::size_t>(3 * index.normal_index);
        if (normal_offset + 2 < attributes.normals.size()) {
            vertex.normal = vec3(
                attributes.normals[normal_offset],
                attributes.normals[normal_offset + 1],
                attributes.normals[normal_offset + 2]);
            vertex.has_normal = !vertex.normal.near_zero();
        }
    }

    if (index.texcoord_index >= 0) {
        const auto texcoord_offset = static_cast<std::size_t>(2 * index.texcoord_index);
        if (texcoord_offset + 1 < attributes.texcoords.size()) {
            vertex.u = attributes.texcoords[texcoord_offset];
            vertex.v = attributes.texcoords[texcoord_offset + 1];
            vertex.has_texcoord = true;
        }
    }

    // 在构建三角形及其 BVH 前移动真实顶点，因此求交、轮廓和阴影都会改变。
    if (vertex.has_normal && vertex.has_texcoord)
        vertex.position = displacement.displace(
            vertex.position, vertex.normal, vertex.u, vertex.v);

    return vertex;
}

} // 匿名命名空间

obj_model::obj_model(
    const std::filesystem::path& filename,
    std::shared_ptr<material> fallback_material,
    double uniform_scale,
    const vec3& translation,
    const displacement_map& displacement,
    obj_missing_normal_mode missing_normal_mode,
    bvh_split_method split_method,
    obj_material_mode material_mode) {
    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warning;
    std::string error;

    const std::string filename_string = filename.string();
    const std::string material_directory = filename.parent_path().string() + "/";
    const bool loaded = tinyobj::LoadObj(
        &attributes,
        &shapes,
        &materials,
        &warning,
        &error,
        filename_string.c_str(),
        material_directory.c_str(),
        true);

    if (!warning.empty())
        std::clog << "OBJ warning: " << warning << '\n';
    if (!loaded)
        throw std::runtime_error("Failed to load OBJ '" + filename_string + "': " + error);

    const std::size_t source_normal_count = attributes.normals.size() / 3;
    generated_normals = generate_missing_normals(
        attributes, shapes, missing_normal_mode, filename);
    const auto mtl_materials = material_mode == obj_material_mode::mtl
        ? make_mtl_materials(materials, filename.parent_path())
        : std::vector<std::shared_ptr<material>>{};

    hittable_list triangle_list;
    std::size_t mtl_face_count = 0;
    std::size_t fallback_face_count = 0;

    for (const auto& shape : shapes) {
        std::size_t index_offset = 0;
        for (std::size_t face = 0;
             face < shape.mesh.num_face_vertices.size();
             ++face) {
            const auto face_vertex_count = shape.mesh.num_face_vertices[face];
            if (face_vertex_count < 3) {
                index_offset += face_vertex_count;
                continue;
            }

            std::shared_ptr<material> face_material = fallback_material;
            if (material_mode == obj_material_mode::mtl) {
                const int material_id = face < shape.mesh.material_ids.size()
                    ? shape.mesh.material_ids[face]
                    : -1;
                if (material_id >= 0
                    && static_cast<std::size_t>(material_id) < mtl_materials.size()) {
                    face_material = mtl_materials[material_id];
                    ++mtl_face_count;
                } else {
                    ++fallback_face_count;
                }
            }

            std::vector<triangle_vertex> face_vertices;
            face_vertices.reserve(face_vertex_count);
            for (std::size_t vertex = 0; vertex < face_vertex_count; ++vertex) {
                const auto& index = shape.mesh.indices[index_offset + vertex];
                face_vertices.push_back(read_vertex(
                    attributes,
                    index,
                    uniform_scale,
                    translation,
                    displacement,
                    filename));
            }

            // 加载器默认执行三角化；扇形拆分同时兼容保留多边形面的配置。
            for (std::size_t vertex = 1; vertex + 1 < face_vertices.size(); ++vertex) {
                const vec3 edge1 = face_vertices[vertex].position - face_vertices[0].position;
                const vec3 edge2 = face_vertices[vertex + 1].position - face_vertices[0].position;
                if (cross(edge1, edge2).length_squared() <= 1e-20)
                    continue;

                triangle_list.add(std::make_shared<triangle>(
                    face_vertices[0],
                    face_vertices[vertex],
                    face_vertices[vertex + 1],
                    face_material));
                ++triangles;
            }

            index_offset += face_vertex_count;
        }
    }

    if (triangles == 0)
        throw std::runtime_error("OBJ contains no valid triangles: " + filename_string);

    const auto bvh_start = std::chrono::steady_clock::now();
    const auto acceleration = std::make_shared<bvh_node>(triangle_list, split_method);
    const auto bvh_end = std::chrono::steady_clock::now();
    const auto bvh_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        bvh_end - bvh_start).count();
    root = acceleration;
    bbox = root->bounding_box();
    const auto& bvh_stats = acceleration->statistics();
    std::clog << "Loaded OBJ: " << filename_string
              << " (" << triangles << " triangles, "
              << source_normal_count << " source normals, "
              << generated_normals << " generated normals, "
              << attributes.texcoords.size() / 2 << " texcoords, "
              << shapes.size() << " shapes, "
              << "BVH " << bvh_split_method_name(split_method)
              << " " << bvh_milliseconds << " ms"
              << ", " << bvh_stats.node_count << " nodes"
              << ", depth " << bvh_stats.max_depth
              << ", " << bvh_stats.sah_split_count << " SAH splits"
              << ", " << bvh_stats.median_split_count << " median fallbacks";
    if (material_mode == obj_material_mode::mtl) {
        std::clog << ", " << materials.size() << " MTL materials"
                  << ", " << mtl_face_count << " MTL faces"
                  << ", " << fallback_face_count << " fallback faces";
    }
    if (displacement.enabled())
        std::clog << ", displacement scale " << displacement.distance_scale();
    std::clog << ")\n";
}

bool obj_model::hit(const ray& r, interval ray_t, hit_record& rec) const {
    return root->hit(r, ray_t, rec);
}

aabb obj_model::bounding_box() const {
    return bbox;
}
