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

bool SameSceneEdge(const SceneEdge& first, const SceneEdge& second)
{
    if (first.layer != second.layer) return false;
    return (first.edge.first == second.edge.first && first.edge.second == second.edge.second) ||
        (first.edge.first == second.edge.second && first.edge.second == second.edge.first);
}

std::vector<SceneEdge> ResolveTimelineEdges(
    const GeometryScene& scene,
    std::size_t visibleEvents)
{
    std::vector<SceneEdge> edges;
    const std::size_t visible = std::min(visibleEvents, scene.timeline.size());
    for (std::size_t i = 0; i < visible; ++i) {
        const TimelineEvent& event = scene.timeline[i];
        if (event.action == EdgeAction::Add) {
            edges.push_back(event.edge);
        }
        else {
            const auto position = std::find_if(edges.begin(), edges.end(),
                [&event](const SceneEdge& edge) { return SameSceneEdge(edge, event.edge); });
            if (position != edges.end()) edges.erase(position);
        }
    }
    return edges;
}

void DrawSceneEdge(
    const GeometryScene& scene,
    const SceneEdge& sceneEdge,
    const View2D& view,
    float thickness)
{
    const Edge2 edge = sceneEdge.edge;
    if (edge.first >= scene.points.size() || edge.second >= scene.points.size()) return;
    DrawLineEx(
        WorldToScreen(scene.points[edge.first], view),
        WorldToScreen(scene.points[edge.second], view),
        thickness,
        LayerColor(sceneEdge.layer));
}

void DrawScene(
    const GeometryScene& scene,
    const View2D& view,
    const Playback& playback,
    int selectedPoint,
    int hoveredPoint)
{
    for (const SceneEdge& edge : scene.persistentEdges) {
        DrawSceneEdge(scene, edge, view, 3.0F);
    }

    const std::vector<SceneEdge> timelineEdges =
        ResolveTimelineEdges(scene, playback.visibleEvents);
    for (const SceneEdge& edge : timelineEdges) {
        DrawSceneEdge(scene, edge, view, 3.5F);
    }

    if (playback.visibleEvents > 0 && playback.visibleEvents <= scene.timeline.size()) {
        const TimelineEvent& newest = scene.timeline[playback.visibleEvents - 1];
        if (newest.action == EdgeAction::Add) {
            const float pulse = 4.5F + 0.8F * std::sin(static_cast<float>(GetTime()) * 5.0F);
            DrawSceneEdge(scene, newest.edge, view, pulse);
        }
    }

    for (std::size_t i = 0; i < scene.points.size(); ++i) {
        const Vector2 screen = WorldToScreen(scene.points[i], view);
        const bool selected = static_cast<int>(i) == selectedPoint;
        const bool hovered = static_cast<int>(i) == hoveredPoint;
        const float radius = selected ? 9.0F : (hovered ? 8.0F : 6.0F);
        const Color color = selected ? ORANGE : Color{23, 49, 73, 255};
        DrawCircleV(screen, radius, color);
        if (selected || hovered || scene.points.size() <= 25) {
            DrawText(TextFormat("%zu", i), static_cast<int>(screen.x + 9.0F),
                static_cast<int>(screen.y - 17.0F), 16, DARKGRAY);
        }
    }
}

int PointUnderMouse(const GeometryScene& scene, const View2D& view, Vector2 mouse)
{
    for (std::size_t i = 0; i < scene.points.size(); ++i) {
        if (CheckCollisionPointCircle(mouse, WorldToScreen(scene.points[i], view), 12.0F)) {
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

void RunAlgorithm(
    const AlgorithmDefinition& algorithm,
    GeometryScene& scene,
    std::string& status)
{
    AlgorithmVisualization visualization = algorithm.run(scene.points);
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
            if (!visualization.succeeded || visualization.scene.timeline.empty()) {
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
        int hoveredPoint = PointUnderMouse(scene, view, mouse);

        const float wheel = actions.zoomDelta;
        if (wheel != 0.0F) {
            const Point2 worldBeforeZoom = ScreenToWorld(mouse, view);
            view.zoom = std::clamp(view.zoom * std::pow(1.15F, wheel), 0.1F, 80.0F);
            const Point2 worldAfterZoom = ScreenToWorld(mouse, view);
            view.center.x += worldBeforeZoom.x - worldAfterZoom.x;
            view.center.y += worldBeforeZoom.y - worldAfterZoom.y;
        }

        if (actions.secondaryPressed) {
            if (hoveredPoint >= 0) {
                scene.points.erase(scene.points.begin() + hoveredPoint);
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
                scene.points.push_back(SnapToInteger(ScreenToWorld(mouse, view)));
                RunAlgorithm(algorithms[activeAlgorithm], scene, status);
                RestartPlayback(playback);
            }
            else {
                selectedPoint = hoveredPoint;
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
                SavePointsToFile(algorithms[activeAlgorithm].inputFile, scene.points);
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
