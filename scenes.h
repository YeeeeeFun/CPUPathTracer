// scenes.h
#ifndef SCENES_H
#define SCENES_H

#include "camera.h"
#include "bvh.h"
#include "constant_medium.h"
#include "hittable_list.h"
#include "material.h"
#include "obj_instance.h"
#include "quad.h"
#include "sphere.h"
#include "texture.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

struct render_scene {
    std::string name;
    hittable_list world;
    hittable_list lights;
    camera cam;

    void build_world_bvh(bvh_split_method method = bvh_split_method::sah) {
        const auto build_start = std::chrono::steady_clock::now();
        world_acceleration = std::make_shared<bvh_node>(world, method);
        const auto build_end = std::chrono::steady_clock::now();
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            build_end - build_start).count();
        const auto& stats = world_acceleration->statistics();
        std::clog << "Top-level BVH: " << bvh_split_method_name(method)
                  << ", " << world.objects.size() << " objects"
                  << ", " << milliseconds << " ms"
                  << ", " << stats.node_count << " nodes"
                  << ", depth " << stats.max_depth
                  << ", " << stats.sah_split_count << " SAH splits"
                  << ", " << stats.median_split_count << " median fallbacks\n";
    }

    const hittable& render_world() const {
        return world_acceleration ? static_cast<const hittable&>(*world_acceleration)
                                  : static_cast<const hittable&>(world);
    }

  private:
    std::shared_ptr<bvh_node> world_acceleration;
};

struct showcase_scene_settings {
    bool enable_fog = true;
    double fog_density = .00010;
};

struct final_showcase_settings {
    bool enable_global_fog = false;
    double global_fog_density = .00008;
    bool enable_ceiling_light = true;
    double ceiling_light_intensity = 3.0;
    double sphere_light_intensity = 1.0;
};

inline render_scene make_cornell_scene() {
    render_scene scene;
    scene.name = "cornell";

    auto red = make_shared<lambertian>(color(.65, .05, .05));
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    auto green = make_shared<lambertian>(color(.12, .45, .15));
    auto light = make_shared<diffuse_light>(color(15, 15, 15));

    scene.world.add(make_shared<quad>(
        point3(555, 0, 0), vec3(0, 0, 555), vec3(0, 555, 0), green));
    scene.world.add(make_shared<quad>(
        point3(0, 0, 555), vec3(0, 0, -555), vec3(0, 555, 0), red));
    scene.world.add(make_shared<quad>(
        point3(0, 555, 0), vec3(555, 0, 0), vec3(0, 0, 555), white));
    scene.world.add(make_shared<quad>(
        point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 0, -555), white));
    scene.world.add(make_shared<quad>(
        point3(555, 0, 555), vec3(-555, 0, 0), vec3(0, 555, 0), white));

    scene.world.add(make_shared<quad>(
        point3(213, 554, 227), vec3(130, 0, 0), vec3(0, 0, 105), light));

    auto box1 = apply_object_transform(
        box(point3(0, 0, 0), point3(165, 330, 165), white),
        object_transform{15, vec3(265, 0, 295)});
    scene.world.add(box1);

    auto glass = make_shared<dielectric>(1.5);
    scene.world.add(make_shared<sphere>(point3(190, 90, 190), 90, glass));

    auto empty_material = shared_ptr<material>();
    scene.lights.add(make_shared<quad>(
        point3(343, 554, 332), vec3(-130, 0, 0), vec3(0, 0, -105), empty_material));
    scene.lights.add(make_shared<sphere>(
        point3(190, 90, 190), 90, empty_material));

    scene.cam.lookfrom = point3(278, 278, -800);
    scene.cam.lookat = point3(278, 278, 0);

    return scene;
}

