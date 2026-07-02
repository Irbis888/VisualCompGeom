# Raylib Computational Geometry Visualizer

A small C++17/Raylib starter for visualizing computational-geometry data.
Raylib 6.0 is downloaded into the local `build` directory by CMake; no global
Raylib installation or administrator access is needed.

The main framebuffer requests 4x MSAA before window creation, smoothing the
triangle-based line segments and point circles without a custom shader.

The build uses the Visual Studio C++ toolchain already installed on this
machine. The separate `C:\msys64` GCC installation is currently broken by a
DLL mismatch, but it is not needed for this project.

## Build and run

From PowerShell:

```powershell
cd C:\SPS\CompGraph\Fusep\MyCodeAttempt\RaylibGeometryVisualizer
.\build.ps1
.\run.ps1
```

For a non-interactive rendering-context check, run:

```powershell
.\build\Debug\geometry_visualizer.exe --smoke-test
```

The first build needs internet access to fetch Raylib. Later builds use the
downloaded local copy. If command-line downloading is slow, download the
[official Raylib 6.0 source archive](https://codeload.github.com/raysan5/raylib/zip/refs/tags/6.0)
in a browser, save it beside `CMakeLists.txt` as `raylib-6.0.zip`, and run
`build.ps1` again. CMake automatically prefers that local archive.

The application starts in **CPU Convex Hull** mode. Press `Tab`, `1`, or `2`
to switch between CPU Convex Hull and Polygon Triangulation. Each algorithm
loads and saves its own existing TXT input file.

Controls:

- `Space`: play/pause forward;
- `B`: play/pause backward;
- left/right arrows: single-step; `Home`/`End`: jump to either end;
- `R`: replay from the beginning; up/down arrows: change playback speed;
- left-drag a vertex; Shift+left-click empty space to add one;
- right-click a vertex to remove it; right-drag empty space to pan;
- `L`: reload `input.txt`; `S`: save the edited polygon to it; `C`: clear;
- `Enter`: rerun the algorithm; `F`: fit the polygon in the window;
- `Tab` or number keys: switch the active algorithm;
- mouse wheel: zoom; `Escape`: close.

CPU Convex Hull uses `MyCodeAttempt/CompGeomAlgos/input.txt`; Polygon
Triangulation uses `MyCodeAttempt/Triangulation/input.txt`. Both use a point
count followed by `x y` pairs. Triangulation additionally interprets those
points as a counterclockwise polygon boundary.

## Extending it

`GeometryScene` deliberately separates geometry data from drawing. The
renderer only understands points, persistent edges, and generic add/remove
edge events. Future algorithms can produce a different timeline without
changing the camera, input, drawing, or playback code.

`TriangulationAPI` and `ConvexAPI` each construct their complete `Result`,
including the generic visualization timeline. `algorithm_registry.cpp` is the
only application file that knows which APIs exist; `main.cpp` contains only
generic visualization, input, file I/O, and playback behavior.

The view uses standard mathematical coordinates: positive X points right and
positive Y points up. Conversion to Raylib's screen coordinates happens only
in `WorldToScreen` and `ScreenToWorld`.
