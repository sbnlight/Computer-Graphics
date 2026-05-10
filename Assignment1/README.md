# Assignment 1 — OpenGL Basics

Two parts covering 2D drawing and 3D mesh manipulation in OpenGL.

## Part 1 — 2D Triangle Drawing

Interactive 2D triangle drawing program. Each left mouse click adds a vertex; every three clicks complete a filled triangle, drawn in a colour cycled from an 11-entry palette.

**Implemented in `Part1/code.cpp`:**

- `MouseInteraction(x, y)` — collects vertices into a temporary `Triangle` buffer, pushes a completed triangle into the `triangles` vector once three points are gathered, and resets the buffer.
- `DrawTriangles()` — renders all completed filled triangles, the in-progress vertex points, and a preview line once two of three vertices have been clicked.

Demo: `Part 1 Demo.mp4`.

## Part 2 — 3D Mesh Editor

Mouse-controlled 3D modelling program. Number keys `1`–`5` switch between modes:

| Key | Mode | What `LeftMouseMove` does |
|-----|------|---------------------------|
| `1` | Object rotation | Rotate around the mesh centre by an axis perpendicular to the screen-space drag vector (drag on screen → axis in world space via `Screen2World`). |
| `2` | Object translation | Cast a screen ray onto the mesh, project the drag onto the view plane at the hit's camera-space depth, translate by that vector (camera-forward component removed). |
| `3` | Face subdivision | Click on a face to subdivide it (handled in `LeftMouseClick`). |
| `4` | Face extrusion | Find the closest point between the mouse ray and the face-normal extrusion ray, translate the picked face's vertices along the normal by that amount. |
| `5` | Object scaling | Exponential scale around the mesh centre based on vertical drag distance (`exp(dy * 0.01)`). |

Right-mouse drag rotates the camera; scroll zooms. Helper math (`CameraRight/Up/Forward`, `ComposeAroundPivot`, screen↔world conversions) lives in `MainFrame.cpp`.

Demo: `Part 2 Demo.mp4`.

## Build

Both parts use CMake + GLFW + GLAD (bundled in each part's `thirdparty/`).

```bash
cd Part1   # or Part2
mkdir build && cd build
cmake ..
cmake --build .
./Main     # macOS/Linux; on Windows open the generated .sln
```
