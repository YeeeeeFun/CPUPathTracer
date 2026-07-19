// camera.h
#ifndef CAMERA_H
#define CAMERA_H
#include <iostream>
#include <fstream>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <omp.h>
#include "rtweekend.h"
#include "constant.h"
#include "interval.h"
#include "color.h"
#include "hittable.h"
#include "material.h"
#include "path_sampling.h"
#include "pdf.h"

#ifndef RT_PROJECT_SOURCE_DIR
#define RT_PROJECT_SOURCE_DIR "."
#endif

class camera
{
public:
    /* 这里是公共相机参数 */
    double aspect_ratio = 1.0;  // 图像宽度与高度之比
    int image_width = 100;      // 以像素为单位的渲染图像宽度
    int samples_per_pixel = 10; // 每个像素的随机样本数
    int max_depth = 10;         // 光线在场景中反弹的最大次数
    color background;                 // 场景背景色

    double vfov = 90;                  // 垂直视场角
    point3 lookfrom = point3(0, 0, 0); // 相机位置
    point3 lookat = point3(0, 0, -1);  // 观察目标
    vec3 vup = vec3(0, 1, 0);          // 相机参考上方向，通常指向 +Y

    double defocus_angle = 0; // 通过每个像素的光线的变化角度
    double focus_dist = 10;   // 从摄像机观察点到完全聚焦平面的距离

    // 默认最多使用可用逻辑处理器的 80%，可通过 RT_NUM_THREADS 进一步下调。
    static constexpr int max_cpu_percent = 80;
    int tile_size = 16;
    std::uint64_t random_seed = 1337;
    mis_heuristic sampling_heuristic = mis_heuristic::power;
    russian_roulette_config russian_roulette;

