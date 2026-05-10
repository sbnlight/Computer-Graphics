# Assignment 3 — Monte Carlo Path Tracer

A multi-threaded path-tracing renderer with stochastic global illumination. Reads a scene description from a `.toml` file and outputs a PNG via lodepng.

## Implementation

### `Hittable.cpp`

- **`Sphere::Hit`** — analytic ray–sphere intersection. Solves `at² + bt + c = 0`, picks the nearest root with `t > ε`, fills the `HitRecord` with position, normal (`(P − centre) / r`), and the reflection direction `d − 2(d·N)N`.
- **`Triangle::Hit`** — Möller–Trumbore intersection. When `phong_interpolation_` is on, the surface normal is barycentrically interpolated from per-vertex normals (`w·n_a + u·n_b + v·n_c`); otherwise it's the flat face normal `cross(edge1, edge2)`.

### `main.cpp`

- **`SampleHemisphere(normal)`** — uniform hemisphere sampling. Generates a direction in the local frame (`z = u₁`, `r = √(1 − z²)`, `φ = 2π·u₂`), then transforms to world space using a TBN basis built from the surface normal.
- **`Shade(...)`** — recursive shader implementing the rendering equation:
  - Adds ambient (`material.ambient · k_a`) and emission terms.
  - **Bonus task — perfect mirror reflection**: if `shininess < 0`, traces the reflected ray and returns `k_s · specular · L_reflected`.
  - **Diffuse term** (`k_d > 0`): one-sample Monte Carlo estimator of the indirect diffuse — `(diffuse / π) · k_d · L_i · cosθ / pdf` where `pdf = 1 / (2π)` for uniform hemisphere sampling.
  - **Glossy specular term** (`k_s > 0`): samples a Phong lobe around the perfect-reflection direction using `samplePhongLobe`, then evaluates the Phong BRDF `(s+2)/(2π) · cosα^s` divided by its matching PDF.
- **Renderer** — splits columns across `NUM_THREAD` (default 8) `std::thread` workers, accumulates `spp = 512` samples per pixel with sub-pixel jitter, then encodes the buffer to PNG.

## Configuration

Top of `main.cpp`:

```cpp
const int kMaxTraceDepth = 5;
const int spp           = 512;
const int NUM_THREAD    = 8;
const std::string scene_file  = "scene/teapot_area_light.toml";
const std::string output_path = "outputs/output.png";
```

`work_dir` is set to `"../"`, so paths resolve relative to the `build/` directory you run `./Main` from.

Scene files live under `scene/`; meshes under `mesh/`; rendered images under `outputs/`.

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
./Main
```

Render time scales with `spp × width × height × kMaxTraceDepth / NUM_THREAD`. Reduce `spp` for quick previews.
