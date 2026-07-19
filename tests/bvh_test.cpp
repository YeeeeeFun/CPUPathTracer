// bvh_test.cpp
#include "rtweekend.h"
#include "bvh.h"
#include "triangle.h"

#include <cmath>
#include <iostream>
#include <memory>

namespace {

std::shared_ptr<triangle> make_triangle(double z, double x = 0) {
    triangle_vertex v0;
    triangle_vertex v1;
    triangle_vertex v2;
    v0.position = point3(x, 0, z);
    v1.position = point3(x + .4, 0, z);
    v2.position = point3(x, .4, z);
    return std::make_shared<triangle>(v0, v1, v2, std::shared_ptr<material>());
}

int fail(const char* message) {
    std::cerr << "BVH test failed: " << message << '\n';
    return 1;
}

} // 匿名命名空间

int main() {
    hittable_list objects;
    objects.add(make_triangle(0));
    objects.add(make_triangle(-2));
    bvh_node bvh(objects, bvh_split_method::sah);

    const ray hit_ray(point3(.1, .1, 1), vec3(0, 0, -1));
    hit_record list_record;
    hit_record bvh_record;
    if (!objects.hit(hit_ray, interval(.001, infinity), list_record))
        return fail("linear list did not hit");
    if (!bvh.hit(hit_ray, interval(.001, infinity), bvh_record))
        return fail("BVH did not hit");
    if (std::fabs(list_record.t - bvh_record.t) > 1e-9)
        return fail("BVH and linear list returned different nearest hits");

    const ray miss_ray(point3(2, 2, 1), vec3(0, 0, -1));
    if (bvh.hit(miss_ray, interval(.001, infinity), bvh_record))
        return fail("BVH reported a false hit");

    hittable_list single_object;
    single_object.add(make_triangle(0));
    bvh_node single_leaf_bvh(single_object);
    if (!single_leaf_bvh.hit(hit_ray, interval(.001, infinity), bvh_record))
        return fail("single-leaf BVH did not hit");

    hittable_list clustered_objects;
    const double positions[] = {0, .5, 1, 8, 8.5, 9, 20, 20.5};
    for (const double x : positions)
        clustered_objects.add(make_triangle(0, x));

    bvh_node median_bvh(clustered_objects, bvh_split_method::median);
    bvh_node sah_bvh(clustered_objects, bvh_split_method::sah);
    if (median_bvh.statistics().sah_split_count != 0
        || median_bvh.statistics().median_split_count == 0) {
        return fail("median mode did not exclusively use median splits");
    }
    if (sah_bvh.statistics().sah_split_count == 0)
        return fail("SAH mode did not select any SAH split");
    if (sah_bvh.statistics().leaf_count != clustered_objects.objects.size())
        return fail("SAH statistics lost primitives");

    for (const double x : positions) {
        const ray clustered_ray(point3(x + .1, .1, 1), vec3(0, 0, -1));
        hit_record linear_record;
        hit_record median_record;
        hit_record sah_record;
        if (!clustered_objects.hit(
                clustered_ray, interval(.001, infinity), linear_record)
            || !median_bvh.hit(
                clustered_ray, interval(.001, infinity), median_record)
            || !sah_bvh.hit(
                clustered_ray, interval(.001, infinity), sah_record)) {
            return fail("one acceleration mode missed a clustered triangle");
        }
        if (std::fabs(linear_record.t - median_record.t) > 1e-9
            || std::fabs(linear_record.t - sah_record.t) > 1e-9) {
            return fail("SAH, median, and linear traversal disagree");
        }
    }

    // 相同质心无法分配到不同 SAH 分箱，构建器必须回退到中位数划分。
    hittable_list degenerate_objects;
    for (int index = 0; index < 5; ++index)
        degenerate_objects.add(make_triangle(0));
    bvh_node fallback_bvh(degenerate_objects, bvh_split_method::sah);
    if (fallback_bvh.statistics().sah_split_count != 0
        || fallback_bvh.statistics().median_split_count == 0) {
        return fail("degenerate SAH input did not use the median fallback");
    }
    if (!fallback_bvh.hit(hit_ray, interval(.001, infinity), bvh_record))
        return fail("median fallback BVH did not preserve hits");

    // 顶层 BVH 必须能够把另一棵 BVH 作为模型实例包含在内。
    hittable_list mesh_objects;
    mesh_objects.add(make_triangle(0, 0));
    mesh_objects.add(make_triangle(0, .5));
    auto mesh_bvh = std::make_shared<bvh_node>(
        mesh_objects, bvh_split_method::sah);
    hittable_list scene_objects;
    scene_objects.add(mesh_bvh);
    scene_objects.add(make_triangle(0, 10));
    bvh_node scene_bvh(scene_objects, bvh_split_method::sah);
    const ray nested_bvh_ray(point3(.1, .1, 1), vec3(0, 0, -1));
    hit_record scene_list_record;
    hit_record scene_bvh_record;
    if (!scene_objects.hit(
            nested_bvh_ray, interval(.001, infinity), scene_list_record)
        || !scene_bvh.hit(
            nested_bvh_ray, interval(.001, infinity), scene_bvh_record)) {
        return fail("top-level BVH missed a nested model BVH");
    }
    if (std::fabs(scene_list_record.t - scene_bvh_record.t) > 1e-9)
        return fail("top-level and linear scene traversal disagree");

    bool rejected_empty_list = false;
    try {
        const hittable_list empty_objects;
        bvh_node invalid_bvh(empty_objects);
    } catch (const std::invalid_argument&) {
        rejected_empty_list = true;
    }
    if (!rejected_empty_list)
        return fail("empty BVH input was not rejected");

    std::cout << "BVH tests passed.\n";
    return 0;
}
