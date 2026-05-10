# Assignment 2 — GLSL Shaders

A textured 3D mesh (Stanford bunny) rendered with a custom GLSL vertex/fragment shader pair. Supports two surface modes: opaque Blinn-Phong and a glass-like transparent material.

## Implementation

### `vertex_shader.glsl`

- Computes world-space `FragPos` and `Normal` (the latter using `transpose(inverse(model))` so non-uniform scaling stays correct).
- Outputs `gl_Position` from the standard `projection * view * model * position` chain.

### `fragment_shader.glsl`

- **Blinn-Phong shading** (`computePhongScalar`): ambient + Lambert diffuse + Blinn-Phong half-vector specular, with hard-coded strengths (`shininess=32`, `specularStrength=0.5`, etc.).
- Final colour is `(ambient + diffuse + specular) * albedo`, where `albedo` is sampled from `diffuseMap`.
- **Transparent mode** (`uTransparent != 0`): alpha is blended from `alphaBase=0.25` toward `1.0` using a Schlick-Fresnel term `F = F0 + (1-F0)(1-cosθ)^5`, so silhouettes go opaque and grazing-angle facets stay see-through (glass look).

## Assets

- `bunny.obj` + `bunny-atlas.jpg` — Stanford bunny mesh and its diffuse texture atlas.
- `tiny_obj_loader.h`, `stb_image.h` — single-header loaders for OBJ / images.
- `Camera.*`, `Mesh.*`, `MainFrame.*`, `Main.cpp` — host-side OpenGL setup, mesh upload, uniform binding, and main loop.

Demo: `demo.mp4`.

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
./Main     # macOS/Linux; on Windows open Assignment2.sln
```

CMake uses GLFW / GLAD / GLM bundled under `thirdparty/`.
