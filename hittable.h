// hittable.h
#ifndef HITTABLE_H
#define HITTABLE_H

#include "ray.h"
#include "interval.h"
#include "aabb.h"
#include <memory>

class material;

class hit_record
{
public:
  point3 p;
  vec3 normal;
  vec3 tangent;
  vec3 bitangent;
  std::shared_ptr<material> mat;
  double t = 0;
  double u = 0;
  double v = 0;
  bool front_face = false;
  bool has_tangent_space = false;

  void set_face_normal(const ray &r, const vec3 &outward_normal)
  {
    // 设置命中记录的法向量
    // 注意：outward_normal 必须是单位向量。

    front_face = dot(r.direction(), outward_normal) < 0;
    normal = front_face ? outward_normal : -outward_normal;
  }
};

class hittable
{
public:
  virtual ~hittable() = default;

  virtual bool hit(const ray &r, interval ray_t, hit_record &rec) const = 0;

  virtual aabb bounding_box() const = 0;

  virtual double pdf_value(const point3& origin, const vec3& direction) const {
        return 0.0;
    }

    virtual vec3 random(const point3& origin) const {
        return vec3(1,0,0);
    }
};

class translate : public hittable
{
public:
  translate(shared_ptr<hittable> object, const vec3 &offset)
      : object(object), offset(offset)
  {
    bbox = object->bounding_box() + offset;
  }

  bool hit(const ray &r, interval ray_t, hit_record &rec) const override
  {
    // 将光线向后移动偏移量
    ray offset_r(r.origin() - offset, r.direction(), r.time());

    // 确定沿偏移发生相交的位置（如果有）
    if (!object->hit(offset_r, ray_t, rec))
      return false;

    // 将交叉点向前移动偏移量
    rec.p += offset;

    return true;
  }

  aabb bounding_box() const override { return bbox; }

private:
  shared_ptr<hittable> object;
  vec3 offset;
  aabb bbox;
};

class rotate_y : public hittable
{
public:
  rotate_y(shared_ptr<hittable> object, double angle) : object(object)
  {
    auto radians = degrees_to_radians(angle);
    sin_theta = std::sin(radians);
    cos_theta = std::cos(radians);
    bbox = object->bounding_box();

    point3 min(infinity, infinity, infinity);
    point3 max(-infinity, -infinity, -infinity);

    for (int i = 0; i < 2; i++)
    {
      for (int j = 0; j < 2; j++)
      {
        for (int k = 0; k < 2; k++)
        {
          auto x = i * bbox.x.max + (1 - i) * bbox.x.min;
          auto y = j * bbox.y.max + (1 - j) * bbox.y.min;
          auto z = k * bbox.z.max + (1 - k) * bbox.z.min;

          auto newx = cos_theta * x + sin_theta * z;
          auto newz = -sin_theta * x + cos_theta * z;

          vec3 tester(newx, y, newz);

          for (int c = 0; c < 3; c++)
          {
            min[c] = std::fmin(min[c], tester[c]);
            max[c] = std::fmax(max[c], tester[c]);
          }
        }
      }
    }

    bbox = aabb(min, max);
  }

  bool hit(const ray &r, interval ray_t, hit_record &rec) const override
  {
    // 将光线从世界空间更改为对象空间
    auto origin = point3(
        (cos_theta * r.origin().x()) - (sin_theta * r.origin().z()),
        r.origin().y(),
        (sin_theta * r.origin().x()) + (cos_theta * r.origin().z()));

    auto direction = vec3(
        (cos_theta * r.direction().x()) - (sin_theta * r.direction().z()),
        r.direction().y(),
        (sin_theta * r.direction().x()) + (cos_theta * r.direction().z()));

    ray rotated_r(origin, direction, r.time());

    // 确定对象空间中发生相交的位置（如果有）
    if (!object->hit(rotated_r, ray_t, rec))
      return false;

    // 将交点从对象空间更改为世界空间
    rec.p = point3(
        (cos_theta * rec.p.x()) + (sin_theta * rec.p.z()),
        rec.p.y(),
        (-sin_theta * rec.p.x()) + (cos_theta * rec.p.z()));
    
    // 将法线从对象空间转换到世界空间。
    rec.normal = vec3(
        (cos_theta * rec.normal.x()) + (sin_theta * rec.normal.z()),
        rec.normal.y(),
        (-sin_theta * rec.normal.x()) + (cos_theta * rec.normal.z()));

    if (rec.has_tangent_space)
    {
      rec.tangent = vec3(
          (cos_theta * rec.tangent.x()) + (sin_theta * rec.tangent.z()),
          rec.tangent.y(),
          (-sin_theta * rec.tangent.x()) + (cos_theta * rec.tangent.z()));
      rec.bitangent = vec3(
          (cos_theta * rec.bitangent.x()) + (sin_theta * rec.bitangent.z()),
          rec.bitangent.y(),
          (-sin_theta * rec.bitangent.x()) + (cos_theta * rec.bitangent.z()));
    }

    return true;
  }

  aabb bounding_box() const override { return bbox; }

private:
  shared_ptr<hittable> object;
  double sin_theta;
  double cos_theta;
  aabb bbox;
};

#endif
