# VisualCompGeom

Current version: **0.0.2**

VisualCompGeom is a small C++17 application for interactively visualizing
computational-geometry algorithms. It uses [raylib](https://www.raylib.com/)
for rendering and keeps algorithm code separate from the renderer through a
shared `GeometryScene` event model.

The application currently visualizes:

- a CPU convex hull over an unordered point cloud;
- polygon monotonization and triangulation;
- Fortune's sweep-line algorithm for Voronoi diagrams;
- randomized incremental Delaunay triangulation with edge legalization.

Points can be edited directly in the window, algorithm operations can be
played forward or backward, and each algorithm has a persistent TXT input
file.

## Project status

The raylib visualizer and the CPU algorithms are functional and covered by a
smoke test. The Fortune Voronoi implementation currently builds raw Voronoi
edges and vertices, clips them to a viewport box, and visualizes the sweep; DCEL
face-cycle construction from the clipped raw edges is still future work. The
CUDA directories are experiments and are not registered in the visualizer.

The documented and tested build environment is Windows x64 with Visual Studio
2022 or 2026. The CMake project itself may be portable, but other operating
systems are not currently tested by this repository.

## Changelog

### 0.0.2

- Added live input updates for point add/move/delete operations. The
  visualizer now reruns the active algorithm from the editable input-site
  prefix instead of accidentally mixing generated algorithm vertices into the
  input.
- Added mouse control for the timeline progress bar: click to jump and drag to
  scrub through algorithm events.
- Added a seed textbox: empty means a fresh random rerun, while non-empty text
  fixes randomized algorithm order reproducibly.
- Improved Fortune Voronoi handling for degenerate cases:
  - same-height site insertion no longer performs an invalid
    `old, new, old` split;
  - simultaneous circle events at the same event point are processed together;
  - same-level circle events are accepted, fixing cases such as square corners
    plus a center site where the center cell should be a rhombus.
- Fortune Voronoi raw edges are updated online during the sweep and no longer
  get removed and redrawn as an identical final diagram.
- Fortune Voronoi clipping bounds now expand to include generated Voronoi
  vertices.
- Added the Delaunay Triangulation project with API, input file, standalone
  Visual Studio project, visualizer registration, and access to the shared DCEL
  structures.
- Implemented randomized incremental Delaunay triangulation with triangle/edge
  insertion, incircle-based edge legalization, one-step `Replace` flip events,
  and final super-triangle cleanup.

### 0.0.1

- Initial raylib visualizer with shared `GeometryScene` timeline model.
- Added CPU convex hull, polygon triangulation, and early Fortune Voronoi
  visualization.
- Added TXT input loading/saving and basic playback controls.

## Algorithms

### CPU Convex Hull

`CompGeomAlgos/CPUConvexHull.cpp` sorts an arbitrary point cloud by X and then
Y, constructs upper and lower chains, and uses the `Orient2D` predicate to
remove turns that cannot belong to the hull. The visualization records every
edge addition and removal, so the chain stack behavior can be replayed.

Input order is irrelevant. The algorithm uses integer coordinates internally.

### Polygon Triangulation

`Triangulation/Triangulation.cpp` first classifies polygon vertices and uses a
sweep-line status structure to insert diagonals that partition the polygon
into Y-monotone pieces. It then recursively inserts valid interior diagonals
until every piece is a triangle.

The input must be a simple counterclockwise polygon whose points are listed in
boundary order. Consecutive duplicate vertices are rejected. Integer
coordinates and distinct vertex heights are recommended for clear sweep-line
examples.

Orange edges are monotonization diagonals; red edges are triangulation
diagonals.

### Fortune Voronoi

`FortuneVoronoi/FortuneVoronoi.cpp` runs a top-to-bottom event sweep. Site
events split the symbolic beach-line tree and create raw Voronoi edges; circle
events create Voronoi vertices, finish the old raw edges, remove the
disappearing arc, and start the new edge. After the sweep, unfinished raw edges
are clipped to a viewport box and emitted as drawable segments.

The computation does not store parabolic arcs directly. Beach-line leaves store
site pointers, internal tree nodes store ordered breakpoint site-pairs, and the
sampled parametric curve is produced only for visualization. The standalone
Fortune project prints the raw vertices, raw-edge count, and clipped segments
to the console.

### Delaunay Triangulation

`DelaunayTriangulation/DelaunayTriangulation.cpp` implements randomized
incremental Delaunay triangulation. It starts from a super-triangle made from
the top input site and two artificial vertices, inserts the remaining sites in
a shuffled order, splits containing triangles or edges, and legalizes old edges
with incircle tests. Edge flips are recorded as single `Replace` timeline
actions. The final cleanup removes all artificial vertices and incident edges
in one timeline step, so rewinding one step shows the full super-triangle
context again. The result also exposes drawable edges and a basic shared-DCEL
half-edge structure through `DelaunayAPI.h`.

### CUDA directories

`literally nothing to see here, go on`

`CudaPlayground` is a minimal CUDA vector-addition experiment.
`CudaConvexHull` is also experimental: despite its directory name, it does not
yet implement a complete GPU convex hull and is not used by the raylib app.
CUDA and the CUDA Toolkit are therefore **not required** to build or use the
visualizer.

## Prerequisites

Install:

- Windows 10 or 11, x64;
- Visual Studio 2022 or 2026 with:
  - **Desktop development with C++**;
  - **C++ CMake tools for Windows**;
- PowerShell;
- an internet connection for the first build.

Raylib does not need to be installed globally. CMake downloads the pinned
Raylib 6.0 source into the local build directory and builds it together with
the application.

## Quick start

Clone the repository and enter the visualizer directory:

```powershell
git clone <repository-url>
cd VisualCompGeom\RaylibGeometryVisualizer
```

Build and run the Debug configuration:

```powershell
.\build.ps1
.\run.ps1
```

For an optimized build:

```powershell
.\build.ps1 -Configuration Release
.\run.ps1 -Configuration Release
```

The first build downloads approximately 50 MB of Raylib source. Later builds
reuse the local copy in `RaylibGeometryVisualizer/build`.

### Manual CMake build

The scripts are conveniences. The equivalent Visual Studio 2022 commands are:

```powershell
cmake -S RaylibGeometryVisualizer -B RaylibGeometryVisualizer\build `
  -G "Visual Studio 17 2022" -A x64
cmake --build RaylibGeometryVisualizer\build --config Debug
```

For Visual Studio 2026, use:

```powershell
cmake -S RaylibGeometryVisualizer -B RaylibGeometryVisualizer\build `
  -G "Visual Studio 18 2026" -A x64
cmake --build RaylibGeometryVisualizer\build --config Debug
```

Run the resulting executable:

```powershell
.\RaylibGeometryVisualizer\build\Debug\geometry_visualizer.exe
```

If CMake reports that the existing build directory uses a different
generator, remove `RaylibGeometryVisualizer/build` and configure again.

### Offline or slow Raylib download

Download the official
[Raylib 6.0 source archive](https://codeload.github.com/raysan5/raylib/zip/refs/tags/6.0)
in a browser and save it as:

```text
RaylibGeometryVisualizer/raylib-6.0.zip
```

CMake verifies the archive's SHA-256 hash and automatically prefers it over a
network download.

### Visual Studio debugging

Open `VisualCompGeom.sln` from the repository root. The tracked
`RaylibGeometryVisualizer` wrapper project invokes CMake automatically and
launches the correct Debug or Release executable.

On the first launch, right-click `RaylibGeometryVisualizer` in Solution
Explorer and choose **Set as Startup Project**, then press `F5`. The project is
listed first in a fresh solution and will normally already be selected.

Building the entire root solution also builds the CUDA project and therefore
requires CUDA 13.3. If CUDA is not installed, build or start only
`RaylibGeometryVisualizer`; the visualizer itself has no CUDA dependency.

The standalone `CompGeomAlgos/CPUConvexHull.slnx`,
`FortuneVoronoi/FortuneVoronoi.slnx`, `DelaunayTriangulation/DelaunayTriangulation.slnx`, and `CudaPlayground/CudaPlayground.slnx`
solutions remain available for focused console-project work.

## Using the application

The app starts in **CPU Convex Hull** mode.

### Algorithm selection

| Input | Action |
|---|---|
| `1` | Select CPU Convex Hull |
| `2` | Select Polygon Triangulation |
| `3` | Select Fortune Voronoi |
| `4` | Select Delaunay Triangulation |
| `Tab` | Select the next registered algorithm |

Switching algorithms automatically loads that algorithm's TXT input file,
recomputes its visualization, resets playback, and fits the view.

### Playback

| Input | Action |
|---|---|
| `Space` | Play or pause forward playback |
| `B` | Play or pause backward playback |
| `Left` / `Right` | Press to move one operation backward or forward; hold to scroll continuously |
| Mouse drag on progress bar | Jump/scrub to a timeline step |
| Seed textbox | For randomized algorithms, leave empty for a fresh random run or type a seed for repeatable reruns |
| `Home` / `End` | Jump to the beginning or end |
| `R` | Replay from the beginning |
| `Up` / `Down` | Increase or decrease playback speed |

### Editing and camera

| Input | Action |
|---|---|
| Left-drag a point | Move it; coordinates snap to integers |
| `Shift` + left-click empty space | Add a point |
| Right-click a point | Remove it |
| Right-drag empty space | Pan the camera |
| Mouse wheel | Zoom around the pointer |
| `F` | Fit all points in the window |
| `C` | Clear all points |
| `Enter` | Re-run the active algorithm |
| `Escape` | Close the application |

Editing a point automatically recomputes the active algorithm. The visualizer
preserves the current/end timeline position when practical, so live point
editing can be used to inspect how the result changes without manually
replaying from the beginning every time.

### Loading and saving input

| Input | Action |
|---|---|
| `L` | Reload the active algorithm's TXT file |
| `S` | Save the current points to that TXT file |

The files are:

- `CompGeomAlgos/input.txt` for CPU Convex Hull;
- `Triangulation/input.txt` for Polygon Triangulation;
- `FortuneVoronoi/input.txt` for Fortune Voronoi sites;
- `DelaunayTriangulation/input.txt` for Delaunay Triangulation sites.

Both use this format:

```text
<point-count>
<x0> <y0>
<x1> <y1>
...
```

Example:

```text
4
0 0
100 0
100 100
0 100
```

Saving rounds coordinates to integers. For triangulation, list the polygon
boundary counterclockwise; for convex hull, points may appear in any order.

## Architecture

```text
VisualCompGeom/
|-- VisualCompGeom.sln             root Visual Studio solution
|-- include/
|   |-- GeometryScene.h          shared rendering/event model
|   `-- Commons.h/.cpp           shared point predicates and TXT reader
|-- CompGeomAlgos/
|   |-- ConvexAPI.h
|   |-- CPUConvexHull.cpp
|   `-- input.txt
|-- Triangulation/
|   |-- TriangulationAPI.h
|   |-- Triangulation.cpp
|   `-- input.txt
|-- FortuneVoronoi/
|   |-- FortuneAPI.h
|   |-- FortuneVoronoi.cpp
|   |-- FortuneVoronoi.vcxproj / FortuneVoronoi.slnx
|   `-- input.txt
|-- DelaunayTriangulation/
|   |-- DelaunayAPI.h
|   |-- DelaunayTriangulation.cpp
|   |-- DelaunayTriangulation.vcxproj / DelaunayTriangulation.slnx
|   `-- input.txt
`-- RaylibGeometryVisualizer/
    |-- CMakeLists.txt
    |-- build.ps1 / run.ps1
    `-- src/
        |-- algorithm_registry.h/.cpp
        `-- main.cpp
```

`main.cpp` contains only generic drawing, camera, editing, TXT I/O, and
timeline playback. It does not include an algorithm API.

Each algorithm API returns an `AlgorithmVisualization` containing:

- `scene.points`: input points plus optional generated points, addressed by index;
- `scene.pointStyles`: optional per-point radius/color overrides;
- `scene.initialVisiblePointCount`: how many leading points are editable input points;
- `scene.persistentEdges`: edges visible for the entire timeline;
- `scene.persistentSweepLine` / `scene.persistentParametricCurve`: optional always-visible overlays;
- `scene.timeline`: ordered edge, point, sweep-line, and parametric-curve events with captions;
- `status`: a short result or error message;
- `succeeded`: whether the algorithm accepted the input.

The renderer applies timeline events from the beginning up to the selected
step. This makes forward stepping and rewinding deterministic without putting
algorithm-specific state in the renderer.

### Scene colors

| `EdgeLayer` | Default use | Color |
|---|---|---|
| `Input` | Input polygon or permanent edges | Blue |
| `Intermediate` | Temporary/intermediate construction | Orange |
| `Result` | Final-result construction | Red |

## Adding a new algorithm

The extension boundary is deliberately small. Do not add algorithm-specific
logic to `main.cpp`.

### 1. Create the algorithm API

Create a directory such as `MyAlgorithm` with `MyAlgorithmAPI.h`,
`MyAlgorithm.cpp`, and `input.txt`.

Minimal header:

```cpp
#pragma once

#include "GeometryScene.h"

#include <vector>

namespace my_algorithm {

struct Result {
    AlgorithmVisualization visualization;
    // Optional algorithm-specific output can be stored here too.
};

Result Run(const std::vector<Point2>& points);

} // namespace my_algorithm
```

### 2. Construct the visualization in the API

The APIвЂ”not the rendererвЂ”must decide which edges exist and when they change.

```cpp
my_algorithm::Result my_algorithm::Run(const std::vector<Point2>& points)
{
    Result result;
    GeometryScene& scene = result.visualization.scene;
    scene.points = points;
    scene.initialVisiblePointCount = points.size();

    // Always-visible edge between point 0 and point 1.
    scene.persistentEdges.push_back({{0, 1}, EdgeLayer::Input});

    // Add an edge at the next playback step.
    scene.timeline.push_back({
        EdgeAction::Add,
        {{1, 2}, EdgeLayer::Intermediate},
        "Add candidate edge"
    });

    // Remove the same edge at a later step.
    scene.timeline.push_back({
        EdgeAction::Remove,
        {{1, 2}, EdgeLayer::Intermediate},
        "Reject candidate edge"
    });

    // Add a final result edge.
    scene.timeline.push_back({
        EdgeAction::Add,
        {{0, 2}, EdgeLayer::Result},
        "Accept result edge"
    });

    // A generated point can be stored in scene.points and revealed later.
    const std::size_t generated = scene.points.size();
    scene.points.push_back({120.0, 80.0});
    TimelineEvent showPoint;
    showPoint.kind = TimelineEventKind::Point;
    showPoint.pointAction = PointAction::Show;
    showPoint.pointIndex = generated;
    showPoint.point = scene.points[generated];
    showPoint.pointStyle = ScenePointStyle{9.0, SceneColor{218, 67, 78, 255}};
    showPoint.caption = "Reveal generated vertex";
    scene.timeline.push_back(showPoint);

    // Overlays are timeline state, not real graph edges.
    TimelineEvent sweep;
    sweep.kind = TimelineEventKind::SweepLine;
    sweep.sweepLine.visible = true;
    sweep.sweepLine.y = 42.0;
    sweep.caption = "Move sweep line";
    scene.timeline.push_back(sweep);

    TimelineEvent curve;
    curve.kind = TimelineEventKind::ParametricCurve;
    curve.parametricCurve.visible = true;
    curve.parametricCurve.samples = {{0.0, 0.0}, {40.0, 25.0}, {80.0, 0.0}};
    curve.caption = "Draw sampled parametric curve";
    scene.timeline.push_back(curve);

    result.visualization.status = "Ready.";
    result.visualization.succeeded = true;
    return result;
}
```

Every edge endpoint is an index into `scene.points`. Never emit an out-of-range
index. Use the same `SceneEdge` and `EdgeLayer` for a matching add/remove pair.
For generated vertices, append them after the input points and set
`scene.initialVisiblePointCount = points.size()`. The visualizer will edit,
save, and re-run only those leading input points; generated points are display
artifacts controlled by timeline events.

If the input is invalid, preserve the points for display, set a useful status
message, leave `succeeded` false, and return without throwing when practical.

### 3. Add the source to CMake

In `RaylibGeometryVisualizer/CMakeLists.txt`:

```cmake
add_library(my_algorithm
    ../MyAlgorithm/MyAlgorithm.cpp
)
target_include_directories(my_algorithm PUBLIC
    ../MyAlgorithm
    ../include
)

target_compile_definitions(geometry_visualizer PRIVATE
    MY_ALGORITHM_INPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/../MyAlgorithm/input.txt"
)

target_link_libraries(geometry_visualizer PRIVATE my_algorithm)
```

If the source also contains a standalone console `main()`, guard it with a
compile definition, following `TRIANGULATION_NO_MAIN` and
`CPU_CONVEX_HULL_NO_MAIN` in the existing CMake file.

### 4. Register the algorithm

Include the API in `RaylibGeometryVisualizer/src/algorithm_registry.cpp`,
define a fallback input path, and add one descriptor:

```cpp
#include "MyAlgorithmAPI.h"

#ifndef MY_ALGORITHM_INPUT_FILE
#define MY_ALGORITHM_INPUT_FILE "input.txt"
#endif

// Inside AvailableAlgorithms():
{
    "My Algorithm",
    MY_ALGORITHM_INPUT_FILE,
    [](const std::vector<Point2>& points) {
        return my_algorithm::Run(points).visualization;
    }
}
```

Its position in `AvailableAlgorithms()` determines its number key. The first
registered algorithm is selected at startup. `Tab` automatically cycles
through every registered entry.

No change to `main.cpp` is required.

### 5. Test it

Rebuild and run the shared smoke test:

```powershell
cd RaylibGeometryVisualizer
.\build.ps1
.\build\Debug\geometry_visualizer.exe --smoke-test
```

The smoke test loads every registered input, requires successful API
execution, checks TXT round-tripping, and initializes a hidden Raylib/OpenGL
window. A timeline may be empty, which is useful for point-only
visualizations. It returns a non-zero exit code on failure.

## Troubleshooting

### Raylib cannot be downloaded

Use the local archive procedure under **Offline or slow Raylib download** and
confirm the file is named exactly `raylib-6.0.zip`.

### The triangulation input is rejected

Check that the polygon:

- has at least three points;
- is simple and does not self-intersect;
- is listed counterclockwise;
- has no consecutive duplicate points.

### The visualizer opens the wrong dataset

Each registered algorithm owns a separate input path. Switch to the desired
algorithm, then press `L`. Pressing `S` overwrites the input file shown in the
application panel.

### Lines look jagged

The app requests a 4x MSAA framebuffer before window creation. Support still
depends on the graphics driver; update the driver if multisampling is not
available.

## Publishing note

This repository currently has no root license file. Add an explicit license
before publishing if you want others to have clear permission to use, modify,
or redistribute the code.

