// bump_test.cpp
#include "rtweekend.h"
#include "material.h"

#include <cmath>
#include <iostream>

namespace {

bool close_enough(double left, double right, double tolerance = 1e-9) {
    return std::fabs(left - right) <= tolerance;
}

int fail(const char* message) {
    std::cerr << "Bump test failed: " << message << '\n';
    return 1;
}

} // 匿名命名空间

int main() {
    const vec3 normal(0, 0, 1);
    const vec3 tangent(1, 0, 0);
    const vec3 bitangent(0, 1, 0);

    const vec3 unchanged = bump_normal_from_gradient(
        normal, tangent, bitangent, 0, 0);
    if (!close_enough(unchanged.x(), 0)
        || !close_enough(unchanged.y(), 0)
        || !close_enough(unchanged.z(), 1)) {
        return fail("a flat height map changed the normal");
    }

    const vec3 bumped = bump_normal_from_gradient(
        normal, tangent, bitangent, 1, 0);
    if (!(bumped.x() < 0)
        || !close_enough(bumped.y(), 0)
        || !(bumped.z() > 0)
        || !close_enough(bumped.length(), 1)) {
        return fail("height gradient produced an invalid normal");
    }

    std::cout << "Bump tests passed.\n";
    return 0;
}
