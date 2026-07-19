// texture.h
#ifndef TEXTURE_H
#define TEXTURE_H

#include "rtweekend.h"
#include "color.h"
#include "rtw_stb_image.h"
#include "perlin.h"

#include <algorithm>
#include <cmath>
#include <utility>

class texture {
  public:
    virtual ~texture() = default;

    virtual color value(double u, double v, const point3& p) const = 0;
};

inline double texture_luminance(const color& value) {
    return 0.2126 * value.x() + 0.7152 * value.y() + 0.0722 * value.z();
}

class solid_color : public texture {
  public:
    solid_color(color c) : color_value(c) {}

    solid_color(double red, double green, double blue) : solid_color(color(red,green,blue)) {}

    color value(double u, double v, const point3& p) const override {
        return color_value;
    }

  private:
    color color_value;
};

class tinted_texture : public texture {
  public:
    tinted_texture(shared_ptr<texture> source, const color& tint)
      : source(std::move(source)), tint(tint) {}

    color value(double u, double v, const point3& p) const override {
        return tint * source->value(u, v, p);
    }

  private:
    shared_ptr<texture> source;
    color tint;
};

class checker_texture : public texture {
  public:
    checker_texture(double _scale, shared_ptr<texture> _even, shared_ptr<texture> _odd)
      : inv_scale(1.0 / _scale), even(_even), odd(_odd) {}

    checker_texture(double _scale, color c1, color c2)
      : inv_scale(1.0 / _scale),
        even(make_shared<solid_color>(c1)),
        odd(make_shared<solid_color>(c2))
    {}

    color value(double u, double v, const point3& p) const override {
        auto xInteger = static_cast<int>(std::floor(inv_scale * p.x()));
        auto yInteger = static_cast<int>(std::floor(inv_scale * p.y()));
        auto zInteger = static_cast<int>(std::floor(inv_scale * p.z()));

        bool isEven = (xInteger + yInteger + zInteger) % 2 == 0;

        return isEven ? even->value(u, v, p) : odd->value(u, v, p);
    }

  private:
    double inv_scale;
    shared_ptr<texture> even;
    shared_ptr<texture> odd;
};

enum class image_color_space {
    srgb,
    linear
};

inline color bilinear_interpolate(
    const color& c00,
    const color& c10,
    const color& c01,
    const color& c11,
    double tx,
    double ty) {
    const color top = (1.0 - tx) * c00 + tx * c10;
    const color bottom = (1.0 - tx) * c01 + tx * c11;
    return (1.0 - ty) * top + ty * bottom;
}

class image_texture : public texture {
  public:
    explicit image_texture(
        const char* filename,
        image_color_space color_space = image_color_space::srgb)
      : image(filename), color_space(color_space) {}

    color value(double u, double v, const point3& p) const override {
        (void)p;

        // 如果没有纹理数据，则返回纯青色作为调试辅助。
        if (image.height() <= 0) return color(0,1,1);

        // OBJ 的 UV 原点位于左下角，而图像行从顶部开始。
        u = interval(0,1).clamp(u);
        v = 1.0 - interval(0,1).clamp(v);

        const double x = u * (image.width() - 1);
        const double y = v * (image.height() - 1);
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const int x1 = std::min(x0 + 1, image.width() - 1);
        const int y1 = std::min(y0 + 1, image.height() - 1);

        return bilinear_interpolate(
            texel(x0, y0),
            texel(x1, y0),
            texel(x0, y1),
            texel(x1, y1),
            x - x0,
            y - y0);
    }

    int width() const { return image.width(); }
    int height() const { return image.height(); }

  private:
    color texel(int x, int y) const {
        const auto pixel = image.pixel_data(x, y);
        constexpr double byte_to_unit = 1.0 / 255.0;
        color result(
            byte_to_unit * pixel[0],
            byte_to_unit * pixel[1],
            byte_to_unit * pixel[2]);

        if (color_space == image_color_space::srgb) {
            result = color(
                srgb_to_linear(result.x()),
                srgb_to_linear(result.y()),
                srgb_to_linear(result.z()));
        }

        return result;
    }

    rtw_image image;
    image_color_space color_space;
};

class noise_texture : public texture {
  public:
    noise_texture(double scale) : scale(scale) {}

    color value(double u, double v, const point3& p) const override {
        return color(.5, .5, .5) * (1 + std::sin(scale * p.z() + 10 * noise.turb(p, 7)));
    }

  private:
    perlin noise;
    double scale;
};

#endif
