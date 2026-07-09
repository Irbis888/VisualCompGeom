#include "raylib.h"

#include "GeometryScene.h"
#include "algorithm_registry.h"
#include "input_register.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct View2D {
    Point2 center{0.0, 0.0};
    float zoom = 1.0F;
};

struct Playback {
    std::size_t visibleEvents = 0;
    bool playing = true;
    int direction = 1;
    float eventsPerSecond = 4.0F;
    float elapsed = 0.0F;
};

Vector2 WorldToScreen(Point2 world, const View2D& view)
{
    return {
        GetScreenWidth() * 0.5F + static_cast<float>(world.x - view.center.x) * view.zoom,
        GetScreenHeight() * 0.5F - static_cast<float>(world.y - view.center.y) * view.zoom
    };
}

Point2 ScreenToWorld(Vector2 screen, const View2D& view)
{
    return {
        view.center.x + (screen.x - GetScreenWidth() * 0.5F) / view.zoom,
        view.center.y - (screen.y - GetScreenHeight() * 0.5F) / view.zoom
    };
}

Point2 SnapToInteger(Point2 point)
{
    return {std::round(point.x), std::round(point.y)};
}

float ChooseGridStep(float zoom)
{
    const float roughStep = 90.0F / zoom;
    const float power = std::pow(10.0F, std::floor(std::log10(roughStep)));
    const float normalized = roughStep / power;
    if (normalized <= 2.0F) return 2.0F * power;
    if (normalized <= 5.0F) return 5.0F * power;
    return 10.0F * power;
}

void DrawGrid(const View2D& view)
{
    const Point2 bottomLeft = ScreenToWorld(
        {0.0F, static_cast<float>(GetScreenHeight())}, view);
    const Point2 topRight = ScreenToWorld(
        {static_cast<float>(GetScreenWidth()), 0.0F}, view);
    const float step = ChooseGridStep(view.zoom);

    const Color gridColor{218, 223, 230, 255};
    for (double x = std::floor(bottomLeft.x / step) * step; x <= topRight.x; x += step) {
        const float screenX = WorldToScreen({x, 0.0}, view).x;
        DrawLineV({screenX, 0.0F}, {screenX, static_cast<float>(GetScreenHeight())}, gridColor);
    }
    for (double y = std::floor(bottomLeft.y / step) * step; y <= topRight.y; y += step) {
        const float screenY = WorldToScreen({0.0, y}, view).y;
        DrawLineV({0.0F, screenY}, {static_cast<float>(GetScreenWidth()), screenY}, gridColor);
    }

    const Vector2 origin = WorldToScreen({0.0, 0.0}, view);
    DrawLineEx({origin.x, 0.0F}, {origin.x, static_cast<float>(GetScreenHeight())}, 2.0F, GRAY);
    DrawLineEx({0.0F, origin.y}, {static_cast<float>(GetScreenWidth()), origin.y}, 2.0F, GRAY);
}

Color LayerColor(EdgeLayer layer)
{
    switch (layer) {
    case EdgeLayer::Input:        return Color{35, 88, 135, 255};
    case EdgeLayer::Intermediate: return Color{232, 139, 45, 255};
    case EdgeLayer::Result:       return Color{218, 67, 78, 255};
    }
    return BLACK;
}

Color ToRaylibColor(SceneColor color)
{
    return Color{color.r, color.g, color.b, color.a};
}

bool SameSceneEdge(const SceneEdge& first, const SceneEdge& second)
{
    if (first.layer != second.layer) return false;
    return (first.edge.first == second.edge.first && first.edge.second == second.edge.second) ||
        (first.edge.first == second.edge.second && first.edge.second == second.edge.first);
}

struct VisualPoint {
    Point2 position{};
    ScenePointStyle style{};
    bool visible = false;
};

struct ResolvedTimelineState {
    std::vector<VisualPoint> points;
    std::vector<SceneEdge> edges;
    SweepLineState sweepLine;
    ParametricCurveState parametricCurve;
};

ScenePointStyle PointStyleAt(const GeometryScene& scene, std::size_t index)
{
    if (index < scene.pointStyles.size()) return scene.pointStyles[index];
    return {};
}

