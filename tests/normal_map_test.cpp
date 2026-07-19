// normal_map_test.cpp
#include "rtweekend.h"
#include "material.h"

#include <cmath>
#include <iostream>

namespace {

bool close_enough(double left, double right, double tolerance = 1e-9) {
    return std::fabs(left - right) <= tolerance;
}

int fail(const char* message) {
    std::cerr << "Normal map test failed: " << message << '\n';
    return 1;
}

} // 匿名命名空间

int main() {
    const vec3 flat = decode_tangent_space_normal(color(.5, .5, 1.0));
    if (!close_enough(flat.x(), 0)
        || !close_enough(flat.y(), 0)
        || !close_enough(flat.z(), 1)) {
        return fail("neutral normal-map texel did not decode to +Z");
    }

    const vec3 tilted = decode_tangent_space_normal(color(.75, .5, 1.0));
    if (!(tilted.x() > 0) || !close_enough(tilted.y(), 0) || !(tilted.z() > 0))
        return fail("red channel did not tilt the normal toward +X");

    const vec3 flipped_green = decode_tangent_space_normal(
        color(.5, .75, 1.0), 1.0, true);
    if (!(flipped_green.y() < 0))
        return fail("green-channel convention switch did not invert Y");

    const vec3 world = tangent_space_to_world_normal(
        vec3(0, 0, 1), vec3(1, 0, 0), vec3(0, 1, 0), tilted);
    if (!close_enough(world.x(), tilted.x())
        || !close_enough(world.y(), tilted.y())
        || !close_enough(world.z(), tilted.z())) {
        return fail("TBN transform changed an identity-basis normal");
    }

    std::cout << "Normal map tests passed.\n";
    return 0;
}
