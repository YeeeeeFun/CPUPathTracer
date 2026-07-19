// participating_medium_test.cpp
#include "constant_medium.h"
#include "sphere.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

bool close_enough(double left, double right, double tolerance = 1e-12) {
    return std::fabs(left - right) <= tolerance;
}

int fail(const char* message) {
    std::cerr << "Participating medium test failed: " << message << '\n';
    return 1;
}

template <typename Function>
bool throws_invalid_argument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

} // 匿名命名空间

int main() {
    const double density = .25;
    const double sample = .75;
    const double expected_distance = -std::log1p(-sample) / density;
    if (!close_enough(
            sample_homogeneous_medium_distance(density, sample),
            expected_distance)) {
        return fail("exponential free-flight sampling is incorrect");
    }

    if (!close_enough(
            sample_homogeneous_medium_distance(2.0 * density, sample),
            .5 * expected_distance)) {
        return fail("free-flight distance does not scale inversely with density");
    }

    if (!throws_invalid_argument([] {
            sample_homogeneous_medium_distance(0.0, .5);
        })
        || !throws_invalid_argument([] {
            sample_homogeneous_medium_distance(1.0, 1.0);
        })) {
        return fail("invalid free-flight parameters were accepted");
    }

    const double isotropic = 1.0 / (4.0 * pi);
    if (!close_enough(henyey_greenstein_phase(-1.0, 0.0), isotropic)
        || !close_enough(henyey_greenstein_phase(0.0, 0.0), isotropic)
        || !close_enough(henyey_greenstein_phase(1.0, 0.0), isotropic)) {
        return fail("g=0 is not isotropic");
    }

    constexpr double forward_g = .6;
    if (!(henyey_greenstein_phase(1.0, forward_g)
            > henyey_greenstein_phase(0.0, forward_g)
        && henyey_greenstein_phase(0.0, forward_g)
            > henyey_greenstein_phase(-1.0, forward_g))) {
        return fail("positive g does not favor forward scattering");
    }

    constexpr int integration_steps = 200000;
    double phase_integral = 0.0;
    for (int index = 0; index < integration_steps; ++index) {
        const double cosine_theta = -1.0
            + (2.0 * index + 1.0) / integration_steps;
        phase_integral += henyey_greenstein_phase(cosine_theta, forward_g);
    }
    phase_integral *= 4.0 * pi / integration_steps;
    if (!close_enough(phase_integral, 1.0, 1e-8))
        return fail("Henyey-Greenstein phase function is not normalized");

    seed_random(20260721);
    henyey_greenstein_pdf phase_pdf(vec3(0, 0, 1), forward_g);
    constexpr int sample_count = 200000;
    double mean_cosine = 0.0;
    for (int index = 0; index < sample_count; ++index)
        mean_cosine += phase_pdf.generate().z();
    mean_cosine /= sample_count;
    if (!close_enough(mean_cosine, forward_g, .01))
        return fail("phase-function samples do not reproduce g");

    auto boundary = make_shared<sphere>(
        point3(0, 0, 0), 1.0, shared_ptr<material>());
    if (!throws_invalid_argument([&boundary] {
            constant_medium invalid_medium(boundary, 0.0, color(1, 1, 1));
        })
        || !throws_invalid_argument([&boundary] {
            constant_medium invalid_medium(boundary, 1.0, color(1, 1, 1), 1.0);
        })) {
        return fail("invalid medium parameters were accepted");
    }

    std::cout << "Participating medium tests passed.\n";
    return 0;
}