ResolvedTimelineState ResolveTimelineState(
    const GeometryScene& scene,
    std::size_t visibleEvents)
{
    ResolvedTimelineState state;
    state.points.reserve(scene.points.size());

    const std::size_t initiallyVisible =
        scene.initialVisiblePointCount == std::numeric_limits<std::size_t>::max()
            ? scene.points.size()
            : std::min(scene.initialVisiblePointCount, scene.points.size());

    for (std::size_t i = 0; i < scene.points.size(); ++i) {
        state.points.push_back({
            scene.points[i],
            PointStyleAt(scene, i),
            i < initiallyVisible
        });
    }

    state.sweepLine = scene.persistentSweepLine;
    state.parametricCurve = scene.persistentParametricCurve;

    const std::size_t visible = std::min(visibleEvents, scene.timeline.size());
    for (std::size_t i = 0; i < visible; ++i) {
        const TimelineEvent& event = scene.timeline[i];
        switch (event.kind) {
        case TimelineEventKind::Edge:
            if (event.action == EdgeAction::Add) {
                state.edges.push_back(event.edge);
            }
            else {
                const auto position = std::find_if(state.edges.begin(), state.edges.end(),
                    [&event](const SceneEdge& edge) { return SameSceneEdge(edge, event.edge); });
                if (position != state.edges.end()) state.edges.erase(position);
            }
            break;
        case TimelineEventKind::Point:
            if (event.pointIndex >= state.points.size()) break;
            if (event.pointAction == PointAction::Show) {
                state.points[event.pointIndex].visible = true;
                state.points[event.pointIndex].position = event.point;
                state.points[event.pointIndex].style = event.pointStyle;
            }
            else if (event.pointAction == PointAction::Hide) {
                state.points[event.pointIndex].visible = false;
            }
            else if (event.pointAction == PointAction::Move) {
                state.points[event.pointIndex].position = event.point;
            }
            else if (event.pointAction == PointAction::Restyle) {
                state.points[event.pointIndex].style = event.pointStyle;
            }
            break;
        case TimelineEventKind::SweepLine:
            state.sweepLine = event.sweepLine;
            break;
        case TimelineEventKind::ParametricCurve:
            state.parametricCurve = event.parametricCurve;
            break;
        }
    }
    return state;
}

void DrawSceneEdge(
    const ResolvedTimelineState& state,
    const SceneEdge& sceneEdge,
    const View2D& view,
    float thickness)
{
    const Edge2 edge = sceneEdge.edge;
    if (edge.first >= state.points.size() || edge.second >= state.points.size()) return;
    if (!state.points[edge.first].visible || !state.points[edge.second].visible) return;
    DrawLineEx(
        WorldToScreen(state.points[edge.first].position, view),
        WorldToScreen(state.points[edge.second].position, view),
        thickness,
        LayerColor(sceneEdge.layer));
}

void DrawSweepLine(const SweepLineState& sweepLine, const View2D& view)
{
    if (!sweepLine.visible) return;
    const Point2 worldLeft = ScreenToWorld({0.0F, 0.0F}, view);
    const Point2 worldRight = ScreenToWorld({static_cast<float>(GetScreenWidth()), 0.0F}, view);
    DrawLineEx(
        WorldToScreen({worldLeft.x, sweepLine.y}, view),
        WorldToScreen({worldRight.x, sweepLine.y}, view),
        static_cast<float>(sweepLine.thickness),
        ToRaylibColor(sweepLine.color));
}

void DrawParametricCurve(const ParametricCurveState& curve, const View2D& view)
{
    if (!curve.visible || curve.samples.size() < 2) return;
    for (std::size_t i = 1; i < curve.samples.size(); ++i) {
        DrawLineEx(
            WorldToScreen(curve.samples[i - 1], view),
            WorldToScreen(curve.samples[i], view),
            static_cast<float>(curve.thickness),
            ToRaylibColor(curve.color));
    }
}

