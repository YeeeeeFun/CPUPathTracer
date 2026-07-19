// constant_medium.h
#ifndef CONSTANT_MEDIUM_H
#define CONSTANT_MEDIUM_H

#include "rtweekend.h"
#include "hittable.h"
#include "material.h"
#include "texture.h"

#include <cmath>
#include <stdexcept>

inline double sample_homogeneous_medium_distance(
    double density,
    double uniform_sample) {
    if (!std::isfinite(density) || density <= 0.0)
        throw std::invalid_argument("Medium density must be finite and positive.");
    if (!std::isfinite(uniform_sample)
        || uniform_sample < 0.0
        || uniform_sample >= 1.0) {
        throw std::invalid_argument("Medium random sample must be in [0, 1).");
    }

    return -std::log1p(-uniform_sample) / density;
}

class constant_medium : public hittable {
  public:
    constant_medium(
        shared_ptr<hittable> boundary,
        double density,
        shared_ptr<texture> tex,
        double asymmetry = 0.0)
      : boundary(validated_boundary(std::move(boundary))),
        density(validated_density(density)),
        phase_function(make_shared<henyey_greenstein>(
            std::move(tex), asymmetry)) {}

    constant_medium(
        shared_ptr<hittable> boundary,
        double density,
        const color& albedo,
        double asymmetry = 0.0)
      : boundary(validated_boundary(std::move(boundary))),
        density(validated_density(density)),
        phase_function(make_shared<henyey_greenstein>(
            albedo, asymmetry)) {}

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        hit_record rec1, rec2;

        if (!boundary->hit(r, interval::universe, rec1))
            return false;

        if (!boundary->hit(r, interval(rec1.t+0.0001, infinity), rec2))
            return false;

        if (rec1.t < ray_t.min) rec1.t = ray_t.min;
        if (rec2.t > ray_t.max) rec2.t = ray_t.max;

        if (rec1.t >= rec2.t)
            return false;

        if (rec1.t < 0)
            rec1.t = 0;

        const auto ray_length = r.direction().length();
        const auto distance_inside_boundary = (rec2.t - rec1.t) * ray_length;
        const auto hit_distance = sample_homogeneous_medium_distance(
            density, random_double());

        if (hit_distance > distance_inside_boundary)
            return false;

        rec.t = rec1.t + hit_distance / ray_length;
        rec.p = r.at(rec.t);

        rec.normal = vec3(1,0,0);  // 任意
        rec.front_face = true;     // 任意
        rec.mat = phase_function;

        return true;
    }

    aabb bounding_box() const override { return boundary->bounding_box(); }

  private:
    static shared_ptr<hittable> validated_boundary(
        shared_ptr<hittable> value) {
        if (!value)
            throw std::invalid_argument("Medium boundary must not be null.");
        return value;
    }

    static double validated_density(double value) {
        if (!std::isfinite(value) || value <= 0.0)
            throw std::invalid_argument("Medium density must be finite and positive.");
        return value;
    }

    shared_ptr<hittable> boundary;
    double density;
    shared_ptr<material> phase_function;
};

#endif
