// rtweekend.h
#ifndef RTWEEKEND_H
#define RTWEEKEND_H

#include <cmath>
#include <limits>
#include <memory>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <random>
#include "interval.h"
#include "constant.h"
// 常用类型

using std::shared_ptr;
using std::make_shared;
using std::sqrt;

// 常量

const double pi = 3.1415926535897932385;

// 实用函数

inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

inline std::mt19937 &local_rng() {
    static thread_local std::mt19937 generator(5489u);
    return generator;
}

inline void seed_random(std::uint64_t seed) {
    std::seed_seq seed_sequence {
        static_cast<std::uint32_t>(seed),
        static_cast<std::uint32_t>(seed >> 32)
    };
    local_rng().seed(seed_sequence);
}

inline double random_double() {
    // 返回 [0,1) 范围内的随机实数。
    return std::generate_canonical<double, 53>(local_rng());
}

inline double random_double(double min, double max) {
    // 返回 [min,max) 范围内的随机实数。
    return min + (max-min)*random_double();
}

inline int random_int(int min, int max) {
    // 返回 [min,max] 范围内的随机整数。
    return static_cast<int>(random_double(min, max+1));
}

// 通用头文件

#include "ray.h"
#include "vec3.h"

#endif
