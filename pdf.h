// pdf.h
#ifndef PDF_H
#define PDF_H

#include "onb.h"
#include "hittable_list.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

enum class mis_heuristic {
    balance,
    power
};

inline const char* mis_heuristic_name(mis_heuristic heuristic) {
    return heuristic == mis_heuristic::power ? "power" : "balance";
}

inline double balance_heuristic(double sampled_pdf, double other_pdf) {
    sampled_pdf = std::max(0.0, sampled_pdf);
    other_pdf = std::max(0.0, other_pdf);
    const double sum = sampled_pdf + other_pdf;
    return std::isfinite(sum) && sum > 0 ? sampled_pdf / sum : 0;
}

inline double power_heuristic(double sampled_pdf, double other_pdf) {
    sampled_pdf = std::max(0.0, sampled_pdf);
    other_pdf = std::max(0.0, other_pdf);
    const double scale = std::max(sampled_pdf, other_pdf);
    if (!std::isfinite(scale) || scale <= 0)
        return 0;

    const double sampled = sampled_pdf / scale;
    const double other = other_pdf / scale;
    return sampled * sampled / (sampled * sampled + other * other);
}

inline double mis_weight(
    mis_heuristic heuristic,
    double sampled_pdf,
    double other_pdf) {
    return heuristic == mis_heuristic::power
        ? power_heuristic(sampled_pdf, other_pdf)
        : balance_heuristic(sampled_pdf, other_pdf);
}

class pdf {
  public:
    virtual ~pdf() {}

    virtual double value(const vec3& direction) const = 0;
    virtual vec3 generate() const = 0;
};

inline double henyey_greenstein_phase(double cosine_theta, double asymmetry) {
    cosine_theta = std::clamp(cosine_theta, -1.0, 1.0);
    const double g_squared = asymmetry * asymmetry;
    const double denominator = 1.0 + g_squared - 2.0 * asymmetry * cosine_theta;
    return (1.0 - g_squared)
        / (4.0 * pi * denominator * std::sqrt(denominator));
}

class henyey_greenstein_pdf : public pdf {
  public:
    henyey_greenstein_pdf(const vec3& incoming_direction, double asymmetry)
      : uvw(validated_direction(incoming_direction)),
        asymmetry(validated_asymmetry(asymmetry)) {}

    double value(const vec3& direction) const override {
        if (direction.near_zero())
            return 0.0;

        const double cosine_theta = dot(unit_vector(direction), uvw.w());
        return henyey_greenstein_phase(cosine_theta, asymmetry);
    }

    vec3 generate() const override {
        const double random_cosine = random_double();
        double cosine_theta;

        if (std::fabs(asymmetry) < 1e-3) {
            cosine_theta = 2.0 * random_cosine - 1.0;
        } else {
            const double ratio = (1.0 - asymmetry * asymmetry)
                / (1.0 - asymmetry + 2.0 * asymmetry * random_cosine);
            cosine_theta = (1.0 + asymmetry * asymmetry - ratio * ratio)
                / (2.0 * asymmetry);
            cosine_theta = std::clamp(cosine_theta, -1.0, 1.0);
        }

        const double phi = 2.0 * pi * random_double();
        const double sine_theta = std::sqrt(
            std::max(0.0, 1.0 - cosine_theta * cosine_theta));
        return uvw.transform(vec3(
            sine_theta * std::cos(phi),
            sine_theta * std::sin(phi),
            cosine_theta));
    }

  private:
    static vec3 validated_direction(const vec3& direction) {
        if (direction.near_zero())
            throw std::invalid_argument(
                "Henyey-Greenstein incoming direction must be non-zero.");
        return direction;
    }

    static double validated_asymmetry(double value) {
        if (!std::isfinite(value) || value <= -1.0 || value >= 1.0)
            throw std::invalid_argument(
                "Henyey-Greenstein asymmetry must be finite and in (-1, 1).");
        return value;
    }

    onb uvw;
    double asymmetry;
};

class cosine_pdf : public pdf {
  public:
    cosine_pdf(const vec3& w) : uvw(w) {}

    double value(const vec3& direction) const override {
        auto cosine_theta = dot(unit_vector(direction), uvw.w());
        return std::fmax(0, cosine_theta/pi);
    }

    vec3 generate() const override {
        return uvw.transform(random_cosine_direction());
    }

  private:
    onb uvw;
};

class hittable_pdf : public pdf {
  public:
    hittable_pdf(const hittable& objects, const point3& origin)
      : objects(objects), origin(origin)
    {}

    double value(const vec3& direction) const override {
        return objects.pdf_value(origin, direction);
    }

    vec3 generate() const override {
        return objects.random(origin);
    }

  private:
    const hittable& objects;
    point3 origin;
};

#endif
