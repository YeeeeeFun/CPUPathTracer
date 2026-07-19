// material.h
#ifndef MATERIAL_H
#define MATERIAL_H

#include "hittable.h"
#include "rtweekend.h"
#include "texture.h"
#include "pdf.h"

class hit_record;

class scatter_record {
  public:
    color attenuation;
    shared_ptr<pdf> pdf_ptr;
    bool skip_pdf;
    ray skip_pdf_ray;
};

class material {
  public:
    virtual ~material() = default;

    virtual bool scatter(
        const ray& r_in, const hit_record& rec, scatter_record &srec
    ) const {
        return false;
    }

    virtual color emitted(
        const ray& r_in,
        const hit_record& rec,
        double u,
        double v,
        const point3& p) const {
        return color(0,0,0);
    }

    virtual double scattering_pdf(const ray& r_in, const hit_record& rec, const ray& scattered)
    const {
        return 0;
    }
};

// 朗伯漫反射材质
class lambertian : public material {
  public:
    lambertian(const color& albedo) : tex(make_shared<solid_color>(albedo)) {}
    lambertian(shared_ptr<texture> tex) : tex(tex) {}

    bool scatter(
        const ray& r_in, const hit_record& rec, scatter_record &srec
    ) const override {

        srec.attenuation = tex->value(rec.u, rec.v, rec.p);
        srec.pdf_ptr = make_shared<cosine_pdf>(rec.normal);
        srec.skip_pdf = false;
        return true;
    }

    double scattering_pdf(const ray& r_in, const hit_record& rec, const ray& scattered)
    const override {
        auto cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
        return cos_theta < 0 ? 0 : cos_theta / pi;
    }

  private:
    shared_ptr<texture> tex;
};

inline vec3 tangent_space_to_world_normal(
    const vec3& normal,
    const vec3& tangent,
    const vec3& bitangent,
    const vec3& tangent_space_normal) {
    if (tangent_space_normal.near_zero())
        return normal;

    vec3 result = unit_vector(
        tangent_space_normal.x() * tangent
        + tangent_space_normal.y() * bitangent
        + tangent_space_normal.z() * normal);
    if (dot(result, normal) <= 1e-8)
        return normal;
    return result;
}

inline vec3 bump_normal_from_gradient(
    const vec3& normal,
    const vec3& tangent,
    const vec3& bitangent,
    double height_change_u,
    double height_change_v) {
    return tangent_space_to_world_normal(
        normal,
        tangent,
        bitangent,
        unit_vector(vec3(-height_change_u, -height_change_v, 1.0)));
}

inline vec3 decode_tangent_space_normal(
    const color& encoded,
    double strength = 1.0,
    bool flip_green = false) {
    const double green_sign = flip_green ? -1.0 : 1.0;
    const vec3 decoded(
        strength * (2.0 * encoded.x() - 1.0),
        strength * green_sign * (2.0 * encoded.y() - 1.0),
        2.0 * encoded.z() - 1.0);
    return decoded.near_zero() ? vec3(0, 0, 1) : unit_vector(decoded);
}

class normal_mapped_lambertian : public material {
  public:
    normal_mapped_lambertian(
        shared_ptr<texture> albedo,
        shared_ptr<texture> normal_map,
        double strength = 1.0,
        bool flip_green = false)
      : albedo(std::move(albedo)),
        normal_map(std::move(normal_map)),
        strength(strength),
        flip_green(flip_green) {}

    bool scatter(
        const ray& r_in, const hit_record& rec, scatter_record& srec
    ) const override {
        (void)r_in;
        const vec3 shading_normal = mapped_normal(rec);
        srec.attenuation = albedo->value(rec.u, rec.v, rec.p);
        srec.pdf_ptr = make_shared<cosine_pdf>(shading_normal);
        srec.skip_pdf = false;
        return true;
    }

    double scattering_pdf(
        const ray& r_in, const hit_record& rec, const ray& scattered
    ) const override {
        (void)r_in;
        const double cos_theta = dot(
            mapped_normal(rec), unit_vector(scattered.direction()));
        return cos_theta < 0 ? 0 : cos_theta / pi;
    }

    vec3 mapped_normal(const hit_record& rec) const {
        if (!rec.has_tangent_space || !normal_map)
            return rec.normal;

        const vec3 tangent_normal = decode_tangent_space_normal(
            normal_map->value(rec.u, rec.v, rec.p), strength, flip_green);
        return tangent_space_to_world_normal(
            rec.normal, rec.tangent, rec.bitangent, tangent_normal);
    }

  private:
    shared_ptr<texture> albedo;
    shared_ptr<texture> normal_map;
    double strength;
    bool flip_green;
};

class bump_lambertian : public material {
  public:
    bump_lambertian(
        shared_ptr<texture> albedo,
        shared_ptr<image_texture> height_map,
        double strength)
      : albedo(std::move(albedo)),
        height_map(std::move(height_map)),
        strength(strength) {}

    bool scatter(
        const ray& r_in, const hit_record& rec, scatter_record& srec
    ) const override {
        (void)r_in;
        const vec3 shading_normal = perturbed_normal(rec);
        srec.attenuation = albedo->value(rec.u, rec.v, rec.p);
        srec.pdf_ptr = make_shared<cosine_pdf>(shading_normal);
        srec.skip_pdf = false;
        return true;
    }

