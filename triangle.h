// triangle.h
#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "rtweekend.h"
#include "hittable.h"

#include <algorithm>
#include <cmath>
#include <memory>

struct triangle_vertex {
    point3 position;
    vec3 normal;
    double u = 0;
    double v = 0;
    bool has_normal = false;
    bool has_texcoord = false;
};

class triangle : public hittable {
  public:
    triangle(
        const triangle_vertex& vertex0,
        const triangle_vertex& vertex1,
        const triangle_vertex& vertex2,
        std::shared_ptr<material> material)
      : vertices{vertex0, vertex1, vertex2}, mat(std::move(material)) {
        edge1 = vertices[1].position - vertices[0].position;
        edge2 = vertices[2].position - vertices[0].position;
        outward_normal = unit_vector(cross(edge1, edge2));

        const point3 minimum(
            std::min({vertices[0].position.x(), vertices[1].position.x(), vertices[2].position.x()}),
            std::min({vertices[0].position.y(), vertices[1].position.y(), vertices[2].position.y()}),
            std::min({vertices[0].position.z(), vertices[1].position.z(), vertices[2].position.z()}));
        const point3 maximum(
            std::max({vertices[0].position.x(), vertices[1].position.x(), vertices[2].position.x()}),
            std::max({vertices[0].position.y(), vertices[1].position.y(), vertices[2].position.y()}),
            std::max({vertices[0].position.z(), vertices[1].position.z(), vertices[2].position.z()}));
        bbox = aabb(minimum, maximum);

        has_vertex_normals = vertices[0].has_normal
            && vertices[1].has_normal
            && vertices[2].has_normal;
        has_vertex_texcoords = vertices[0].has_texcoord
            && vertices[1].has_texcoord
            && vertices[2].has_texcoord;

        if (has_vertex_texcoords) {
            const double du1 = vertices[1].u - vertices[0].u;
            const double dv1 = vertices[1].v - vertices[0].v;
            const double du2 = vertices[2].u - vertices[0].u;
            const double dv2 = vertices[2].v - vertices[0].v;
            const double determinant = du1 * dv2 - dv1 * du2;
            if (std::fabs(determinant) > 1e-12) {
                const double inverse_determinant = 1.0 / determinant;
                uv_tangent = inverse_determinant * (dv2 * edge1 - dv1 * edge2);
                uv_bitangent = inverse_determinant * (-du2 * edge1 + du1 * edge2);
                has_tangent_space = !uv_tangent.near_zero() && !uv_bitangent.near_zero();
            }
        }
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        // Möller–Trumbore 求交；对行列式取绝对值，使三角形两面均可命中。
        constexpr double parallel_epsilon = 1e-10;
        const vec3 pvec = cross(r.direction(), edge2);
        const double determinant = dot(edge1, pvec);
        if (std::fabs(determinant) < parallel_epsilon)
            return false;

        const double inverse_determinant = 1.0 / determinant;
        const vec3 tvec = r.origin() - vertices[0].position;
        const double barycentric1 = dot(tvec, pvec) * inverse_determinant;
        if (barycentric1 < 0 || barycentric1 > 1)
            return false;

        const vec3 qvec = cross(tvec, edge1);
        const double barycentric2 = dot(r.direction(), qvec) * inverse_determinant;
        if (barycentric2 < 0 || barycentric1 + barycentric2 > 1)
            return false;

        const double t = dot(edge2, qvec) * inverse_determinant;
        if (!ray_t.surrounds(t))
            return false;

        const double barycentric0 = 1.0 - barycentric1 - barycentric2;
        rec.t = t;
        rec.p = r.at(t);
        rec.mat = mat;
        rec.has_tangent_space = false;

        // 使用几何法线判断正反面，并将插值着色法线调整到同一半球。
        rec.set_face_normal(r, outward_normal);
        if (has_vertex_normals) {
            vec3 shading_normal =
                barycentric0 * vertices[0].normal
                + barycentric1 * vertices[1].normal
                + barycentric2 * vertices[2].normal;
            if (!shading_normal.near_zero()) {
                shading_normal = unit_vector(shading_normal);
                if (dot(shading_normal, outward_normal) < 0)
                    shading_normal = -shading_normal;
                rec.normal = rec.front_face ? shading_normal : -shading_normal;
            }
        }

        if (has_vertex_texcoords) {
            rec.u = barycentric0 * vertices[0].u
                + barycentric1 * vertices[1].u
                + barycentric2 * vertices[2].u;
            rec.v = barycentric0 * vertices[0].v
                + barycentric1 * vertices[1].v
                + barycentric2 * vertices[2].v;
        } else {
            rec.u = barycentric1;
            rec.v = barycentric2;
        }

        if (has_tangent_space) {
            vec3 tangent = uv_tangent - dot(uv_tangent, rec.normal) * rec.normal;
            if (!tangent.near_zero()) {
                tangent = unit_vector(tangent);
                vec3 bitangent = unit_vector(cross(rec.normal, tangent));
                const vec3 oriented_uv_bitangent = rec.front_face ? uv_bitangent : -uv_bitangent;
                if (dot(bitangent, oriented_uv_bitangent) < 0)
                    bitangent = -bitangent;

                rec.tangent = tangent;
                rec.bitangent = bitangent;
                rec.has_tangent_space = true;
            }
        }

        return true;
    }

    aabb bounding_box() const override { return bbox; }

  private:
    triangle_vertex vertices[3];
    vec3 edge1;
    vec3 edge2;
    vec3 outward_normal;
    vec3 uv_tangent;
    vec3 uv_bitangent;
    std::shared_ptr<material> mat;
    aabb bbox;
    bool has_vertex_normals = false;
    bool has_vertex_texcoords = false;
    bool has_tangent_space = false;
};

#endif