inline render_scene make_showcase_scene(
    bvh_split_method obj_bvh_method = bvh_split_method::sah,
    const showcase_scene_settings& settings = {}) {
    render_scene scene;
    scene.name = "showcase";

    auto ground = make_shared<lambertian>(color(.28, .30, .32));
    auto light = make_shared<diffuse_light>(color(12, 12, 12));
    auto glass = make_shared<dielectric>(1.5);
    auto polished_metal = make_shared<metal>(color(.82, .85, .90), .03);
    auto box_diffuse = make_shared<lambertian>(color(.32, .12, .55));
    auto marble = make_shared<lambertian>(make_shared<noise_texture>(.035));

    const auto earth_path = (
        std::filesystem::path(RT_PROJECT_SOURCE_DIR) / "earthmap.jpg").string();
    auto earth = make_shared<lambertian>(
        make_shared<image_texture>(earth_path.c_str()));

    const auto spot_directory = (
        std::filesystem::path(RT_PROJECT_SOURCE_DIR) / "models" / "spot");
    const auto spot_texture_path = (spot_directory / "spot_texture.png").string();
    const auto spot_height_path = (spot_directory / "hmap.jpg").string();
    constexpr double spot_bump_strength = 2.0;
    constexpr double spot_displacement_scale = 8.0;
    constexpr bool enable_spot_displacement = false;
    auto spot_height_map = make_shared<image_texture>(
        spot_height_path.c_str(), image_color_space::linear);
    auto spot_material = make_shared<bump_lambertian>(
        make_shared<image_texture>(spot_texture_path.c_str()),
        spot_height_map,
        spot_bump_strength);

    const auto african_head_directory = (
        std::filesystem::path(RT_PROJECT_SOURCE_DIR) / "models" / "african_head");
    const auto african_head_diffuse_path = (
        african_head_directory / "african_head_diffuse.tga").string();
    const auto african_head_normal_path = (
        african_head_directory / "african_head_nm_tangent.tga").string();
    constexpr bool enable_african_head_normal_map = true;
    constexpr double african_head_normal_strength = 1.0;
    constexpr bool african_head_flip_normal_green = false;
    auto african_head_albedo = make_shared<image_texture>(
        african_head_diffuse_path.c_str());
    shared_ptr<material> african_head_material;
    if (enable_african_head_normal_map) {
        african_head_material = make_shared<normal_mapped_lambertian>(
            african_head_albedo,
            make_shared<image_texture>(
                african_head_normal_path.c_str(), image_color_space::linear),
            african_head_normal_strength,
            african_head_flip_normal_green);
    } else {
        african_head_material = make_shared<lambertian>(african_head_albedo);
    }

    scene.world.add(make_shared<quad>(
        point3(-350, 0, 900), vec3(1250, 0, 0), vec3(0, 0, -1500), ground));

    scene.world.add(make_shared<quad>(
        point3(70, 650, 100), vec3(500, 0, 0), vec3(0, 0, 360), light));

    auto empty_material = shared_ptr<material>();
    scene.lights.add(make_shared<quad>(
        point3(70, 650, 100), vec3(500, 0, 0), vec3(0, 0, 360), empty_material));

    scene.world.add(make_shared<sphere>(point3(70, 165, 300), 105, earth));
    scene.world.add(make_shared<sphere>(point3(315, 135, 135), 82, glass));
    scene.world.add(make_shared<sphere>(point3(520, 190, 360), 118, polished_metal));
    scene.world.add(make_shared<sphere>(point3(275, 365, 430), 88, marble));

    const auto african_head_path = african_head_directory / "african_head.obj";
    obj_instance_config african_head_config(
        african_head_path, african_head_material);
    african_head_config.uniform_scale = 120.0;
    african_head_config.transform = {180, vec3(35, 390, 555)};
    scene.world.add(make_obj_instance(african_head_config, obj_bvh_method));

    auto bunny_material = make_shared<lambertian>(color(.12, .32, .72));
    const auto bunny_path = (
        std::filesystem::path(RT_PROJECT_SOURCE_DIR) / "models" / "bunny" / "bunny.obj");
    obj_instance_config bunny_config(bunny_path, bunny_material);
    bunny_config.uniform_scale = 1200.0;
    bunny_config.transform = {180, vec3(-90, 330, 370)};
    bunny_config.missing_normal_mode = obj_missing_normal_mode::smooth;
    scene.world.add(make_obj_instance(bunny_config, obj_bvh_method));

    const auto spot_path = spot_directory / "spot_triangulated_good.obj";
    const auto spot_displacement = enable_spot_displacement
        ? displacement_map(spot_height_map, spot_displacement_scale)
        : displacement_map();
    obj_instance_config spot_config(spot_path, spot_material);
    spot_config.uniform_scale = 145.0;
    spot_config.transform = {-25, vec3(520, 385, 160)};
    spot_config.displacement = spot_displacement;
    scene.world.add(make_obj_instance(spot_config, obj_bvh_method));

    const auto crate_directory = (
        std::filesystem::path(RT_PROJECT_SOURCE_DIR) / "models" / "Crate");
    auto crate_fallback = make_shared<lambertian>(color(.64, .64, .64));
    obj_instance_config crate_config(
        crate_directory / "Crate1.obj", crate_fallback);
    crate_config.uniform_scale = 70.0;
    crate_config.transform = {-18, vec3(650, 75, 600)};
    crate_config.missing_normal_mode = obj_missing_normal_mode::flat;
    crate_config.material_mode = obj_material_mode::mtl;
    scene.world.add(make_obj_instance(crate_config, obj_bvh_method));

    auto floating_box = apply_object_transform(
        box(point3(0, 0, 0), point3(145, 160, 145), box_diffuse),
        object_transform{-24, vec3(360, 225, 560)});
    scene.world.add(floating_box);

    constexpr double fog_anisotropy = .35;
    const color fog_scattering_albedo(.70, .78, .90);
    if (settings.enable_fog) {
        auto fog_boundary = box(
            point3(-1200, -200, -1200), point3(1500, 1200, 1500), nullptr);
        scene.world.add(make_shared<constant_medium>(
            fog_boundary,
            settings.fog_density,
            fog_scattering_albedo,
            fog_anisotropy));
        std::clog << "Participating medium: homogeneous fog, density "
                  << settings.fog_density << ", HG g=" << fog_anisotropy << ".\n";
    } else {
        std::clog << "Participating medium: disabled.\n";
    }

    scene.cam.lookfrom = point3(700, 330, -950);
    scene.cam.lookat = point3(275, 245, 285);

    return scene;
}

