// displacement.h
#ifndef DISPLACEMENT_H
#define DISPLACEMENT_H

#include "texture.h"

#include <memory>
#include <utility>

class displacement_map {
  public:
    displacement_map() = default;

    displacement_map(
        std::shared_ptr<texture> height_map,
        double distance,
        double midpoint = 0.5)
      : height_map(std::move(height_map)),
        distance(distance),
        midpoint(midpoint) {}

    bool enabled() const {
        return height_map && distance != 0;
    }

    point3 displace(
        const point3& position,
        const vec3& normal,
        double u,
        double v) const {
        if (!enabled() || normal.near_zero())
            return position;

        const double height = texture_luminance(
            height_map->value(u, v, position));
        return position
            + distance * (height - midpoint) * unit_vector(normal);
    }

    double distance_scale() const { return distance; }

  private:
    std::shared_ptr<texture> height_map;
    double distance = 0;
    double midpoint = 0.5;
};

#endif
