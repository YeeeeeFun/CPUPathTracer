// obj_instance.h
#ifndef OBJ_INSTANCE_H
#define OBJ_INSTANCE_H

#include "bvh.h"
#include "displacement.h"
#include "hittable.h"
#include "obj_model.h"

#include <filesystem>
#include <memory>
#include <utility>

struct object_transform {
    double rotate_y_degrees = 0;
    vec3 translation = vec3(0, 0, 0);
};

inline std::shared_ptr<hittable> apply_object_transform(
    std::shared_ptr<hittable> object,
    const object_transform& transform) {
    if (transform.rotate_y_degrees != 0)
        object = std::make_shared<rotate_y>(object, transform.rotate_y_degrees);
    if (!transform.translation.near_zero())
        object = std::make_shared<translate>(object, transform.translation);
    return object;
}

struct obj_instance_config {
    obj_instance_config(
        std::filesystem::path model_path,
        std::shared_ptr<material> model_material)
      : path(std::move(model_path)), material(std::move(model_material)) {}

    std::filesystem::path path;
    std::shared_ptr<material> material;
    double uniform_scale = 1.0;
    object_transform transform;
    displacement_map displacement;
    obj_missing_normal_mode missing_normal_mode =
        obj_missing_normal_mode::smoothing_groups;
    obj_material_mode material_mode = obj_material_mode::supplied;
};

inline std::shared_ptr<hittable> make_obj_instance(
    const obj_instance_config& config,
    bvh_split_method split_method = bvh_split_method::sah) {
    auto object = std::make_shared<obj_model>(
        config.path,
        config.material,
        config.uniform_scale,
        vec3(0, 0, 0),
        config.displacement,
        config.missing_normal_mode,
        split_method,
        config.material_mode);
    return apply_object_transform(object, config.transform);
}

#endif