    void render(const hittable &world, const hittable& lights)
    {
        const auto run_start = std::chrono::steady_clock::now();
        initialize();

        const int thread_count = render_thread_count();
        const int effective_tile_size = std::max(1, tile_size);
        const int tiles_x = (image_width + effective_tile_size - 1) / effective_tile_size;
        const int tiles_y = (image_height + effective_tile_size - 1) / effective_tile_size;
        const int tile_count = tiles_x * tiles_y;
        std::vector<color> framebuffer(static_cast<std::size_t>(image_width) * image_height);
        std::vector<render_statistics> thread_statistics(thread_count);
        std::vector<std::atomic<int>> remaining_tiles_per_row(image_height);
        for (auto& remaining : remaining_tiles_per_row)
            remaining.store(tiles_x, std::memory_order_relaxed);

        std::atomic<int> completed_tiles{0};
        int next_row_to_write = 0;
        int last_reported_percent = 0;
        const int progress_interval = std::max(1, tile_count / 20);

        std::error_code output_error;
        const auto output_path = next_output_path(output_error);
        if (output_error) {
            std::cerr << "Failed to prepare the results directory: "
                      << output_error.message() << '\n';
            return;
        }

        std::ofstream file(output_path, std::ios::out | std::ios::trunc);
        if (!file) {
            std::cerr << "Failed to open " << output_path.string() << " for writing.\n";
            return;
        }

        file << "P3\n"
             << image_width << " " << image_height << "\n255\n";
        file.flush();

        std::clog << "Rendering with " << thread_count << " threads ("
                  << max_cpu_percent << "% logical CPU cap), "
                  << effective_tile_size << "x" << effective_tile_size << " tiles.\n";
        std::clog << "MIS: " << mis_heuristic_name(sampling_heuristic)
                  << " heuristic, 50% light / 50% material-or-phase sampling.\n";
        if (russian_roulette.enabled) {
            std::clog << "Russian roulette: enabled from depth "
                      << russian_roulette.start_depth << ", survival "
                      << 100 * russian_roulette.min_survival_probability << "% to "
                      << 100 * russian_roulette.max_survival_probability << "%.\n";
        } else {
            std::clog << "Russian roulette: disabled.\n";
        }
        std::clog << "Output: " << output_path.string() << '\n';

        omp_set_dynamic(0);

        #pragma omp parallel for schedule(dynamic, 1) num_threads(thread_count)
        for (int tile_index = 0; tile_index < tile_count; ++tile_index) {
            render_statistics& statistics = thread_statistics[omp_get_thread_num()];
            const int tile_x = tile_index % tiles_x;
            const int tile_y = tile_index / tiles_x;
            const int x_begin = tile_x * effective_tile_size;
            const int y_begin = tile_y * effective_tile_size;
            const int x_end = std::min(x_begin + effective_tile_size, image_width);
            const int y_end = std::min(y_begin + effective_tile_size, image_height);

            for (int j = y_begin; j < y_end; ++j) {
                for (int i = x_begin; i < x_end; ++i) {
                    const auto pixel_index = static_cast<std::size_t>(j) * image_width + i;
                    seed_random(pixel_seed(pixel_index));

                    color pixel_color(0, 0, 0);
                    const int stratum_count = sqrt_spp * sqrt_spp;
                    for (int sample_index = 0;
                         sample_index < samples_per_pixel;
                         ++sample_index) {
                        // 将非完全平方的样本均匀映射到向上取整的分层网格，避免丢弃样本。
                        const int stratum_index = static_cast<int>(
                            ((2LL * sample_index + 1) * stratum_count)
                            / (2LL * samples_per_pixel));
                        const int s_i = stratum_index % sqrt_spp;
                        const int s_j = stratum_index / sqrt_spp;
                        ray r = get_ray(i, j, s_i, s_j);
                        pixel_color += ray_color(
                            r,
                            max_depth,
                            0,
                            color(1, 1, 1),
                            world,
                            lights,
                            statistics);
                    }
                    framebuffer[pixel_index] = pixel_samples_scale * pixel_color;
                }
            }

            for (int j = y_begin; j < y_end; ++j)
                remaining_tiles_per_row[j].fetch_sub(1, std::memory_order_release);

            // Tile 会乱序完成；这里只按从上到下的顺序写出已经完整的连续扫描行。
            #pragma omp critical(frame_output)
            {
                const int first_unwritten_row = next_row_to_write;
                while (next_row_to_write < image_height
                    && remaining_tiles_per_row[next_row_to_write].load(std::memory_order_acquire) == 0) {
                    const auto row_offset = static_cast<std::size_t>(next_row_to_write) * image_width;
                    for (int i = 0; i < image_width; ++i)
                        write_color(file, framebuffer[row_offset + i]);
                    ++next_row_to_write;
                }

                if (next_row_to_write != first_unwritten_row
                    && (next_row_to_write == image_height || next_row_to_write % 16 == 0)) {
                    file.flush();
                }
            }

            const int done = completed_tiles.fetch_add(1, std::memory_order_relaxed) + 1;
            if (done == tile_count || done % progress_interval == 0) {
                #pragma omp critical(render_progress)
                {
                    const int current_done = completed_tiles.load(std::memory_order_relaxed);
                    const int current_percent = 100 * current_done / tile_count;
                    if (current_percent > last_reported_percent) {
                        std::clog << "\rRender progress: " << current_percent << '%' << std::flush;
                        last_reported_percent = current_percent;
                    }
                }
            }
        }

        std::clog << '\n';

        file.flush();
        file.close();

        const auto elapsed_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - run_start).count();
        const auto hours = elapsed_milliseconds / 3600000;
        const auto minutes = (elapsed_milliseconds % 3600000) / 60000;
        const auto seconds = (elapsed_milliseconds % 60000) / 1000;
        const auto milliseconds = elapsed_milliseconds % 1000;