    double scattering_pdf(
        const ray& r_in, const hit_record& rec, const ray& scattered
    ) const override {
        (void)r_in;
        const double cos_theta = dot(
            perturbed_normal(rec), unit_vector(scattered.direction()));
        return cos_theta < 0 ? 0 : cos_theta / pi;
    }

    vec3 perturbed_normal(const hit_record& rec) const {
        if (!rec.has_tangent_space
            || !height_map
            || height_map->width() <= 1
            || height_map->height() <= 1) {
            return rec.normal;
        }

        const double du = 1.0 / height_map->width();
        const double dv = 1.0 / height_map->height();
        const double center = sample_height(rec.u, rec.v, rec.p);
        const double change_u = strength
            * (sample_height(rec.u + du, rec.v, rec.p) - center);
        const double change_v = strength
            * (sample_height(rec.u, rec.v + dv, rec.p) - center);
        return bump_normal_from_gradient(
            rec.normal, rec.tangent, rec.bitangent, change_u, change_v);
    }

  private:
    double sample_height(double u, double v, const point3& p) const {
        return texture_luminance(height_map->value(u, v, p));
    }

    shared_ptr<texture> albedo;
    shared_ptr<image_texture> height_map;
    double strength;
};

// 金属材质
class metal : public material {
  public:
    metal(const color& albedo, double fuzz) : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1) {}

    bool scatter(const ray& r_in, const hit_record& rec, scatter_record& srec)
    const override {
        vec3 reflected = reflect(r_in.direction(), rec.normal);
        reflected = unit_vector(reflected) + (fuzz * random_unit_vector());

        srec.attenuation=albedo;
        srec.pdf_ptr=nullptr;
        srec.skip_pdf=true;
        srec.skip_pdf_ray=ray(rec.p, reflected, r_in.time());
        
        return true;
    }

  private:
    color albedo;
    double fuzz;
};

// 可反射和折射的电介质材质
class dielectric : public material {
  public:
    dielectric(double refraction_index) : refraction_index(refraction_index) {}

    bool scatter(const ray& r_in, const hit_record& rec, scatter_record& srec)
    const override {
        srec.attenuation = color(1.0, 1.0, 1.0);
        srec.pdf_ptr=nullptr;
        srec.skip_pdf=true;
        double ri = rec.front_face ? (1.0/refraction_index) : refraction_index;

        vec3 unit_direction = unit_vector(r_in.direction());
        vec3 refracted = refract(unit_direction, rec.normal, ri);

        double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta*cos_theta);

        bool cannot_refract = ri * sin_theta > 1.0;
        vec3 direction;

        if (cannot_refract || reflectance(cos_theta, ri) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, ri);

        srec.skip_pdf_ray=ray(rec.p, direction, r_in.time());
        return true;
    }

  private:
    // 材料折射率与包围介质折射率之比。
    double refraction_index;

    static double reflectance(double cosine, double refraction_index) {
        // 使用 Schlick 反射率近似公式。
        auto r0 = (1 - refraction_index) / (1 + refraction_index);
        r0 = r0*r0;
        return r0 + (1-r0)*std::pow((1 - cosine),5);
    }
};

// 漫射自发光材质
class diffuse_light : public material {
  public:
    diffuse_light(shared_ptr<texture> tex) : tex(tex) {}
    diffuse_light(const color& emit) : tex(make_shared<solid_color>(emit)) {}

    color emitted(const ray& r_in, const hit_record& rec, double u, double v, const point3& p)
    const override {
        if (!rec.front_face)
            return color(0,0,0);
        return tex->value(u, v, p);
    }

  private:
    shared_ptr<texture> tex;
};

class henyey_greenstein : public material {
  public:
    henyey_greenstein(const color& albedo, double asymmetry)
      : tex(make_shared<solid_color>(albedo)), asymmetry(asymmetry) {
        validate_asymmetry();
    }

    henyey_greenstein(shared_ptr<texture> tex, double asymmetry)
      : tex(std::move(tex)), asymmetry(asymmetry) {
        validate_asymmetry();
    }

    bool scatter(const ray& r_in, const hit_record& rec, scatter_record& srec)
    const override {
        srec.attenuation = tex->value(rec.u, rec.v, rec.p);
        srec.pdf_ptr = make_shared<henyey_greenstein_pdf>(
            r_in.direction(), asymmetry);
        srec.skip_pdf = false;
        return true;
    }

    double scattering_pdf(const ray& r_in, const hit_record& rec, const ray& scattered)
    const override {
        if (r_in.direction().near_zero() || scattered.direction().near_zero())
            return 0.0;

        const double cosine_theta = dot(
            unit_vector(r_in.direction()),
            unit_vector(scattered.direction()));
        return henyey_greenstein_phase(cosine_theta, asymmetry);
    }

  private:
    void validate_asymmetry() const {
        if (!std::isfinite(asymmetry) || asymmetry <= -1.0 || asymmetry >= 1.0)
            throw std::invalid_argument(
                "Henyey-Greenstein asymmetry must be finite and in (-1, 1).");
    }

    shared_ptr<texture> tex;
    double asymmetry;
};

// 各向同性介质相位函数
class isotropic : public henyey_greenstein {
  public:
    isotropic(const color& albedo) : henyey_greenstein(albedo, 0.0) {}
    isotropic(shared_ptr<texture> tex)
      : henyey_greenstein(std::move(tex), 0.0) {}
};

#endif
