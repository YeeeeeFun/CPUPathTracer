// main.cpp
#include "scenes.h"

#include <iostream>

// 公共测试参数
namespace render_config {
constexpr int scene_id = 3;  // 1：康奈尔盒，2：功能展示，3：最终展示
constexpr bvh_split_method obj_bvh_method = bvh_split_method::sah;
constexpr bool enable_world_bvh = true;
constexpr bvh_split_method world_bvh_method = bvh_split_method::sah;
constexpr mis_heuristic sampling_heuristic = mis_heuristic::power; // balance 或 power

constexpr bool enable_showcase_fog = true;
constexpr double showcase_fog_density = .00010;

constexpr bool enable_final_global_fog = false;
constexpr double final_global_fog_density = .00008;

constexpr bool enable_final_ceiling_light = true;
constexpr double final_ceiling_light_intensity = 2.0;
constexpr double final_sphere_light_intensity = 0.5; // 0.5 为一半，2.0 为两倍

constexpr russian_roulette_config russian_roulette{
    true, // 是否启用
    5,    // 开始深度
    .05,  // 最小存活概率
    .95   // 最大存活概率
};

constexpr double aspect_ratio = 1.2;
constexpr int image_width = 1500;
constexpr int samples_per_pixel = 100;
constexpr int max_depth = 50;
const color background = color(0, 0, 0);
constexpr double vfov = 40;
const vec3 vup = vec3(0, 1, 0);
constexpr double defocus_angle = 0;
}

void apply_common_render_settings(camera& cam) {
    cam.aspect_ratio = render_config::aspect_ratio;
    cam.image_width = render_config::image_width;
    cam.samples_per_pixel = render_config::samples_per_pixel;
    cam.max_depth = render_config::max_depth;
    cam.background = render_config::background;
    cam.vfov = render_config::vfov;
    cam.vup = render_config::vup;
    cam.defocus_angle = render_config::defocus_angle;
    cam.sampling_heuristic = render_config::sampling_heuristic;
    cam.russian_roulette = render_config::russian_roulette;
}

int main() {
#ifdef _OPENMP
    std::clog << "Build: " << RT_BUILD_TYPE << '\n'
              << "OpenMP: enabled (_OPENMP=" << _OPENMP << ")\n";
#else
    std::clog << "Build: " << RT_BUILD_TYPE << '\n'
              << "OpenMP: disabled\n";
#endif

    render_scene scene;

    switch (render_config::scene_id) {
        case 1:
            scene = make_cornell_scene();
            break;
        case 2:
            scene = make_showcase_scene(
                render_config::obj_bvh_method,
                showcase_scene_settings{
                    render_config::enable_showcase_fog,
                    render_config::showcase_fog_density});
            break;
        case 3:
            scene = make_final_showcase_scene(
                render_config::obj_bvh_method,
                final_showcase_settings{
                    render_config::enable_final_global_fog,
                    render_config::final_global_fog_density,
                    render_config::enable_final_ceiling_light,
                    render_config::final_ceiling_light_intensity,
                    render_config::final_sphere_light_intensity});
            break;
        default:
            std::cerr << "Unknown scene_id: " << render_config::scene_id << '\n'
                      << "Set scene_id to 1 (Cornell), 2 (Showcase), "
                      << "or 3 (Final Showcase).\n";
            return 1;
    }

    apply_common_render_settings(scene.cam);

    if (render_config::enable_world_bvh) {
        scene.build_world_bvh(render_config::world_bvh_method);
    } else {
        std::clog << "Top-level acceleration: linear list, "
                  << scene.world.objects.size() << " objects\n";
    }

    std::clog << "Scene: " << scene.name << '\n';
    scene.cam.render(scene.render_world(), scene.lights);
}