        render_statistics totals;
        for (const auto& statistics : thread_statistics)
            totals.add(statistics);
        const double average_path_depth = totals.completed_paths > 0
            ? static_cast<double>(totals.path_depth_sum) / totals.completed_paths
            : 0;
        const double rr_termination_percent = totals.rr_tests > 0
            ? 100.0 * totals.rr_terminations / totals.rr_tests
            : 0;
        const double elapsed_seconds = std::max(0.001, elapsed_milliseconds / 1000.0);
        const auto rays_per_second = static_cast<std::uint64_t>(
            totals.traced_rays / elapsed_seconds);

        std::clog << "Done: " << output_path.string() << '\n';
        std::clog << "Run time: " << hours << "h " << minutes << "m "
                  << seconds << "s " << milliseconds << "ms\n";
        std::clog << "Path statistics: " << totals.completed_paths << " paths, "
                  << totals.traced_rays << " traced rays, average depth "
                  << average_path_depth << ", maximum depth "
                  << totals.max_path_depth << ", " << rays_per_second
                  << " rays/s.\n";
        if (russian_roulette.enabled) {
            std::clog << "Russian roulette: " << totals.rr_terminations
                      << " terminations / " << totals.rr_tests << " decisions ("
                      << rr_termination_percent << "%).\n";
        }
    }