void DrawScene(
    const GeometryScene& scene,
    const View2D& view,
    const Playback& playback,
    int selectedPoint,
    int hoveredPoint)
{
    const ResolvedTimelineState state = ResolveTimelineState(scene, playback.visibleEvents);

    DrawSweepLine(state.sweepLine, view);
    DrawParametricCurve(state.parametricCurve, view);

    for (const SceneEdge& edge : scene.persistentEdges) {
        DrawSceneEdge(state, edge, view, 3.0F);
    }

    for (const SceneEdge& edge : state.edges) {
        DrawSceneEdge(state, edge, view, 3.5F);
    }

    if (playback.visibleEvents > 0 && playback.visibleEvents <= scene.timeline.size()) {
        const TimelineEvent& newest = scene.timeline[playback.visibleEvents - 1];
        if (newest.kind == TimelineEventKind::Edge && newest.action == EdgeAction::Add) {
            const float pulse = 4.5F + 0.8F * std::sin(static_cast<float>(GetTime()) * 5.0F);
            DrawSceneEdge(state, newest.edge, view, pulse);
        }
        else if (newest.kind == TimelineEventKind::Point &&
            newest.pointIndex < state.points.size() &&
            state.points[newest.pointIndex].visible) {
            const Vector2 screen = WorldToScreen(state.points[newest.pointIndex].position, view);
            const float pulse = static_cast<float>(state.points[newest.pointIndex].style.radius) +
                3.0F + 0.8F * std::sin(static_cast<float>(GetTime()) * 5.0F);
            DrawCircleLines(static_cast<int>(screen.x), static_cast<int>(screen.y), pulse, ORANGE);
        }
    }

    for (std::size_t i = 0; i < state.points.size(); ++i) {
        if (!state.points[i].visible) continue;
        const Vector2 screen = WorldToScreen(state.points[i].position, view);
        const bool selected = static_cast<int>(i) == selectedPoint;
        const bool hovered = static_cast<int>(i) == hoveredPoint;
        const float baseRadius = static_cast<float>(state.points[i].style.radius);
        const float radius = selected ? baseRadius + 3.0F : (hovered ? baseRadius + 2.0F : baseRadius);
        const Color color = selected ? ORANGE : ToRaylibColor(state.points[i].style.color);
        DrawCircleV(screen, radius, color);
        if (selected || hovered || state.points.size() <= 25) {
            DrawText(TextFormat("%zu", i), static_cast<int>(screen.x + 9.0F),
                static_cast<int>(screen.y - 17.0F), 16, DARKGRAY);
        }
    }
}