namespace final_showcase_detail {

inline shared_ptr<image_texture> image(
    const std::filesystem::path& path,
    image_color_space color_space = image_color_space::srgb) {
    return make_shared<image_texture>(path.string().c_str(), color_space);
}

inline void add_obj(
    render_scene& scene,
    const std::filesystem::path& path,
    shared_ptr<material> material,
    double scale,
    const vec3& translation,
    double rotation,
    bvh_split_method split_method,
    obj_missing_normal_mode normal_mode = obj_missing_normal_mode::smoothing_groups,
    const displacement_map& displacement = displacement_map(),
    obj_material_mode material_mode = obj_material_mode::supplied) {
    obj_instance_config config(path, std::move(material));
    config.uniform_scale = scale;
    config.transform = {rotation, translation};
    config.missing_normal_mode = normal_mode;
    config.displacement = displacement;
    config.material_mode = material_mode;
    scene.world.add(make_obj_instance(config, split_method));
}

inline void add_sphere_light(
    render_scene& scene,
    const point3& center,
    double radius,
    const color& emission) {
    scene.world.add(make_shared<sphere>(
        center, radius, make_shared<diffuse_light>(emission)));
    scene.lights.add(make_shared<sphere>(
        center, radius, shared_ptr<material>()));
}

struct scene_placement {
    double x = 0;
    double z = 0;
    double rotation = 0;
    double hover = 0;
};

class deterministic_layout {
  public:
    explicit deterministic_layout(std::uint64_t seed) : generator(seed) {}

    scene_placement place(
        double footprint_radius,
        double min_x,
        double max_x,
        double min_z,
        double max_z,
        double hover_probability = .36,
        double min_hover = 90,
        double max_hover = 380) {
        for (int attempt = 0; attempt < 1000; ++attempt) {
            scene_placement candidate;
            candidate.x = uniform(min_x, max_x);
            candidate.z = uniform(min_z, max_z);
            // 仅在正面朝向范围内变化，不做无限制旋转；各模型再校正自身正面轴。
            candidate.rotation = uniform(-70, 70);
            if (!overlaps(candidate.x, candidate.z, footprint_radius)) {
                const double hover_roll = uniform(0, 1);
                const double hover_height = uniform(min_hover, max_hover);
                if (hover_roll < hover_probability) {
                    candidate.hover = hover_height;
                    ++hovering_count;
                }
                occupied.push_back({candidate.x, candidate.z, footprint_radius});
                ++placement_count;
                return candidate;
            }
        }
        std::clog << "Final layout placement failed: footprint "
                  << footprint_radius << ", x [" << min_x << ", " << max_x
                  << "], z [" << min_z << ", " << max_z << "].\n";
        throw std::runtime_error("Unable to place final-scene object without overlap.");
    }

    double uniform(double minimum, double maximum) {
        return std::uniform_real_distribution<double>(minimum, maximum)(generator);
    }

    void reserve(double x, double z, double footprint_radius) {
        occupied.push_back({x, z, footprint_radius});
    }

    int placements() const { return placement_count; }
    int hovering() const { return hovering_count; }

  private:
    struct occupied_disc {
        double x;
        double z;
        double radius;
    };

    bool overlaps(double x, double z, double radius) const {
        constexpr double clearance = 16.0;
        for (const auto& object : occupied) {
            const double dx = x - object.x;
            const double dz = z - object.z;
            const double minimum_distance = radius + object.radius + clearance;
            if (dx * dx + dz * dz < minimum_distance * minimum_distance)
                return true;
        }
        return false;
    }

