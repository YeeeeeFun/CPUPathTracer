// displacement_test.cpp
#include "rtweekend.h"
#include "displacement.h"

#include <cmath>
#include <iostream>
#include <memory>

namespace {

bool close_enough(double left, double right, double tolerance = 1e-9) {
    return std::fabs(left - right) <= tolerance;
}

int fail(const char* message) {
    std::cerr << "Displacement test failed: " << message << '\n';
    return 1;
}

} // 匿名命名空间

int main() {
    const auto height = std::make_shared<solid_color>(color(.75, .75, .75));
    const displacement_map displacement(height, 8.0, .5);
    const point3 displaced = displacement.displace(
        point3(1, 2, 3), vec3(0, 2, 0), .25, .75);

    if (!close_enough(displaced.x(), 1)
        || !close_enough(displaced.y(), 4)
        || !close_enough(displaced.z(), 3)) {
        return fail("vertex did not move by the expected signed distance");
    }

    const auto low_height = std::make_shared<solid_color>(color(.25, .25, .25));
    const displacement_map inward_displacement(low_height, 8.0, .5);
    const point3 inward = inward_displacement.displace(
        point3(1, 2, 3), vec3(0, 2, 0), .25, .75);
    if (!close_enough(inward.y(), 0))
        return fail("height below the midpoint did not move inward");

    const displacement_map disabled;
    const point3 unchanged = disabled.displace(
        point3(1, 2, 3), vec3(0, 1, 0), .25, .75);
    if (!close_enough(unchanged.x(), 1)
        || !close_enough(unchanged.y(), 2)
        || !close_enough(unchanged.z(), 3)) {
        return fail("disabled displacement changed a vertex");
    }

    std::cout << "Displacement tests passed.\n";
    return 0;
}
