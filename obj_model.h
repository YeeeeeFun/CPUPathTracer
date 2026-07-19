// obj_model.h
#ifndef OBJ_MODEL_H
#define OBJ_MODEL_H

#include "rtweekend.h"
#include "hittable.h"
#include "displacement.h"
#include "bvh.h"

#include <cstddef>
#include <filesystem>
#include <memory>

enum class obj_missing_normal_mode {
    smoothing_groups,
    smooth,
    flat
};

enum class obj_material_mode {
    supplied,
    mtl
};

class obj_model : public hittable {
  public:
    obj_model(
        const std::filesystem::path& filename,
        std::shared_ptr<material> fallback_material,
        double uniform_scale = 1.0,
        const vec3& translation = vec3(0, 0, 0),
        const displacement_map& displacement = displacement_map(),
        obj_missing_normal_mode missing_normal_mode =
            obj_missing_normal_mode::smoothing_groups,
        bvh_split_method split_method = bvh_split_method::sah,
        obj_material_mode material_mode = obj_material_mode::supplied);

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override;
    aabb bounding_box() const override;

    std::size_t triangle_count() const { return triangles; }
    std::size_t generated_normal_count() const { return generated_normals; }

  private:
    std::shared_ptr<hittable> root;
    aabb bbox;
    std::size_t triangles = 0;
    std::size_t generated_normals = 0;
};

#endif