    std::mt19937_64 generator;
    std::vector<occupied_disc> occupied;
    int placement_count = 0;
    int hovering_count = 0;
};

inline double distance_scale(double base_scale, double z) {
    return base_scale * (1.0 + .00016 * std::max(0.0, z));
}

inline double front_facing_rotation(
    double source_front_yaw,
    double variation) {
    double rotation = source_front_yaw + variation;
    while (rotation > 180)
        rotation -= 360;
    while (rotation < -180)
        rotation += 360;
    return rotation;
}

inline void add_random_sphere_field(
    render_scene& scene,
    deterministic_layout& layout) {
    auto glass = make_shared<dielectric>(1.5);
    auto silver = make_shared<metal>(color(.82, .86, .92), .08);
    const color palette[] = {
        color(.65, .08, .06), color(.08, .28, .70),
        color(.12, .58, .22), color(.62, .18, .72),
        color(.78, .45, .08), color(.08, .55, .62)
    };

    for (int index = 0; index < 28; ++index) {
        const double base_radius = index == 3
            ? 85.0
            : 36.0 + 8.0 * (index % 5);
        const auto placement = index == 3
            ? scene_placement{-930, -20, 0, 0}
            : layout.place(95, -1750, 1750, -100, 5250, .36, 80, 330);
        const double radius = distance_scale(base_radius, placement.z);
        shared_ptr<material> material;
        if (index % 7 == 0)
            material = glass;
        else if (index % 4 == 0)
            material = silver;
        else
            material = make_shared<lambertian>(palette[index % 6]);
        scene.world.add(make_shared<sphere>(
            point3(
                placement.x,
                radius + placement.hover,
                placement.z),
            radius,
            material));
    }

    auto moving_material = make_shared<lambertian>(color(.92, .22, .08));
    const auto moving = layout.place(115, -1500, 1500, 200, 4400, .36, 100, 300);
    const double moving_radius = distance_scale(76, moving.z);
    scene.world.add(make_shared<sphere>(
        point3(moving.x, moving_radius + moving.hover, moving.z),
        point3(moving.x, moving_radius + moving.hover + 130, moving.z),
        moving_radius,
        moving_material));
}

} // 命名空间 final_showcase_detail

