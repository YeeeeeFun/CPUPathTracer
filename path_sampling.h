// path_sampling.h
#ifndef PATH_SAMPLING_H
#define PATH_SAMPLING_H

#include "rtweekend.h"
#include "color.h"

#include <algorithm>
#include <cmath>

struct russian_roulette_config {
    bool enabled = true;
    int start_depth = 5;
    double min_survival_probability = .05;
    double max_survival_probability = .95;
};

inline bool russian_roulette_enabled_for_depth(
    const russian_roulette_config& config,
    int path_depth) {
    return config.enabled && path_depth >= std::max(0, config.start_depth);
}

inline double russian_roulette_survival_probability(
    const color& path_throughput,
    const russian_roulette_config& config) {
    const double minimum = std::clamp(
        config.min_survival_probability, 0.0, 1.0);
    const double maximum = std::clamp(
        config.max_survival_probability, minimum, 1.0);
    double largest_component = 0;
    for (int component = 0; component < 3; ++component) {
        const double value = path_throughput[component];
        if (std::isinf(value) && value > 0)
            return maximum;
        if (std::isfinite(value))
            largest_component = std::max(largest_component, value);
    }

    if (!(largest_component > 0))
        return 0;

    return std::clamp(largest_component, minimum, maximum);
}

#endif
