<div align="center">

# CPUPathTracer

**A multithreaded offline path tracer written in modern C++**

[English](README.md) | [简体中文](README_zh-CN.md)

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.16%2B-064F8C?logo=cmake&logoColor=white)
![OpenMP](https://img.shields.io/badge/OpenMP-enabled-009688)
![Renderer](https://img.shields.io/badge/renderer-CPU%20offline-6A5ACD)

<img src="docs/images/scene-final-showcase.png" alt="Final showcase rendered by CPUPathTracer" width="100%">

</div>

CPUPathTracer is a C++17 offline renderer built to explore the complete path-tracing pipeline on the CPU. It combines physically motivated light transport, importance sampling, hierarchical acceleration, textured OBJ assets, participating media, and deterministic OpenMP parallelism in a compact codebase.

## Highlights

- **Path tracing:** recursive indirect illumination, emissive geometry, one-sample multiple importance sampling, and Russian roulette termination.
- **Two-level acceleration:** binned SAH BVH for both scene objects and individual triangle meshes, with median splitting as an optional comparison mode.
- **OBJ asset pipeline:** OBJ/MTL loading, triangle intersection, generated smooth normals, smoothing groups, texture coordinates, and reusable model instances.
- **Surface detail:** image and procedural textures, bilinear filtering, normal mapping, bump mapping, and true vertex displacement.
- **Materials and media:** diffuse, metal, dielectric, emissive materials, homogeneous volumes, and anisotropic phase scattering.
- **CPU parallelism:** 16 × 16 tile scheduling with OpenMP, deterministic per-pixel random sampling, progressive PPM output, and a default limit of 80% of logical CPU threads.
- **Reproducible scenes:** three fixed scenes for baseline verification, focused feature inspection, and final integration testing.

## Gallery

<table>
  <tr>
    <td width="50%"><img src="docs/images/scene-reference-box.png" alt="Reference box scene"></td>
    <td width="50%"><img src="docs/images/scene-material-showcase.png" alt="Material and OBJ showcase scene"></td>
  </tr>
  <tr>
    <td align="center"><b>Scene 1 · Reference Box</b><br>Area lighting, indirect illumination, glass and diffuse surfaces</td>
    <td align="center"><b>Scene 2 · Material Showcase</b><br>OBJ assets, textures, surface detail and participating media</td>
  </tr>
</table>

<p align="center">
  <img src="docs/images/scene-final-showcase.png" alt="Final integrated showcase scene" width="100%">
  <br><b>Scene 3 · Final Showcase</b><br>
  Multiple emitters, mirrors, mixed materials, transformed meshes and two-level SAH BVH acceleration
</p>

## Rendering Pipeline

```text
Camera and deterministic pixel sampler
                │
                ▼
       OpenMP tile scheduler
                │
                ▼
 Recursive path integration ── MIS ── Russian roulette
                │
                ▼
      Scene SAH BVH (top level)
                │
                ▼
      Mesh SAH BVH (triangles)
                │
                ▼
 Progressive framebuffer → numbered PPM image
```

For every non-specular bounce, the integrator chooses either light sampling or material sampling with equal probability. The selected sample is then weighted using the configured balance or power heuristic. Russian roulette begins only after a configurable path depth, preserving an unbiased estimate while reducing work on low-throughput paths.

## Build and Run

### Requirements

- Windows 10/11
- CMake 3.16 or newer
- A C++17 compiler with OpenMP support
- MinGW-w64 GCC is recommended for the included VS Code configuration

### Command line

```powershell
git clone https://github.com/YeeeeeFun/CPUPathTracer.git
cd CPUPathTracer

cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
.\build\rttroyl.exe
```

Rendered images are written to `results/new5070-N.ppm`. The numeric suffix is selected automatically, so an existing result is never overwritten. The terminal displays build information, active acceleration and sampling modes, render progress, thread count, path statistics, and elapsed time.

### VS Code

Open the repository root in VS Code and select **Run Ray Tracing (Release + OpenMP)** from the Run and Debug panel. The provided task configures and builds the Release target before launch. If MinGW is installed elsewhere, update the tool paths in `.vscode/tasks.json` and `.vscode/launch.json`.

## Configuration

The commonly adjusted options are grouped in `render_config` at the top of [`main.cpp`](main.cpp):

| Option | Purpose |
| --- | --- |
| `scene_id` | Select scene `1`, `2`, or `3` |
| `image_width` | Output width; height follows the aspect ratio |
| `samples_per_pixel` | Samples per pixel; higher values reduce Monte Carlo noise |
| `max_depth` | Maximum path depth |
| `sampling_heuristic` | Select balance or power MIS weighting |
| `obj_bvh_method` / `world_bvh_method` | Select SAH or median BVH construction |
| `enable_world_bvh` | Enable top-level scene acceleration |
| `russian_roulette` | Enable termination and configure its start depth and survival range |
| `enable_showcase_fog` | Toggle the volume in Scene 2 |
| `enable_final_global_fog` | Toggle global fog in Scene 3 |
| `enable_final_ceiling_light` | Toggle the ceiling emitter in Scene 3 |
| `final_ceiling_light_intensity` | Adjust the ceiling emitter intensity |
| `final_sphere_light_intensity` | Adjust emissive sphere intensity |

The renderer uses at most 80% of available logical processors by default. To request fewer threads for a run:

```powershell
$env:RT_NUM_THREADS = "8"
.\build\rttroyl.exe
```

The environment variable can lower the thread count, but it cannot bypass the built-in 80% cap.

## Project Structure

```text
.
├── main.cpp                 # Render configuration and scene selection
├── scenes.h                 # Scene construction
├── camera.h                 # Camera, integrator and OpenMP render loop
├── bvh.h                    # Median and binned SAH BVH
├── triangle.h               # Triangle intersection and attributes
├── obj_model.*              # OBJ/MTL loading and mesh construction
├── material.h               # Surface materials and phase functions
├── texture.h                # Image and procedural textures
├── displacement.h           # Normal, bump and displacement utilities
├── constant_medium.h        # Homogeneous participating media
├── models/                  # Meshes and texture assets
├── external/                # Header-only third-party dependencies
└── docs/images/             # README render results
```

## Prerequisite Knowledge

These references are useful before reading the renderer implementation:

- [Software rasterization and triangle-mesh fundamentals](https://haqr.eu/tinyrenderer/)
- [Ray and path tracing fundamentals](https://raytracing.github.io/)

## Scope

This repository focuses on an offline CPU path tracer. It intentionally does not provide a real-time UI, GPU backend, denoiser, or a full production PBR material system. The small scope keeps the sampling, acceleration, geometry, material, and parallel-rendering code directly inspectable.

## Third-Party Components and Assets

- `tiny_obj_loader.h` is used for OBJ/MTL parsing.
- `stb_image.h` is used for image decoding.
- Asset attribution files are preserved in the corresponding model directories where provided.