inline render_scene make_final_showcase_scene(
    bvh_split_method obj_bvh_method = bvh_split_method::sah,
    const final_showcase_settings& settings = {}) {
    using namespace final_showcase_detail;

    render_scene scene;
    scene.name = "final-showcase";

    const auto source_root = std::filesystem::path(RT_PROJECT_SOURCE_DIR);
    const auto models = source_root / "models";
    constexpr std::uint64_t layout_seed = 0x5070F1A1ULL;
    deterministic_layout layout(layout_seed);
    // 随机放置物体前，为两个前景镜面立方体预留空间。
    layout.reserve(-1320, 1650, 520);
    layout.reserve(350, 700, 500);
    // 随机放置前，为中景和前景的固定物体预留锚点。
    layout.reserve(-900, 500, 285);
    layout.reserve(-400, 950, 285);
    layout.reserve(850, 1450, 295);
    layout.reserve(-700, 1600, 300);
    layout.reserve(200, 1900, 300);
    layout.reserve(-1350, -80, 260);
    layout.reserve(-930, -20, 110);
    layout.reserve(-1500, 3300, 370);
    layout.reserve(1500, 3800, 370);

    auto ground_texture = make_shared<checker_texture>(
        210.0, color(.04, .22, .62), color(.05, .55, .34));
    auto ground = make_shared<lambertian>(ground_texture);
    scene.world.add(make_shared<quad>(
        point3(-3200, 0, -1500),
        vec3(6400, 0, 0),
        vec3(0, 0, 8000),
        ground));

    // 较弱的顶部光负责补光，四个大球形光源展示多光源贡献和软阴影。
    if (settings.enable_ceiling_light) {
        const color ceiling_emission(
            settings.ceiling_light_intensity,
            settings.ceiling_light_intensity,
            settings.ceiling_light_intensity);
        scene.world.add(make_shared<quad>(
            point3(-600, 1350, 1200),
            vec3(1200, 0, 0),
            vec3(0, 0, 1600),
            make_shared<diffuse_light>(ceiling_emission)));
        scene.lights.add(make_shared<quad>(
            point3(-600, 1350, 1200),
            vec3(1200, 0, 0),
            vec3(0, 0, 1600),
            shared_ptr<material>()));
        std::clog << "Ceiling light: enabled, intensity "
                  << settings.ceiling_light_intensity << ".\n";
    } else {
        std::clog << "Ceiling light: disabled.\n";
    }

    const auto ground_light_a = layout.place(
        330, -1350, 1350, 250, 3000, 0.0);
    const auto ground_light_b = layout.place(
        350, -1350, 1350, 500, 3600, 0.0);
    const auto air_light_a = layout.place(
        320, -1400, 1400, 900, 4200, 1.0, 420, 720);
    const scene_placement air_light_b{-1350, -80, 0, 520};
    const double sphere_light_scale = settings.sphere_light_intensity;
    add_sphere_light(scene,
        point3(ground_light_a.x, 205, ground_light_a.z),
        205, sphere_light_scale * color(18, 11, 7));
    add_sphere_light(scene,
        point3(ground_light_b.x, 220, ground_light_b.z),
        220, sphere_light_scale * color(7, 12, 20));
    add_sphere_light(scene,
        point3(air_light_a.x, 190 + air_light_a.hover, air_light_a.z),
        190, sphere_light_scale * color(19, 8, 14));
    add_sphere_light(scene,
        point3(air_light_b.x, 200 + air_light_b.hover, air_light_b.z),
        200, sphere_light_scale * color(8, 18, 11));
    std::clog << "Sphere light intensity multiplier: "
              << settings.sphere_light_intensity << ".\n";

    auto glass = make_shared<dielectric>(1.5);
    auto polished_metal = make_shared<metal>(color(.86, .90, .96), .015);
    auto rough_metal = make_shared<metal>(color(.72, .48, .20), .32);
    auto marble = make_shared<lambertian>(make_shared<noise_texture>(.028));
    auto checker = make_shared<lambertian>(make_shared<checker_texture>(
        28.0, color(.08, .12, .55), color(.85, .82, .12)));
    auto earth = make_shared<lambertian>(image(source_root / "earthmap.jpg"));
    auto uv_grid = make_shared<lambertian>(image(models / "grid.tga"));

    // 固定补充前景物体，避免随机深度范围导致前景过空。
    auto foreground_gold = make_shared<metal>(color(.95, .62, .12), .08);
    auto foreground_red = make_shared<lambertian>(color(.88, .08, .05));
    auto foreground_green = make_shared<lambertian>(color(.08, .78, .24));
    auto foreground_blue = make_shared<lambertian>(color(.06, .28, .92));
    const shared_ptr<material> foreground_materials[] = {
        glass,
        foreground_gold,
        foreground_red,
        foreground_green,
        foreground_blue,
        checker,
        earth
    };
    const double foreground_radii[] = {140, 126, 118, 110, 104, 100, 122};
    for (int index = 0; index < 7; ++index) {
        const auto placement = layout.place(
            1.30 * foreground_radii[index],
            -1700, 1700, -430, 80, 0.0);
        scene.world.add(make_shared<sphere>(
            point3(placement.x, foreground_radii[index], placement.z),
            foreground_radii[index],
            foreground_materials[index]));
    }

    // 主体球体与 OBJ 模型共用可复现的随机布局。
    const shared_ptr<material> hero_materials[] = {
        glass, rough_metal, polished_metal, earth, marble, checker, uv_grid
    };
    const double hero_radii[] = {205, 160, 225, 200, 128, 118, 122};
    for (int index = 0; index < 7; ++index) {
        const auto placement = layout.place(
            1.55 * hero_radii[index], -1900, 1900, -100, 3800, .36, 110, 430);
        const double radius = distance_scale(hero_radii[index], placement.z);
        scene.world.add(make_shared<sphere>(
            point3(placement.x, radius + placement.hover, placement.z),
            radius,
            hero_materials[index]));
    }

    auto mirror = make_shared<metal>(color(.98, .98, .98), 0.0);
    auto mirror_body = make_shared<lambertian>(color(.08, .10, .12));
    auto add_mirror_box = [&](double width,
                              double height,
                              double depth,
                              double x,
                              double z,
                              double rotation) {
        auto body = box(
            point3(-.5 * width, 0, 0),
            point3(.5 * width, height, depth),
            mirror_body);
        auto panel = make_shared<quad>(
            point3(-.5 * width + 24, 24, -2),
            vec3(width - 48, 0, 0),
            vec3(0, height - 48, 0),
            mirror);
        const object_transform transform{rotation, vec3(x, 0, z)};
        scene.world.add(apply_object_transform(body, transform));
        scene.world.add(apply_object_transform(panel, transform));
    };
    // 两个镜面朝向相机并略向场景中心倾斜，以反射中央物体组。
    add_mirror_box(650, 1200, 320, -1320, 1650, -42);
    add_mirror_box(700, 1000, 320, 350, 700, 18);

    // Spot 仅保留能直观看出差异的纹理、凹凸和位移版本。
    const auto spot = models / "spot";
    auto spot_albedo = image(spot / "spot_texture.png");
    auto spot_texture = make_shared<lambertian>(spot_albedo);
    auto spot_height = image(spot / "hmap.jpg", image_color_space::linear);
    auto spot_bump = make_shared<bump_lambertian>(spot_albedo, spot_height, 2.0);
    const scene_placement spot_texture_placement{-900, 500, -28, 0};
    const scene_placement spot_bump_placement{-400, 950, 18, 0};
    const scene_placement spot_displacement_placement{850, 1450, -36, 0};
    const double spot_texture_scale = distance_scale(210, spot_texture_placement.z);
    const double spot_bump_scale = distance_scale(210, spot_bump_placement.z);
    const double spot_displacement_size = distance_scale(
        210, spot_displacement_placement.z);
    add_obj(scene, spot / "spot_triangulated_good.obj", spot_texture,
        spot_texture_scale,
        vec3(
            spot_texture_placement.x,
            .76 * spot_texture_scale + spot_texture_placement.hover,
            spot_texture_placement.z),
        front_facing_rotation(0, spot_texture_placement.rotation), obj_bvh_method);
    add_obj(scene, spot / "spot_triangulated_good.obj", spot_bump,
        spot_bump_scale,
        vec3(
            spot_bump_placement.x,
            .76 * spot_bump_scale + spot_bump_placement.hover,
            spot_bump_placement.z),
        front_facing_rotation(0, spot_bump_placement.rotation), obj_bvh_method);
    add_obj(scene, spot / "spot_triangulated_good.obj", spot_bump,
        spot_displacement_size,
        vec3(
            spot_displacement_placement.x,
            .76 * spot_displacement_size + spot_displacement_placement.hover,
            spot_displacement_placement.z),
        front_facing_rotation(0, spot_displacement_placement.rotation), obj_bvh_method,
        obj_missing_normal_mode::smoothing_groups,
        displacement_map(spot_height, 8.0));

    // 非洲人头模型包含漫反射和切线空间法线贴图版本；外层眼球使用透明角膜材质。
    const auto african = models / "african_head";
    auto african_head_albedo = image(african / "african_head_diffuse.tga");
    auto african_inner_albedo = image(african / "african_head_eye_inner_diffuse.tga");
    auto african_head_diffuse = make_shared<lambertian>(african_head_albedo);
    auto african_cornea = make_shared<dielectric>(1.38);
    auto african_inner_diffuse = make_shared<lambertian>(african_inner_albedo);
    auto african_head_normal = make_shared<normal_mapped_lambertian>(
        african_head_albedo,
        image(african / "african_head_nm_tangent.tga", image_color_space::linear));
    auto african_inner_normal = make_shared<normal_mapped_lambertian>(
        african_inner_albedo,
        image(african / "african_head_eye_inner_nm_tangent.tga", image_color_space::linear));
    const shared_ptr<material> african_head_materials[] = {
        african_head_diffuse, african_head_normal
    };
    const shared_ptr<material> african_inner_materials[] = {
        african_inner_diffuse, african_inner_normal
    };
    const scene_placement african_placements[] = {
        {-1500, 3300, 30, 0},
        {1500, 3800, -30, 0}
    };
    for (int version = 0; version < 2; ++version) {
        const auto placement = african_placements[version];
        const double scale = distance_scale(265, placement.z);
        const vec3 position(
            placement.x, scale + placement.hover, placement.z);
        const double rotation = front_facing_rotation(180, placement.rotation);
        add_obj(scene, african / "african_head.obj", african_head_materials[version],
            scale, position, rotation, obj_bvh_method);
        add_obj(scene, african / "african_head_eye_outer.obj", african_cornea,
            scale, position, rotation, obj_bvh_method);
        add_obj(scene, african / "african_head_eye_inner.obj", african_inner_materials[version],
            scale, position, rotation, obj_bvh_method);
    }

    // Boggie 包含漫反射和法线贴图版本，各组件网格保持对齐。
    const auto boggie = models / "boggie";
    auto boggie_body_albedo = image(boggie / "body_diffuse.tga");
    auto boggie_head_albedo = image(boggie / "head_diffuse.tga");
    auto boggie_eyes_albedo = image(boggie / "eyes_diffuse.tga");
    auto boggie_body_diffuse = make_shared<lambertian>(boggie_body_albedo);
    auto boggie_head_diffuse = make_shared<lambertian>(boggie_head_albedo);
    auto boggie_eyes_diffuse = make_shared<lambertian>(boggie_eyes_albedo);
    auto boggie_body_normal = make_shared<normal_mapped_lambertian>(
        boggie_body_albedo,
        image(boggie / "body_nm_tangent.tga", image_color_space::linear));
    auto boggie_head_normal = make_shared<normal_mapped_lambertian>(
        boggie_head_albedo,
        image(boggie / "head_nm_tangent.tga", image_color_space::linear));
    auto boggie_eyes_normal = make_shared<normal_mapped_lambertian>(
        boggie_eyes_albedo,
        image(boggie / "eyes_nm_tangent.tga", image_color_space::linear));
    const shared_ptr<material> boggie_body_materials[] = {
        boggie_body_diffuse, boggie_body_normal
    };
    const shared_ptr<material> boggie_head_materials[] = {
        boggie_head_diffuse, boggie_head_normal
    };
    const shared_ptr<material> boggie_eyes_materials[] = {
        boggie_eyes_diffuse, boggie_eyes_normal
    };
    for (int version = 0; version < 2; ++version) {
        const auto placement = layout.place(
            330, -1450, 1450, 500, 5200, .36, 120, 430);
        const double scale = distance_scale(265, placement.z);
        const vec3 position(
            placement.x, scale + placement.hover, placement.z);
        const double rotation = front_facing_rotation(180, placement.rotation);
        add_obj(scene, boggie / "body.obj", boggie_body_materials[version],
            scale, position, rotation, obj_bvh_method);
        add_obj(scene, boggie / "head.obj", boggie_head_materials[version],
            scale, position, rotation, obj_bvh_method);
        add_obj(scene, boggie / "eyes.obj", boggie_eyes_materials[version],
            scale, position, rotation, obj_bvh_method);
    }

    // Diablo 包含漫反射纹理和法线贴图版本。
    const auto diablo = models / "diablo3_pose";
    auto diablo_albedo = image(diablo / "diablo3_pose_diffuse.tga");
    auto diablo_diffuse = make_shared<lambertian>(diablo_albedo);
    auto diablo_normal = make_shared<normal_mapped_lambertian>(
        diablo_albedo,
        image(diablo / "diablo3_pose_nm_tangent.tga", image_color_space::linear));
    const shared_ptr<material> diablo_materials[] = {
        diablo_diffuse, diablo_normal
    };
    for (int version = 0; version < 2; ++version) {
        const auto placement = layout.place(
            340, -1450, 1450, 600, 5300, .36, 130, 450);
        const double scale = distance_scale(265, placement.z);
        add_obj(scene, diablo / "diablo3_pose.obj", diablo_materials[version],
            scale, vec3(placement.x, scale + placement.hover, placement.z),
            front_facing_rotation(180, placement.rotation), obj_bvh_method);
    }

    // 缺少完整贴图组的模型只保留信息最完整的版本。
    auto neutral = make_shared<lambertian>(color(.52, .54, .58));
    auto bunny_blue = make_shared<lambertian>(color(.10, .30, .72));
    auto bunny_white = make_shared<lambertian>(color(.82, .72, .58));
    const auto bunny = models / "bunny" / "bunny.obj";
    const scene_placement bunny_blue_placement{-700, 1600, -22, 0};
    const scene_placement bunny_white_placement{200, 1900, 28, 0};
    const double bunny_blue_scale = distance_scale(1780, bunny_blue_placement.z);
    const double bunny_white_scale = distance_scale(1780, bunny_white_placement.z);
    add_obj(scene, bunny, bunny_blue, bunny_blue_scale,
        vec3(
            bunny_blue_placement.x,
            -.03 * bunny_blue_scale + bunny_blue_placement.hover,
            bunny_blue_placement.z),
        front_facing_rotation(180, bunny_blue_placement.rotation),
        obj_bvh_method, obj_missing_normal_mode::smooth);
    add_obj(scene, bunny, bunny_white, bunny_white_scale,
        vec3(
            bunny_white_placement.x,
            -.03 * bunny_white_scale + bunny_white_placement.hover,
            bunny_white_placement.z),
        front_facing_rotation(180, bunny_white_placement.rotation),
        obj_bvh_method, obj_missing_normal_mode::smooth);

    const auto rock = models / "rock";
    auto rock_albedo = image(rock / "rock.png");
    auto rock_texture = make_shared<lambertian>(rock_albedo);
    auto rock_height = image(rock / "rock.png", image_color_space::linear);
    auto rock_bump = make_shared<bump_lambertian>(rock_albedo, rock_height, 1.5);
    const shared_ptr<material> rock_materials[] = {
        rock_texture, rock_bump
    };
    for (int version = 0; version < 2; ++version) {
        const auto placement = layout.place(
            350, -1550, 1550, 500, 5400, .36, 100, 420);
        const double scale = distance_scale(100, placement.z);
        add_obj(scene, rock / "rock.obj", rock_materials[version], scale,
            vec3(placement.x, .31 * scale + placement.hover, placement.z),
            placement.rotation, obj_bvh_method);
    }

    const auto crate = models / "Crate" / "Crate1.obj";
    const auto crate_mtl_placement = layout.place(
        225, -1450, 1450, 300, 4750, .36, 100, 420);
    const double crate_mtl_scale = distance_scale(90, crate_mtl_placement.z);
    add_obj(scene, crate, neutral, crate_mtl_scale,
        vec3(
            crate_mtl_placement.x,
            crate_mtl_scale + crate_mtl_placement.hover,
            crate_mtl_placement.z),
        crate_mtl_placement.rotation,
        obj_bvh_method, obj_missing_normal_mode::flat,
        displacement_map(), obj_material_mode::mtl);

    const auto cube = models / "cube" / "cube.obj";
    const auto cube_mtl_placement = layout.place(
        225, -1450, 1450, 300, 4750, .36, 100, 420);
    const double cube_mtl_scale = distance_scale(90, cube_mtl_placement.z);
    add_obj(scene, cube, neutral, cube_mtl_scale,
        vec3(
            cube_mtl_placement.x,
            cube_mtl_scale + cube_mtl_placement.hover,
            cube_mtl_placement.z),
        cube_mtl_placement.rotation,
        obj_bvh_method, obj_missing_normal_mode::flat,
        displacement_map(), obj_material_mode::mtl);

    // 地板 OBJ 实例用于对比漫反射纹理与法线贴图。
    const auto floor_obj = models / "floor.obj";
    auto floor_albedo = image(models / "floor_diffuse.tga");
    auto floor_diffuse = make_shared<lambertian>(floor_albedo);
    auto floor_normal = make_shared<normal_mapped_lambertian>(
        floor_albedo,
        image(models / "floor_nm_tangent.tga", image_color_space::linear));
    const shared_ptr<material> floor_materials[] = {
        floor_diffuse, floor_normal
    };
    for (int version = 0; version < 2; ++version) {
        const auto placement = layout.place(
            460, -2100, 2100, 500, 5600, .36, 100, 320);
        const double scale = distance_scale(325, placement.z);
        add_obj(scene, floor_obj, floor_materials[version],
            scale,
            vec3(placement.x, scale + 2 + placement.hover, placement.z),
            placement.rotation,
            obj_bvh_method, obj_missing_normal_mode::flat);
    }

    // 即使关闭全局雾，三个局部介质仍会保留。
    const auto white_smoke_placement = layout.place(
        265, -2100, 2100, 500, 5500, .36, 100, 380);
    const auto dark_smoke_placement = layout.place(
        265, -2100, 2100, 500, 5500, .36, 100, 380);
    const auto blue_fog_placement = layout.place(
        275, -2100, 2100, 500, 5500, .36, 100, 380);
    const double white_smoke_scale = 1.0
        + .00012 * std::max(0.0, white_smoke_placement.z);
    const double dark_smoke_scale = 1.0
        + .00012 * std::max(0.0, dark_smoke_placement.z);
    const double blue_fog_scale = 1.0
        + .00012 * std::max(0.0, blue_fog_placement.z);
    auto white_smoke_boundary = apply_object_transform(
        box(point3(0, 0, 0), white_smoke_scale * point3(340, 460, 330), nullptr),
        object_transform{
            white_smoke_placement.rotation,
            vec3(
                white_smoke_placement.x,
                white_smoke_placement.hover,
                white_smoke_placement.z)});
    auto dark_smoke_boundary = apply_object_transform(
        box(point3(0, 0, 0), dark_smoke_scale * point3(330, 500, 320), nullptr),
        object_transform{
            dark_smoke_placement.rotation,
            vec3(
                dark_smoke_placement.x,
                dark_smoke_placement.hover,
                dark_smoke_placement.z)});
    auto blue_fog_boundary = apply_object_transform(
        box(point3(0, 0, 0), blue_fog_scale * point3(380, 355, 355), nullptr),
        object_transform{
            blue_fog_placement.rotation,
            vec3(
                blue_fog_placement.x,
                blue_fog_placement.hover,
                blue_fog_placement.z)});
    scene.world.add(make_shared<constant_medium>(
        white_smoke_boundary, .009, color(.92, .92, .92), 0.0));
    scene.world.add(make_shared<constant_medium>(
        dark_smoke_boundary, .012, color(.08, .08, .08), 0.0));
    scene.world.add(make_shared<constant_medium>(
        blue_fog_boundary, .006, color(.35, .55, .90), .25));
    std::clog << "Local participating media: 3 smoke/fog volumes.\n";

    add_random_sphere_field(scene, layout);

    if (settings.enable_global_fog) {
        auto atmosphere_boundary = box(
            point3(-2600, -300, -2400), point3(2600, 1600, 6200), nullptr);
        scene.world.add(make_shared<constant_medium>(
            atmosphere_boundary,
            settings.global_fog_density,
            color(.70, .78, .90),
            .35));
        std::clog << "Global atmosphere: enabled, density "
                  << settings.global_fog_density << ".\n";
    } else {
        std::clog << "Global atmosphere: disabled.\n";
    }

    std::clog << "Final layout seed: " << layout_seed << ", "
              << layout.hovering() << '/' << layout.placements()
              << " placed object groups hovering ("
              << 100.0 * layout.hovering() / layout.placements() << "%).\n";

    scene.cam.lookfrom = point3(0, 650, -2050);
    scene.cam.lookat = point3(0, 270, 2050);

    return scene;
}

#endif
