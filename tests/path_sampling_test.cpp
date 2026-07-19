// path_sampling_test.cpp
#include "path_sampling.h"

#include <cmath>
#include <iostream>

namespace {

bool close_enough(double left, double right, double tolerance = 1e-12) {
    return std::fabs(left - right) <= tolerance;
}

int fail(const char* message) {
    std::cerr << "Path sampling test failed: " << message << '\n';
    return 1;
}

} // 匿名命名空间

int main() {
    const russian_roulette_config config{true, 5, .05, .95};
    if (russian_roulette_enabled_for_depth(config, 4)
        || !russian_roulette_enabled_for_depth(config, 5)) {
        return fail("Russian roulette start depth is incorrect");
    }

    russian_roulette_config disabled = config;
    disabled.enabled = false;
    if (russian_roulette_enabled_for_depth(disabled, 50))
        return fail("disabled Russian roulette was activated");

    if (!close_enough(
            russian_roulette_survival_probability(color(.4, .2, .1), config),
            .4)
        || !close_enough(
            russian_roulette_survival_probability(color(.001, .002, .003), config),
            .05)
        || !close_enough(
            russian_roulette_survival_probability(color(2, 1, .5), config),
            .95)
        || !close_enough(
            russian_roulette_survival_probability(color(0, 0, 0), config),
            0)) {
        return fail("throughput-based survival probability is incorrect");
    }

    const double survival_probability = .4;
    const double original_contribution = .3;
    const double expected_after_roulette = survival_probability
        * (original_contribution / survival_probability);
    if (!close_enough(expected_after_roulette, original_contribution))
        return fail("Russian roulette compensation is biased");

    std::cout << "Path sampling tests passed.\n";
    return 0;
}
