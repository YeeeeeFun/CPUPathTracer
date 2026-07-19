// texture_test.cpp
#include "texture.h"

#include <cmath>
#include <iostream>

namespace {

bool close_enough(double left, double right, double tolerance = 1e-7) {
    return std::fabs(left - right) <= tolerance;
}

int fail(const char* message) {
    std::cerr << "Texture test failed: " << message << '\n';
    return 1;
}

} // 匿名命名空间

int main() {
    for (const double srgb : {0.0, 0.04, 0.18, 0.5, 1.0}) {
        const double round_trip = linear_to_srgb(srgb_to_linear(srgb));
        if (!close_enough(round_trip, srgb))
            return fail("sRGB round trip is inaccurate");
    }

    const color red(1, 0, 0);
    const color green(0, 1, 0);
    const color blue(0, 0, 1);
    const color white(1, 1, 1);
    const color center = bilinear_interpolate(red, green, blue, white, .5, .5);
    if (!close_enough(center.x(), .5)
        || !close_enough(center.y(), .5)
        || !close_enough(center.z(), .5)) {
        return fail("bilinear center sample is incorrect");
    }

    const color corner = bilinear_interpolate(red, green, blue, white, 0, 0);
    if (!close_enough(corner.x(), 1)
        || !close_enough(corner.y(), 0)
        || !close_enough(corner.z(), 0)) {
        return fail("bilinear corner sample is incorrect");
    }

    auto source = std::make_shared<solid_color>(color(.5, .25, 1.0));
    tinted_texture tinted(source, color(.2, .4, .5));
    const color tinted_value = tinted.value(.3, .7, point3(1, 2, 3));
    if (!close_enough(tinted_value.x(), .1)
        || !close_enough(tinted_value.y(), .1)
        || !close_enough(tinted_value.z(), .5)) {
        return fail("tinted texture did not multiply its source color");
    }

    std::cout << "Texture tests passed.\n";
    return 0;
}
