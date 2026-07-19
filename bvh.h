// bvh.h
#ifndef BVH_H
#define BVH_H

#include "rtweekend.h"
#include "hittable.h"
#include "hittable_list.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

enum class bvh_split_method {
    median,
    sah
};

inline const char* bvh_split_method_name(bvh_split_method method) {
    return method == bvh_split_method::sah ? "sah" : "median";
}

struct bvh_build_stats {
    std::size_t node_count = 0;
    std::size_t leaf_count = 0;
    std::size_t max_depth = 0;
    std::size_t sah_split_count = 0;
    std::size_t median_split_count = 0;
};

class bvh_node : public hittable
{
public:
    explicit bvh_node(
        const hittable_list& list,
        bvh_split_method method = bvh_split_method::sah)
      : split_method(method) {
        if (list.objects.empty())
            throw std::invalid_argument("Cannot build a BVH from an empty object list.");

        // 只复制一次指针数组，递归节点仅划分各自负责的区间。
        auto objects = list.objects;
        build(objects, 0, objects.size());
    }

    bool hit(const ray &r, interval ray_t, hit_record &rec) const override
    {
        if (!bbox.hit(r, ray_t))
            return false;

        const bool hit_left = left->hit(r, ray_t, rec);
        const bool hit_right = right && right->hit(
            r, interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec);

        return hit_left || hit_right;
    }

    aabb bounding_box() const override { return bbox; }
    const bvh_build_stats& statistics() const { return stats; }
    bvh_split_method method() const { return split_method; }

private:
    static constexpr int sah_bin_count = 16;

    struct bin_data {
        aabb bbox;
        std::size_t count = 0;
        bool has_bounds = false;

        void add(const aabb& object_bbox) {
            bbox = has_bounds ? aabb(bbox, object_bbox) : object_bbox;
            has_bounds = true;
            ++count;
        }
    };

    struct sah_split {
        int axis = 0;
        int bin = 0;
        double centroid_min = 0;
        double centroid_max = 0;
        double cost = std::numeric_limits<double>::infinity();
        bool valid = false;
    };

    bvh_node(
        std::vector<shared_ptr<hittable>>& objects,
        std::size_t start,
        std::size_t end,
        bvh_split_method method)
      : split_method(method) {
        build(objects, start, end);
    }

    static double object_centroid(
        const shared_ptr<hittable>& object,
        int axis) {
        return object->bounding_box().centroid()[axis];
    }

    static int centroid_bin(double centroid, double minimum, double maximum) {
        const double normalized = (centroid - minimum) / (maximum - minimum);
        const int bin = static_cast<int>(normalized * sah_bin_count);
        return std::clamp(bin, 0, sah_bin_count - 1);
    }

    static int longest_centroid_axis(
        const std::vector<shared_ptr<hittable>>& objects,
        std::size_t start,
        std::size_t end) {
        point3 minimum(infinity, infinity, infinity);
        point3 maximum(-infinity, -infinity, -infinity);
        for (std::size_t index = start; index < end; ++index) {
            const point3 center = objects[index]->bounding_box().centroid();
            for (int axis = 0; axis < 3; ++axis) {
                minimum[axis] = std::min(minimum[axis], center[axis]);
                maximum[axis] = std::max(maximum[axis], center[axis]);
            }
        }

        const vec3 extent = maximum - minimum;
        if (extent.y() > extent.x() && extent.y() >= extent.z())
            return 1;
        if (extent.z() > extent.x() && extent.z() > extent.y())
            return 2;
        return 0;
    }

    static std::size_t median_partition(
        std::vector<shared_ptr<hittable>>& objects,
        std::size_t start,
        std::size_t end) {
        const int axis = longest_centroid_axis(objects, start, end);
        const std::size_t middle = start + (end - start) / 2;
        std::nth_element(
            objects.begin() + start,
            objects.begin() + middle,
            objects.begin() + end,
            [axis](const auto& left_object, const auto& right_object) {
                return object_centroid(left_object, axis)
                    < object_centroid(right_object, axis);
            });
        return middle;
    }