private:
    struct render_statistics {
        std::uint64_t traced_rays = 0;
        std::uint64_t completed_paths = 0;
        std::uint64_t path_depth_sum = 0;
        std::uint64_t rr_tests = 0;
        std::uint64_t rr_terminations = 0;
        int max_path_depth = 0;

        void complete_path(int path_depth) {
            ++completed_paths;
            path_depth_sum += static_cast<std::uint64_t>(std::max(0, path_depth));
            max_path_depth = std::max(max_path_depth, path_depth);
        }

        void add(const render_statistics& other) {
            traced_rays += other.traced_rays;
            completed_paths += other.completed_paths;
            path_depth_sum += other.path_depth_sum;
            rr_tests += other.rr_tests;
            rr_terminations += other.rr_terminations;
            max_path_depth = std::max(max_path_depth, other.max_path_depth);
        }
    };

    /* 私有相机变量 */
    int image_height;           // 渲染图像高度
    double pixel_samples_scale; // 像素样本总和的颜色缩放因子
    int sqrt_spp;                // 每像素分层网格边长
    double recip_sqrt_spp;       // 1 / sqrt_spp

    point3 center;              // 相机中心
    point3 pixel00_loc;         // （0，0）像素的位置
    vec3 pixel_delta_u;         // 向右偏移像素
    vec3 pixel_delta_v;         // 向下偏移像素
    vec3 u, v, w;               // 摄像机帧基向量
    vec3 defocus_disk_u;        // 聚焦盘水平半径
    vec3 defocus_disk_v;        // 聚焦盘垂直半径

    std::filesystem::path next_output_path(std::error_code& error) const {
        const auto output_directory = std::filesystem::path(RT_PROJECT_SOURCE_DIR) / "results";
        std::filesystem::create_directories(output_directory, error);
        if (error)
            return {};

        int highest_index = 0;
        std::filesystem::directory_iterator iterator(output_directory, error);
        const std::filesystem::directory_iterator end;

        while (!error && iterator != end) {
            if (iterator->is_regular_file(error) && !error) {
                const auto path = iterator->path();
                const auto stem = path.stem().string();
                const auto dash = stem.rfind('-');

                // 兼容已有的 new2070-N，并统一把下一张命名为 new5070-N。
                if (path.extension() == ".ppm" && stem.rfind("new", 0) == 0
                    && dash != std::string::npos && dash > 3 && dash + 1 < stem.size()) {
                    const auto device_part = stem.substr(3, dash - 3);
                    const auto index_part = stem.substr(dash + 1);
                    const auto is_decimal_digit = [](unsigned char value) {
                        return std::isdigit(value) != 0;
                    };
                    const bool valid_device = std::all_of(
                        device_part.begin(), device_part.end(), is_decimal_digit);
                    const bool valid_index = std::all_of(
                        index_part.begin(), index_part.end(), is_decimal_digit);

                    if (valid_device && valid_index) {
                        try {
                            highest_index = std::max(highest_index, std::stoi(index_part));
                        } catch (const std::exception&) {
                            // 忽略超出 int 范围或格式异常的旧文件名。
                        }
                    }
                }
            }
            iterator.increment(error);
        }

        if (error)
            return {};

        return output_directory / ("new5070-" + std::to_string(highest_index + 1) + ".ppm");
    }

    int render_thread_count() const {
        const int available_threads = std::max(1, omp_get_num_procs());
        const int thread_cap = std::max(1, available_threads * max_cpu_percent / 100);

        const char* requested_value = std::getenv("RT_NUM_THREADS");
        if (!requested_value)
            return thread_cap;

        char* parse_end = nullptr;
        const long requested_threads = std::strtol(requested_value, &parse_end, 10);
        if (parse_end == requested_value || *parse_end != '\0' || requested_threads < 1)
            return thread_cap;

        if (requested_threads >= thread_cap)
            return thread_cap;

        return static_cast<int>(requested_threads);
    }

    std::uint64_t pixel_seed(std::size_t pixel_index) const {
        auto value = random_seed + static_cast<std::uint64_t>(pixel_index) + 0x9e3779b97f4a7c15ULL;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    void initialize()
    {
        image_height = static_cast<int>(image_width / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        samples_per_pixel = std::max(1, samples_per_pixel);
        sqrt_spp = static_cast<int>(std::ceil(std::sqrt(samples_per_pixel)));
        pixel_samples_scale = 1.0 / samples_per_pixel;
        recip_sqrt_spp = 1.0 / sqrt_spp;

        center = lookfrom;

        // 确定视口尺寸
        auto theta = degrees_to_radians(vfov);
        auto h = std::tan(theta / 2);
        auto viewport_height = 2 * h * focus_dist;
        auto viewport_width = viewport_height * (static_cast<double>(image_width) / image_height);

        // 计算摄像机坐标系的 u、v、w 单位基向量
        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        // 计算横向和纵向视口边缘的矢量
        auto viewport_u = viewport_width * u;   // 视口水平边缘的向量
        auto viewport_v = viewport_height * -v; // 视口垂直边缘的向量

        // 计算像素间的水平和垂直增量向量
        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / image_height;

        // 计算左上角像素的位置
        auto viewport_upper_left = center - (focus_dist * w) - viewport_u / 2 - viewport_v / 2;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

        // 计算摄像机离焦盘基向量。
        auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
    }

    ray get_ray(int i, int j, int s_i, int s_j) const
    {
        // 在像素 (i,j) 的分层网格 (s_i,s_j) 中生成相机射线。

        auto offset = sample_square_stratified(s_i,s_j);
        auto pixel_sample = pixel00_loc + ((i + offset.x()) * pixel_delta_u) + ((j + offset.y()) * pixel_delta_v);

        auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;
        auto ray_time = random_double();

        return ray(ray_origin, ray_direction, ray_time);
    }

    vec3 sample_square_stratified(int s_i, int s_j) const
    {
        // 在单位像素 [-0.5,0.5] 的指定分层内生成随机偏移。
        
        auto px=((s_i+random_double())*recip_sqrt_spp)-0.5;
        auto py=((s_j+random_double())*recip_sqrt_spp)-0.5;

        return vec3(px,py,0);
    }

    point3 defocus_disk_sample() const
    {
        // 返回摄像机离焦盘中的一个随机点
        auto p = random_in_unit_disk();
        return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
    }

    bool continue_after_russian_roulette(
        int next_path_depth,
        color& scatter_weight,
        color& next_path_throughput,
        render_statistics& statistics) const {
        if (!russian_roulette_enabled_for_depth(
                russian_roulette, next_path_depth)) {
            return true;
        }

        ++statistics.rr_tests;
        const double survival_probability =
            russian_roulette_survival_probability(
                next_path_throughput, russian_roulette);
        if (!(survival_probability > 0)
            || random_double() >= survival_probability) {
            ++statistics.rr_terminations;
            statistics.complete_path(next_path_depth);
            return false;
        }

        scatter_weight /= survival_probability;
        next_path_throughput /= survival_probability;
        return true;
    }

    color ray_color(
        const ray& r,
        int depth,
        int path_depth,
        const color& path_throughput,
        const hittable& world,
        const hittable& lights,
        render_statistics& statistics) const
    {
        // 如果我们超过了光线反弹限制，就不会再聚集光线。
        if (depth <= 0) {
            statistics.complete_path(path_depth);
            return color(0, 0, 0);
        }

        ++statistics.traced_rays;

        hit_record rec;

        // 如果光线没有碰到任何物体，则返回背景颜色
        if (!world.hit(r, interval(0.001, infinity), rec)) {
            statistics.complete_path(path_depth);
            return background;
        }

        scatter_record srec;
        color color_from_emission = rec.mat->emitted(r, rec,rec.u, rec.v, rec.p);

        if (!rec.mat->scatter(r, rec, srec)) {
            statistics.complete_path(path_depth);
            return color_from_emission;
        }

        const int next_path_depth = path_depth + 1;

        if (srec.skip_pdf) {
            color scatter_weight = srec.attenuation;
            color next_path_throughput = path_throughput * scatter_weight;
            if (!continue_after_russian_roulette(
                    next_path_depth,
                    scatter_weight,
                    next_path_throughput,
                    statistics)) {
                return color_from_emission;
            }
            return color_from_emission + scatter_weight * ray_color(
                srec.skip_pdf_ray,
                depth - 1,
                next_path_depth,
                next_path_throughput,
                world,
                lights,
                statistics);
        }

        auto light_pdf = make_shared<hittable_pdf>(lights, rec.p);
        // 一样本 MIS：选择一种采样技术，并补偿选择概率与启发式权重。
        const bool sampled_light = random_double() < 0.5;
        const shared_ptr<pdf>& sampled_pdf = sampled_light ? light_pdf : srec.pdf_ptr;
        const shared_ptr<pdf>& other_pdf = sampled_light ? srec.pdf_ptr : light_pdf;

        ray scattered(rec.p, sampled_pdf->generate(), r.time());
        const double sampled_pdf_value = sampled_pdf->value(scattered.direction());
        if (!(sampled_pdf_value > 0) || !std::isfinite(sampled_pdf_value)) {
            statistics.complete_path(next_path_depth);
            return color_from_emission;
        }

        const double other_pdf_value = other_pdf->value(scattered.direction());
        constexpr double technique_probability = 0.5;
        const double weight = mis_weight(
            sampling_heuristic,
            technique_probability * sampled_pdf_value,
            technique_probability * other_pdf_value);
        if (!(weight > 0)) {
            statistics.complete_path(next_path_depth);
            return color_from_emission;
        }

        const double scattering_pdf = rec.mat->scattering_pdf(r, rec, scattered);
        if (!(scattering_pdf > 0) || !std::isfinite(scattering_pdf)) {
            statistics.complete_path(next_path_depth);
            return color_from_emission;
        }

        color scatter_weight = srec.attenuation * (
            scattering_pdf * weight
            / (technique_probability * sampled_pdf_value));
        color next_path_throughput = path_throughput * scatter_weight;
        if (!continue_after_russian_roulette(
                next_path_depth,
                scatter_weight,
                next_path_throughput,
                statistics)) {
            return color_from_emission;
        }

        const color sample_color = ray_color(
            scattered,
            depth - 1,
            next_path_depth,
            next_path_throughput,
            world,
            lights,
            statistics);
        const color color_from_scatter = scatter_weight * sample_color;

        return color_from_emission + color_from_scatter;
    }
};

#endif
