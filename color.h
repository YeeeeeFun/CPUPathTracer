// color.h
#ifndef COLOR_H
#define COLOR_H

#include "vec3.h"
#include "interval.h"
#include <cmath>
#include <iostream>

using color = vec3;

inline double srgb_to_linear(double srgb_component) {
    if (srgb_component <= 0)
        return 0;
    if (srgb_component <= 0.04045)
        return srgb_component / 12.92;

    return std::pow((srgb_component + 0.055) / 1.055, 2.4);
}

inline double linear_to_srgb(double linear_component) {
    if (linear_component <= 0)
        return 0;
    if (linear_component <= 0.0031308)
        return 12.92 * linear_component;

    return 1.055 * std::pow(linear_component, 1.0 / 2.4) - 0.055;
}

inline void write_color(std::ostream& out, const color& pixel_color) {
    auto r = pixel_color.x();
    auto g = pixel_color.y();
    auto b = pixel_color.z();

    // 将 NaN 分量替换为零（NaN 不等于自身）。
    if (r != r) r = 0.0;
    if (g != g) g = 0.0;
    if (b != b) b = 0.0;

    // 将线性辐射亮度转换到标准 sRGB 显示曲线。
    r = linear_to_srgb(r);
    g = linear_to_srgb(g);
    b = linear_to_srgb(b);

    // 将 [0,1] 分量映射到 [0,255] 字节范围。
    static const interval intensity(0.000, 0.999);
    int rbyte = int(256 * intensity.clamp(r));
    int gbyte = int(256 * intensity.clamp(g));
    int bbyte = int(256 * intensity.clamp(b));

    // 写出像素颜色分量。
    out << rbyte << ' ' << gbyte << ' ' << bbyte << '\n';
}

#endif
