// aabb.h
#ifndef AABB_H
#define AABB_H

#include "rtweekend.h"
class aabb {
  public:
    interval x, y, z;

    aabb() {} // 默认 AABB 为空，因为区间默认为空

    aabb(const interval& x, const interval& y, const interval& z)
      : x(x), y(y), z(z)
    {
        pad_to_minimums();
    }

    aabb(const point3& a, const point3& b) {
        // 将 a 和 b 两点视为边界框的极值，因此我们不需要特定的最小/最大坐标顺序

        x = (a[0] <= b[0]) ? interval(a[0], b[0]) : interval(b[0], a[0]);
        y = (a[1] <= b[1]) ? interval(a[1], b[1]) : interval(b[1], a[1]);
        z = (a[2] <= b[2]) ? interval(a[2], b[2]) : interval(b[2], a[2]);

        pad_to_minimums();
    }

    aabb(const aabb& box0, const aabb& box1) {
        x = interval(box0.x, box1.x);
        y = interval(box0.y, box1.y);
        z = interval(box0.z, box1.z);
    }

    const interval& axis_interval(int n) const {
        if (n == 1) return y;
        if (n == 2) return z;
        return x;
    }

    double surface_area() const {
        const double dx = x.size();
        const double dy = y.size();
        const double dz = z.size();
        if (dx < 0 || dy < 0 || dz < 0)
            return 0;
        return 2.0 * (dx * dy + dx * dz + dy * dz);
    }

    point3 centroid() const {
        return point3(
            0.5 * (x.min + x.max),
            0.5 * (y.min + y.max),
            0.5 * (z.min + z.max));
    }

    bool hit(const ray& r, interval ray_t) const {
        const point3& ray_orig = r.origin();
        const vec3&   ray_dir  = r.direction();

        for (int axis = 0; axis < 3; axis++) {
            const interval& ax = axis_interval(axis);
            const double adinv = 1.0 / ray_dir[axis];

            auto t0 = (ax.min - ray_orig[axis]) * adinv;
            auto t1 = (ax.max - ray_orig[axis]) * adinv;

            if (t0 < t1) {
                if (t0 > ray_t.min) ray_t.min = t0;
                if (t1 < ray_t.max) ray_t.max = t1;
            } else {
                if (t1 > ray_t.min) ray_t.min = t1;
                if (t0 < ray_t.max) ray_t.max = t0;
            }

            if (ray_t.max <= ray_t.min)
                return false;
        }
        return true;
    }

    private:

    void pad_to_minimums() {
        // 扩展过窄的轴向区间，避免退化包围盒导致求交失败。

        double delta = 0.0001;
        if (x.size() < delta) x = x.expand(delta);
        if (y.size() < delta) y = y.expand(delta);
        if (z.size() < delta) z = z.expand(delta);
    }


};

inline aabb operator+(const aabb& bbox, const vec3& offset) {
    return aabb(bbox.x + offset.x(), bbox.y + offset.y(), bbox.z + offset.z());
}

inline aabb operator+(const vec3& offset, const aabb& bbox) {
    return bbox + offset;
}







#endif