int PointUnderMouse(
    const GeometryScene& scene,
    const View2D& view,
    const Playback& playback,
    Vector2 mouse)
{
    const ResolvedTimelineState state = ResolveTimelineState(scene, playback.visibleEvents);
    for (std::size_t i = 0; i < state.points.size(); ++i) {
        if (!state.points[i].visible) continue;
        const float pickRadius = std::max(12.0F, static_cast<float>(state.points[i].style.radius) + 6.0F);
        if (CheckCollisionPointCircle(mouse, WorldToScreen(state.points[i].position, view), pickRadius)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::vector<Point2> LoadPointsFromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Failed to open " + path.string());

    std::size_t count = 0;
    if (!(input >> count)) throw std::runtime_error("Invalid point count in " + path.string());

    std::vector<Point2> points;
    points.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        Point2 point{};
        if (!(input >> point.x >> point.y)) {
            throw std::runtime_error("Invalid point data in " + path.string());
        }
        points.push_back(point);
    }
    return points;
}

void SavePointsToFile(const std::filesystem::path& path, const std::vector<Point2>& points)
{
    std::ofstream output(path, std::ios::trunc);
    if (!output) throw std::runtime_error("Failed to open " + path.string() + " for writing");

    output << points.size() << '\n';
    for (const Point2 point : points) {
        output << std::lround(point.x) << ' ' << std::lround(point.y) << '\n';
    }
    if (!output) throw std::runtime_error("Failed while writing " + path.string());
}

std::size_t EditablePointCount(const GeometryScene& scene)
{
    if (scene.initialVisiblePointCount == std::numeric_limits<std::size_t>::max()) {
        return scene.points.size();
    }
    return std::min(scene.initialVisiblePointCount, scene.points.size());
}

std::vector<Point2> EditablePoints(const GeometryScene& scene)
{
    const std::size_t count = EditablePointCount(scene);
    return std::vector<Point2>(scene.points.begin(), scene.points.begin() + count);
}

void RunAlgorithm(
    const AlgorithmDefinition& algorithm,
    GeometryScene& scene,
    std::string& status)
{
    AlgorithmVisualization visualization = algorithm.run(EditablePoints(scene));
    scene = std::move(visualization.scene);
    status = std::move(visualization.status);
}

bool LoadAlgorithmInput(
    const AlgorithmDefinition& algorithm,
    GeometryScene& scene,
    std::string& status)
{
    try {
        scene.points = LoadPointsFromFile(algorithm.inputFile);
        RunAlgorithm(algorithm, scene, status);
        return true;
    }
    catch (const std::exception& exception) {
        scene = {};
        status = std::string("Input error: ") + exception.what();
        return false;
    }
}

void RestartPlayback(Playback& playback)
{
    playback.visibleEvents = 0;
    playback.playing = true;
    playback.direction = 1;
    playback.elapsed = 0.0F;
}

void UpdatePlayback(
    Playback& playback,
    const GeometryScene& scene,
    const ApplicationActions& actions)
{
    if (actions.restartPlayback) RestartPlayback(playback);

    if (actions.toggleForwardPlayback) {
        if (!playback.playing && playback.visibleEvents == scene.timeline.size()) {
            playback.visibleEvents = 0;
        }
        playback.direction = 1;
        playback.playing = !playback.playing;
        playback.elapsed = 0.0F;
    }
    if (actions.toggleBackwardPlayback) {
        const bool wasRewinding = playback.playing && playback.direction < 0;
        if (!wasRewinding && playback.visibleEvents == 0) {
            playback.visibleEvents = scene.timeline.size();
        }
        playback.direction = -1;
        playback.playing = !wasRewinding;
        playback.elapsed = 0.0F;
    }
    if (actions.stepForward) {
        playback.playing = false;
        playback.visibleEvents = std::min(playback.visibleEvents + 1, scene.timeline.size());
    }
    if (actions.stepBackward) {
        playback.playing = false;
        if (playback.visibleEvents > 0) --playback.visibleEvents;
    }
    if (actions.jumpToStart) {
        playback.playing = false;
        playback.visibleEvents = 0;
    }
    if (actions.jumpToEnd) {
        playback.playing = false;
        playback.visibleEvents = scene.timeline.size();
    }
    if (actions.increaseSpeed) {
        playback.eventsPerSecond = std::min(playback.eventsPerSecond * 1.25F, 60.0F);
    }
    if (actions.decreaseSpeed) {
        playback.eventsPerSecond = std::max(playback.eventsPerSecond / 1.25F, 0.25F);
    }

    if (!playback.playing || scene.timeline.empty()) return;

    playback.elapsed += GetFrameTime();
    const float interval = 1.0F / playback.eventsPerSecond;
    while (playback.elapsed >= interval && playback.playing) {
        playback.elapsed -= interval;
        if (playback.direction > 0) {
            if (playback.visibleEvents < scene.timeline.size()) ++playback.visibleEvents;
            if (playback.visibleEvents == scene.timeline.size()) playback.playing = false;
        }
        else {
            if (playback.visibleEvents > 0) --playback.visibleEvents;
            if (playback.visibleEvents == 0) playback.playing = false;
        }
    }
}

void FitViewToScene(const GeometryScene& scene, View2D& view)
{
    if (scene.points.empty()) {
        view = {};
        return;
    }

    double minX = scene.points.front().x;
    double maxX = minX;
    double minY = scene.points.front().y;
    double maxY = minY;
    for (const Point2 point : scene.points) {
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    }

    view.center = {(minX + maxX) * 0.5, (minY + maxY) * 0.5};
    const double width = std::max(maxX - minX, 1.0);
    const double height = std::max(maxY - minY, 1.0);
    const float horizontalZoom = static_cast<float>((GetScreenWidth() - 120.0) / width);
    const float verticalZoom = static_cast<float>((GetScreenHeight() - 260.0) / height);
    view.zoom = std::clamp(std::min(horizontalZoom, verticalZoom), 0.1F, 80.0F);
}

std::string CurrentCaption(const GeometryScene& scene, const Playback& playback)
{
    if (scene.timeline.empty()) return "No algorithm timeline.";
    if (playback.visibleEvents == 0) return "Input points";
    const std::size_t index = std::min(playback.visibleEvents, scene.timeline.size()) - 1;
    return scene.timeline[index].caption;
}

void DrawInterface(
    const GeometryScene& scene,
    const Playback& playback,
    const std::string& status,
    const AlgorithmDefinition& algorithm,
    std::size_t algorithmIndex,
    std::size_t algorithmCount)
{
    const int panelWidth = std::min(GetScreenWidth() - 28, 790);
    DrawRectangle(14, 14, panelWidth, 178, Fade(RAYWHITE, 0.94F));
    DrawText(TextFormat("Geometry Visualizer - %s", algorithm.name.c_str()),
        28, 24, 24, Color{23, 49, 73, 255});
    DrawText(CurrentCaption(scene, playback).c_str(), 28, 55, 18, DARKGRAY);
    DrawText(status.c_str(), 28, 78, 16, GRAY);
    DrawText(TextFormat("Step %zu / %zu   Speed %.2f operations/s   %s",
        playback.visibleEvents, scene.timeline.size(), playback.eventsPerSecond,
        playback.playing ? (playback.direction > 0 ? "PLAYING" : "REWINDING") : "PAUSED"),
        28, 100, 18, DARKGRAY);
    DrawText("Space play | B rewind | arrows step | R replay | Up/Down speed", 28, 126, 16, GRAY);
    DrawText("Drag | Shift+click add | right-click remove | L load | S save | C clear | Enter run | F fit",
        28, 147, 15, GRAY);
    DrawText(TextFormat("Tab/1..%zu switch algorithm (%zu/%zu) | File: %s",
        algorithmCount, algorithmIndex + 1, algorithmCount,
        algorithm.inputFile.filename().string().c_str()), 28, 167, 15, GRAY);

    const int barX = 32;
    const int barWidth = std::max(1, GetScreenWidth() - 64);
    const int barY = GetScreenHeight() - 34;
    DrawRectangle(barX, barY, barWidth, 8, Color{205, 211, 219, 255});
    const float progress = scene.timeline.empty()
        ? 0.0F
        : static_cast<float>(playback.visibleEvents) / static_cast<float>(scene.timeline.size());
    DrawRectangle(barX, barY, static_cast<int>(barWidth * progress), 8,
        Color{218, 67, 78, 255});
    DrawFPS(GetScreenWidth() - 95, 16);
}

bool RunSmokeChecks(const std::vector<AlgorithmDefinition>& algorithms)
{
    for (const AlgorithmDefinition& algorithm : algorithms) {
        try {
            const std::vector<Point2> points = LoadPointsFromFile(algorithm.inputFile);
            const AlgorithmVisualization visualization = algorithm.run(points);
            if (!visualization.succeeded) {
                std::cerr << algorithm.name << " smoke test failed: "
                    << visualization.status << '\n';
                return false;
            }
        }
        catch (const std::exception& exception) {
            std::cerr << algorithm.name << " input test failed: " << exception.what() << '\n';
            return false;
        }
    }

    const std::filesystem::path roundTripPath =
        std::filesystem::current_path() / "geometry-input-smoke-test.txt";
    try {
        const std::vector<Point2> points = LoadPointsFromFile(algorithms.front().inputFile);
        SavePointsToFile(roundTripPath, points);
        const std::vector<Point2> loaded = LoadPointsFromFile(roundTripPath);
        std::filesystem::remove(roundTripPath);
        if (loaded.size() != points.size()) return false;
    }
    catch (const std::exception& exception) {
        std::cerr << "TXT round-trip failed: " << exception.what() << '\n';
        return false;
    }
    return true;
}

int main(int argc, char* argv[])
{
    const bool smokeTest = argc > 1 && std::string_view(argv[1]) == "--smoke-test";
    const std::vector<AlgorithmDefinition>& algorithms = AvailableAlgorithms();
    if (algorithms.empty()) {
        std::cerr << "No visualization algorithms are registered.\n";
        return 2;
    }
    if (smokeTest && !RunSmokeChecks(algorithms)) return 2;

    unsigned int windowFlags =
        FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT;
    if (smokeTest) windowFlags |= FLAG_WINDOW_HIDDEN;
    SetConfigFlags(windowFlags);
    InitWindow(1280, 720, "Computational Geometry Visualizer");
    SetTargetFPS(120);

    std::size_t activeAlgorithm = 0;
    GeometryScene scene;
    std::string status;
    LoadAlgorithmInput(algorithms[activeAlgorithm], scene, status);

    View2D view;
    FitViewToScene(scene, view);
    Playback playback;
    int selectedPoint = -1;
    bool panning = false;

    while (true) {
        const InputRegister input = CollectInputRegister();
        if (input.closeRequested) break;
        const ApplicationActions actions = MapInputToApplicationActions(input);
        const Vector2 mouse = actions.pointerPosition;
        int hoveredPoint = PointUnderMouse(scene, view, playback, mouse);

        const float wheel = actions.zoomDelta;
        if (wheel != 0.0F) {
            const Point2 worldBeforeZoom = ScreenToWorld(mouse, view);
            view.zoom = std::clamp(view.zoom * std::pow(1.15F, wheel), 0.1F, 80.0F);
            const Point2 worldAfterZoom = ScreenToWorld(mouse, view);
            view.center.x += worldBeforeZoom.x - worldAfterZoom.x;
            view.center.y += worldBeforeZoom.y - worldAfterZoom.y;
        }

        if (actions.secondaryPressed) {
            if (hoveredPoint >= 0 && static_cast<std::size_t>(hoveredPoint) < EditablePointCount(scene)) {
                std::vector<Point2> points = EditablePoints(scene);
                points.erase(points.begin() + hoveredPoint);
                scene.points = std::move(points);
                RunAlgorithm(algorithms[activeAlgorithm], scene, status);
                RestartPlayback(playback);
                hoveredPoint = -1;
            }
            else {
                panning = true;
            }
        }
        if (panning && actions.secondaryDown) {
            const Vector2 delta = actions.pointerDelta;
            view.center.x -= delta.x / view.zoom;
            view.center.y += delta.y / view.zoom;
        }
        if (actions.secondaryReleased) panning = false;

        if (actions.primaryPressed) {
            if (actions.addPointModifier && hoveredPoint < 0) {
                std::vector<Point2> points = EditablePoints(scene);
                points.push_back(SnapToInteger(ScreenToWorld(mouse, view)));
                scene.points = std::move(points);
                RunAlgorithm(algorithms[activeAlgorithm], scene, status);
                RestartPlayback(playback);
            }
            else {
                selectedPoint = (hoveredPoint >= 0 &&
                    static_cast<std::size_t>(hoveredPoint) < EditablePointCount(scene))
                        ? hoveredPoint
                        : -1;
                if (selectedPoint >= 0) playback.playing = false;
            }
        }
        if (selectedPoint >= 0 && actions.primaryDown) {
            scene.points[static_cast<std::size_t>(selectedPoint)] =
                SnapToInteger(ScreenToWorld(mouse, view));
        }
        if (actions.primaryReleased) {
            if (selectedPoint >= 0) {
                RunAlgorithm(algorithms[activeAlgorithm], scene, status);
                RestartPlayback(playback);
            }
            selectedPoint = -1;
        }

        std::size_t requestedAlgorithm = activeAlgorithm;
        if (actions.nextAlgorithm) {
            requestedAlgorithm = (activeAlgorithm + 1) % algorithms.size();
        }
        if (actions.selectedAlgorithm >= 0 &&
            static_cast<std::size_t>(actions.selectedAlgorithm) < algorithms.size()) {
            requestedAlgorithm = static_cast<std::size_t>(actions.selectedAlgorithm);
        }
        if (requestedAlgorithm != activeAlgorithm) {
            activeAlgorithm = requestedAlgorithm;
            LoadAlgorithmInput(algorithms[activeAlgorithm], scene, status);
            RestartPlayback(playback);
            FitViewToScene(scene, view);
        }

        if (actions.load) {
            LoadAlgorithmInput(algorithms[activeAlgorithm], scene, status);
            RestartPlayback(playback);
            FitViewToScene(scene, view);
        }
        if (actions.save) {
            try {
                SavePointsToFile(algorithms[activeAlgorithm].inputFile, EditablePoints(scene));
                status = "Saved " + algorithms[activeAlgorithm].inputFile.filename().string() + ".";
            }
            catch (const std::exception& exception) {
                status = std::string("Save error: ") + exception.what();
                playback.playing = false;
            }
        }
        if (actions.clear) {
            scene = {};
            RunAlgorithm(algorithms[activeAlgorithm], scene, status);
            RestartPlayback(playback);
        }
        if (actions.runAlgorithm) {
            RunAlgorithm(algorithms[activeAlgorithm], scene, status);
            RestartPlayback(playback);
        }
        if (actions.fitView) FitViewToScene(scene, view);

        UpdatePlayback(playback, scene, actions);

        BeginDrawing();
        ClearBackground(Color{245, 247, 250, 255});
        DrawGrid(view);
        DrawScene(scene, view, playback, selectedPoint, hoveredPoint);
        DrawInterface(scene, playback, status, algorithms[activeAlgorithm],
            activeAlgorithm, algorithms.size());
        EndDrawing();

        if (smokeTest) break;
    }

    CloseWindow();
    return 0;
}
