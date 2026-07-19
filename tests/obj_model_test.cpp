// obj_model_test.cpp
#include "rtweekend.h"
#include "material.h"
#include "obj_instance.h"
#include "obj_model.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>

namespace {

bool close_enough(double left, double right, double tolerance = 1e-7) {
    return std::fabs(left - right) <= tolerance;
}

int fail(const char* message) {
    std::cerr << "OBJ model test failed: " << message << '\n';
    return 1;
}

bool hit_attenuation(
    const hittable& object,
    const ray& test_ray,
    color& attenuation) {
    hit_record record;
    if (!object.hit(test_ray, interval(.001, infinity), record) || !record.mat)
        return false;
    scatter_record scatter;
    if (!record.mat->scatter(test_ray, record, scatter))
        return false;
    attenuation = scatter.attenuation;
    return true;
}

} // 匿名命名空间

int main() {
    const auto fixture = std::filesystem::path(RT_TEST_SOURCE_DIR)
        / "tests" / "fixtures" / "bent_quad.obj";
    const auto no_material = std::shared_ptr<material>();

    const obj_model smoothing_group_model(fixture, no_material);
    const obj_model smooth_model(
        fixture,
        no_material,
        1.0,
        vec3(0, 0, 0),
        displacement_map(),
        obj_missing_normal_mode::smooth);
    const obj_model flat_model(
        fixture,
        no_material,
        1.0,
        vec3(0, 0, 0),
        displacement_map(),
        obj_missing_normal_mode::flat);

    if (smoothing_group_model.generated_normal_count() != 4)
        return fail("smoothing-group mode ignored the OBJ smoothing group");
    if (smooth_model.generated_normal_count() != 4)
        return fail("smooth mode did not generate one normal per shared vertex");
    if (flat_model.generated_normal_count() != 2)
        return fail("flat mode did not generate one normal per face");

    const ray test_ray(point3(.4, .05, 1), vec3(0, 0, -1));
    hit_record smooth_hit;
    hit_record flat_hit;
    if (!smooth_model.hit(test_ray, interval(.001, infinity), smooth_hit)
        || !flat_model.hit(test_ray, interval(.001, infinity), flat_hit)) {
        return fail("test ray missed the fixture");
    }
    if (!(smooth_hit.normal.y() > 0) || !(smooth_hit.normal.z() > 0))
        return fail("smooth normal did not blend adjacent face normals");
    if (!close_enough(flat_hit.normal.y(), 0)
        || !close_enough(flat_hit.normal.z(), 1)) {
        return fail("flat normal did not preserve the first face normal");
    }

    obj_instance_config instance_config(fixture, no_material);
    instance_config.uniform_scale = 2.0;
    instance_config.transform = {90, vec3(2, 3, 4)};
    const auto transformed_instance = make_obj_instance(
        instance_config, bvh_split_method::sah);
    const ray transformed_ray(point3(3, 3.5, 3.5), vec3(-1, 0, 0));
    hit_record transformed_hit;
    if (!transformed_instance->hit(
            transformed_ray, interval(.001, infinity), transformed_hit)) {
        return fail("configured OBJ instance transform was not hittable");
    }
    if (!close_enough(transformed_hit.t, 1)
        || !close_enough(transformed_hit.p.x(), 2)
        || !close_enough(transformed_hit.p.y(), 3.5)
        || !close_enough(transformed_hit.p.z(), 3.5)) {
        return fail("configured OBJ scale/rotation/translation is incorrect");
    }

    const auto material_fixture = std::filesystem::path(RT_TEST_SOURCE_DIR)
        / "tests" / "fixtures" / "two_materials.obj";
    const auto blue_fallback = std::make_shared<lambertian>(color(0, 0, 1));
    const obj_model mtl_model(
        material_fixture,
        blue_fallback,
        1.0,
        vec3(0, 0, 0),
        displacement_map(),
        obj_missing_normal_mode::smoothing_groups,
        bvh_split_method::sah,
        obj_material_mode::mtl);
    color red_attenuation;
    color green_attenuation;
    if (!hit_attenuation(
            mtl_model,
            ray(point3(.1, .1, 1), vec3(0, 0, -1)),
            red_attenuation)
        || !hit_attenuation(
            mtl_model,
            ray(point3(2.1, .1, 1), vec3(0, 0, -1)),
            green_attenuation)) {
        return fail("MTL faces were not hittable with imported materials");
    }
    if (!close_enough(red_attenuation.x(), 1)
        || !close_enough(red_attenuation.y(), 0)
        || !close_enough(green_attenuation.x(), 0)
        || !close_enough(green_attenuation.y(), 1)) {
        return fail("OBJ faces did not receive their usemtl material");
    }

    const obj_model override_model(material_fixture, blue_fallback);
    color override_attenuation;
    if (!hit_attenuation(
            override_model,
            ray(point3(.1, .1, 1), vec3(0, 0, -1)),
            override_attenuation)
        || !close_enough(override_attenuation.x(), 0)
        || !close_enough(override_attenuation.y(), 0)
        || !close_enough(override_attenuation.z(), 1)) {
        return fail("supplied material mode did not override the MTL material");
    }

    std::cout << "OBJ model tests passed.\n";
    return 0;
}
