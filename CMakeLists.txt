cmake_minimum_required(VERSION 3.16)

project(RayTracingTheRestOfYourLife)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)

add_executable(rttroyl
    main.cpp
    obj_model.cpp
    rtw_stb_image.cpp
    vec3.h
    color.h
    ray.h
    hittable.h
    sphere.h
    hittable_list.h
    rtweekend.h
    interval.h
    constant.h
    camera.h
    material.h
    aabb.h
    bvh.h
    texture.h
    displacement.h
    rtw_stb_image.h
    perlin.h
    quad.h
    constant_medium.h
    onb.h
    pdf.h
    path_sampling.h
    scenes.h
    triangle.h
    obj_model.h
    obj_instance.h
    external/tiny_obj_loader.h
)

target_link_libraries(rttroyl PRIVATE OpenMP::OpenMP_CXX)
target_compile_definitions(
    rttroyl PRIVATE
    RT_PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    RT_BUILD_TYPE="${CMAKE_BUILD_TYPE}")