    static sah_split find_sah_split(
        const std::vector<shared_ptr<hittable>>& objects,
        std::size_t start,
        std::size_t end) {
        sah_split best;

        for (int axis = 0; axis < 3; ++axis) {
            double centroid_minimum = infinity;
            double centroid_maximum = -infinity;
            for (std::size_t index = start; index < end; ++index) {
                const double center = object_centroid(objects[index], axis);
                centroid_minimum = std::min(centroid_minimum, center);
                centroid_maximum = std::max(centroid_maximum, center);
            }

            if (!(centroid_maximum > centroid_minimum))
                continue;

            std::array<bin_data, sah_bin_count> bins;
            for (std::size_t index = start; index < end; ++index) {
                const auto& object = objects[index];
                const int bin = centroid_bin(
                    object_centroid(object, axis), centroid_minimum, centroid_maximum);
                bins[bin].add(object->bounding_box());
            }

            std::array<aabb, sah_bin_count> left_bounds;
            std::array<aabb, sah_bin_count> right_bounds;
            std::array<std::size_t, sah_bin_count> left_counts{};
            std::array<std::size_t, sah_bin_count> right_counts{};

            aabb running_bounds;
            bool has_running_bounds = false;
            std::size_t running_count = 0;
            for (int bin = 0; bin < sah_bin_count; ++bin) {
                if (bins[bin].count > 0) {
                    running_bounds = has_running_bounds
                        ? aabb(running_bounds, bins[bin].bbox)
                        : bins[bin].bbox;
                    has_running_bounds = true;
                    running_count += bins[bin].count;
                }
                left_bounds[bin] = running_bounds;
                left_counts[bin] = running_count;
            }

            running_bounds = aabb();
            has_running_bounds = false;
            running_count = 0;
            for (int bin = sah_bin_count - 1; bin >= 0; --bin) {
                if (bins[bin].count > 0) {
                    running_bounds = has_running_bounds
                        ? aabb(running_bounds, bins[bin].bbox)
                        : bins[bin].bbox;
                    has_running_bounds = true;
                    running_count += bins[bin].count;
                }
                right_bounds[bin] = running_bounds;
                right_counts[bin] = running_count;
            }

            for (int bin = 0; bin + 1 < sah_bin_count; ++bin) {
                const std::size_t left_count = left_counts[bin];
                const std::size_t right_count = right_counts[bin + 1];
                if (left_count == 0 || right_count == 0)
                    continue;

                const double cost =
                    left_bounds[bin].surface_area() * static_cast<double>(left_count)
                    + right_bounds[bin + 1].surface_area() * static_cast<double>(right_count);
                if (cost < best.cost) {
                    best.axis = axis;
                    best.bin = bin;
                    best.centroid_min = centroid_minimum;
                    best.centroid_max = centroid_maximum;
                    best.cost = cost;
                    best.valid = true;
                }
            }
        }

        return best;
    }

    static std::size_t apply_sah_partition(
        std::vector<shared_ptr<hittable>>& objects,
        std::size_t start,
        std::size_t end,
        const sah_split& split) {
        const auto middle = std::partition(
            objects.begin() + start,
            objects.begin() + end,
            [&split](const auto& object) {
                return centroid_bin(
                    object_centroid(object, split.axis),
                    split.centroid_min,
                    split.centroid_max) <= split.bin;
            });
        return static_cast<std::size_t>(middle - objects.begin());
    }

    void build(
        std::vector<shared_ptr<hittable>>& objects,
        std::size_t start,
        std::size_t end) {
        const std::size_t object_span = end - start;
        stats.node_count = 1;
        stats.leaf_count = object_span;
        stats.max_depth = 1;

        if (object_span == 1) {
            left = objects[start];
            right = nullptr;
        } else if (object_span == 2) {
            const int axis = longest_centroid_axis(objects, start, end);
            if (object_centroid(objects[start], axis)
                <= object_centroid(objects[start + 1], axis)) {
                left = objects[start];
                right = objects[start + 1];
            } else {
                left = objects[start + 1];
                right = objects[start];
            }
        } else {
            std::size_t middle = start;
            bool used_sah = false;
            if (split_method == bvh_split_method::sah) {
                const sah_split split = find_sah_split(objects, start, end);
                if (split.valid) {
                    middle = apply_sah_partition(objects, start, end, split);
                    used_sah = middle > start && middle < end;
                }
            }

            if (!used_sah)
                middle = median_partition(objects, start, end);

            auto left_node = shared_ptr<bvh_node>(
                new bvh_node(objects, start, middle, split_method));
            auto right_node = shared_ptr<bvh_node>(
                new bvh_node(objects, middle, end, split_method));
            left = left_node;
            right = right_node;

            const auto& left_stats = left_node->statistics();
            const auto& right_stats = right_node->statistics();
            stats.node_count += left_stats.node_count + right_stats.node_count;
            stats.max_depth = 1 + std::max(left_stats.max_depth, right_stats.max_depth);
            stats.sah_split_count = left_stats.sah_split_count
                + right_stats.sah_split_count + (used_sah ? 1 : 0);
            stats.median_split_count = left_stats.median_split_count
                + right_stats.median_split_count + (used_sah ? 0 : 1);
        }

        bbox = right
            ? aabb(left->bounding_box(), right->bounding_box())
            : left->bounding_box();
    }

    shared_ptr<hittable> left;
    shared_ptr<hittable> right;
    aabb bbox;
    bvh_split_method split_method;
    bvh_build_stats stats;
};

#endif
