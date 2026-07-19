// triangle_test.cpp
#include "rtweekend.h"
#include "triangle.h"

#include <cmath>
#include <iostream>
#include <memory>

namespace {

bool close_enough(double left, double right) {
    return std::fabs(left - right) < 1e-9;
}

int fail(const char* message) {
    std::cerr << "Triangle test failed: " << message << '\n';
    return 1;
}

} // 匿名命名空间

int main() {
    triangle_vertex v0;
    triangle_vertex v1;
    triangle_vertex v2;
    v0.position = point3(0, 0, 0);
    v1.position = point3(1, 0, 0);
    v2.position = point3(0, 1, 0);
    v0.u = 0; v0.v = 0; v0.has_texcoord = true;
    v1.u = 1; v1.v = 0; v1.has_texcoord = true;
    v2.u = 0; v2.v = 1; v2.has_texcoord = true;

    triangle subject(v0, v1, v2, std::shared_ptr<material>());
    hit_record record;

    const ray front_ray(point3(.25, .25, 1), vec3(0, 0, -1));
    if (!subject.hit(front_ray, interval(.001, infinity), record))
        return fail("front ray did not hit");
    if (!record.front_face || record.normal.z() <= 0)
        return fail("front face or normal is incorrect");
    if (!close_enough(record.t, 1) || !close_enough(record.u, .25) || !close_enough(record.v, .25))
        return fail("distance or interpolated UV is incorrect");
    if (!record.has_tangent_space
        || !close_enough(record.tangent.x(), 1)
        || !close_enough(record.tangent.y(), 0)
        || !close_enough(record.tangent.z(), 0)
        || !close_enough(record.bitangent.x(), 0)
        || !close_enough(record.bitangent.y(), 1)
        || !close_enough(record.bitangent.z(), 0)) {
        return fail("UV tangent space is incorrect");
    }

    const ray back_ray(point3(.25, .25, -1), vec3(0, 0, 1));
    if (!subject.hit(back_ray, interval(.001, infinity), record))
        return fail("back ray was incorrectly culled");
    if (record.front_face || record.normal.z() >= 0)
        return fail("back face or oriented normal is incorrect");
    if (!record.has_tangent_space || record.bitangent.y() >= 0)
        return fail("back-face tangent space is incorrectly oriented");

    const ray miss_ray(point3(.8, .8, 1), vec3(0, 0, -1));
    if (subject.hit(miss_ray, interval(.001, infinity), record))
        return fail("ray outside the triangle reported a hit");

    std::cout << "Triangle tests passed.\n";
    return 0;
}
