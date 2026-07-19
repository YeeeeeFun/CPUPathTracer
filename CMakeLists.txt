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

include(CTest)
if(BUILD_TESTING)
    add_executable(triangle_test tests/triangle_test.cpp triangle.h)
    target_include_directories(triangle_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME triangle_test COMMAND triangle_test)

    add_executable(bvh_test tests/bvh_test.cpp bvh.h triangle.h)
    target_include_directories(bvh_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME bvh_test COMMAND bvh_test)

    add_executable(texture_test tests/texture_test.cpp texture.h color.h)
    target_include_directories(texture_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME texture_test COMMAND texture_test)

    add_executable(pdf_test tests/pdf_test.cpp pdf.h)
    target_include_directories(pdf_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME pdf_test COMMAND pdf_test)

    add_executable(
        path_sampling_test
        tests/path_sampling_test.cpp
        path_sampling.h)
    target_include_directories(path_sampling_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME path_sampling_test COMMAND path_sampling_test)

    add_executable(
        participating_medium_test
        tests/participating_medium_test.cpp
        constant_medium.h
        material.h
        pdf.h)
    target_include_directories(
        participating_medium_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME participating_medium_test COMMAND participating_medium_test)

    add_executable(bump_test tests/bump_test.cpp material.h triangle.h texture.h)
    target_include_directories(bump_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME bump_test COMMAND bump_test)

    add_executable(
        displacement_test
        tests/displacement_test.cpp
        displacement.h
        texture.h)
    target_include_directories(displacement_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME displacement_test COMMAND displacement_test)

    add_executable(
        normal_map_test
        tests/normal_map_test.cpp
        material.h
        texture.h)
    target_include_directories(normal_map_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME normal_map_test COMMAND normal_map_test)

    add_executable(
        obj_model_test
        tests/obj_model_test.cpp
        obj_model.cpp
        rtw_stb_image.cpp)
    target_include_directories(obj_model_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(
        obj_model_test PRIVATE RT_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME obj_model_test COMMAND obj_model_test)
endif()
