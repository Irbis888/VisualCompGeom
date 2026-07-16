#include "raylib.h"

#include "GeometryScene.h"
#include "algorithm_registry.h"
#include "geometry_3d_visualizer.h"
#include "input_register.h"

#include <algorithm>
#include <cctype>
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

struct ProgressBarLayout {
    int x = 32;
    int y = 0;
    int width = 1;
    int height = 8;
};

ProgressBarLayout CurrentProgressBarLayout()
{
    return {
        32,
        GetScreenHeight() - 34,
        std::max(1, GetScreenWidth() - 64),
        8
    };
}

struct SeedBoxLayout {
    Rectangle bounds{82.0F, 186.0F, 245.0F, 24.0F};
};

SeedBoxLayout CurrentSeedBoxLayout()
{
    return {};
}

bool PointInSeedBox(Vector2 point, const SeedBoxLayout& box)
{
    return CheckCollisionPointRec(point, box.bounds);
}

void ApplySeedTextInput(std::string& seedText, const ApplicationActions& actions)
{
    constexpr std::size_t MaxSeedTextLength = 32;
    for (const char ch : actions.textInput) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (!std::isspace(byte) && seedText.size() < MaxSeedTextLength) {
            seedText.push_back(ch);
        }
    }
    if (actions.textBackspace && !seedText.empty()) {
        seedText.pop_back();
    }
    if (actions.textDelete) {
        seedText.clear();
    }
}

bool PointInProgressBar(Vector2 point, const ProgressBarLayout& bar)
{
    constexpr float verticalGrabPadding = 12.0F;
    return point.x >= static_cast<float>(bar.x) &&
        point.x <= static_cast<float>(bar.x + bar.width) &&
        point.y >= static_cast<float>(bar.y) - verticalGrabPadding &&
        point.y <= static_cast<float>(bar.y + bar.height) + verticalGrabPadding;
}

void SeekPlaybackFromProgressBar(
    Playback& playback,
    const GeometryScene& scene,
    Vector2 point,
    const ProgressBarLayout& bar)
{
    playback.playing = false;
    playback.elapsed = 0.0F;

    if (scene.timeline.empty()) {
        playback.visibleEvents = 0;
        return;
    }

    const float rawProgress =
        (point.x - static_cast<float>(bar.x)) / static_cast<float>(bar.width);
    const float progress = std::clamp(rawProgress, 0.0F, 1.0F);
    playback.visibleEvents = static_cast<std::size_t>(
        std::llround(progress * static_cast<float>(scene.timeline.size())));
}

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
    const auto removeEdge = [&state](const SceneEdge& edgeToRemove) {
        const auto position = std::find_if(state.edges.begin(), state.edges.end(),
            [&edgeToRemove](const SceneEdge& edge) { return SameSceneEdge(edge, edgeToRemove); });
        if (position != state.edges.end()) state.edges.erase(position);
    };
    const auto applyEdgeChange = [&state, &removeEdge](
        EdgeAction action,
        const SceneEdge& edge,
        const SceneEdge& replacementEdge) {
        if (action == EdgeAction::Add) {
            state.edges.push_back(edge);
        }
        else if (action == EdgeAction::Remove) {
            removeEdge(edge);
        }
        else if (action == EdgeAction::Replace) {
            removeEdge(edge);
            state.edges.push_back(replacementEdge);
        }
    };
    const auto applyPointChange = [&state](
        PointAction action,
        std::size_t pointIndex,
        Point2 point,
        ScenePointStyle style) {
        if (pointIndex >= state.points.size()) return;
        if (action == PointAction::Show) {
            state.points[pointIndex].visible = true;
            state.points[pointIndex].position = point;
            state.points[pointIndex].style = style;
        }
        else if (action == PointAction::Hide) {
            state.points[pointIndex].visible = false;
        }
        else if (action == PointAction::Move) {
            state.points[pointIndex].position = point;
        }
        else if (action == PointAction::Restyle) {
            state.points[pointIndex].style = style;
        }
    };

    for (std::size_t i = 0; i < visible; ++i) {
        const TimelineEvent& event = scene.timeline[i];
        switch (event.kind) {
        case TimelineEventKind::Edge:
            applyEdgeChange(event.action, event.edge, event.replacementEdge);
            break;
        case TimelineEventKind::Point:
            applyPointChange(event.pointAction, event.pointIndex, event.point, event.pointStyle);
            break;
        case TimelineEventKind::SweepLine:
            state.sweepLine = event.sweepLine;
            break;
        case TimelineEventKind::ParametricCurve:
            state.parametricCurve = event.parametricCurve;
            break;
        }

        for (const TimelineEdgeChange& change : event.extraEdgeChanges) {
            applyEdgeChange(change.action, change.edge, change.replacementEdge);
        }
        for (const TimelinePointChange& change : event.extraPointChanges) {
            applyPointChange(change.action, change.pointIndex, change.point, change.style);
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
        if (newest.kind == TimelineEventKind::Edge &&
            (newest.action == EdgeAction::Add || newest.action == EdgeAction::Replace)) {
            const SceneEdge pulseEdge = newest.action == EdgeAction::Replace
                ? newest.replacementEdge
                : newest.edge;
            const float pulse = 4.5F + 0.8F * std::sin(static_cast<float>(GetTime()) * 5.0F);
            DrawSceneEdge(state, pulseEdge, view, pulse);
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

bool IsEditablePointIndex(const GeometryScene& scene, int pointIndex)
{
    return pointIndex >= 0 &&
        static_cast<std::size_t>(pointIndex) < EditablePointCount(scene);
}

void RunAlgorithmOnInputPoints(
    const AlgorithmDefinition& algorithm,
    const std::vector<Point2>& inputPoints,
    const AlgorithmRunOptions& options,
    GeometryScene& scene,
    std::string& status)
{
    AlgorithmVisualization visualization = algorithm.run(inputPoints, options);
    scene = std::move(visualization.scene);
    status = std::move(visualization.status);
}

void RunAlgorithm(
    const AlgorithmDefinition& algorithm,
    const AlgorithmRunOptions& options,
    GeometryScene& scene,
    std::string& status)
{
    RunAlgorithmOnInputPoints(algorithm, EditablePoints(scene), options, scene, status);
}

bool LoadAlgorithmInput(
    const AlgorithmDefinition& algorithm,
    const AlgorithmRunOptions& options,
    GeometryScene& scene,
    std::string& status)
{
    try {
        scene.points = LoadPointsFromFile(algorithm.inputFile);
        RunAlgorithm(algorithm, options, scene, status);
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

void RestorePlaybackAfterInputEdit(
    Playback& playback,
    std::size_t oldVisibleEvents,
    std::size_t oldTimelineSize,
    const GeometryScene& scene)
{
    playback.playing = false;
    playback.direction = 1;
    playback.elapsed = 0.0F;

    if (oldTimelineSize > 0 && oldVisibleEvents >= oldTimelineSize) {
        playback.visibleEvents = scene.timeline.size();
        return;
    }

    playback.visibleEvents = std::min(oldVisibleEvents, scene.timeline.size());
}

void RerunAfterInputEdit(
    const AlgorithmDefinition& algorithm,
    const std::vector<Point2>& inputPoints,
    const AlgorithmRunOptions& options,
    GeometryScene& scene,
    std::string& status,
    Playback& playback,
    std::size_t oldVisibleEvents,
    std::size_t oldTimelineSize)
{
    RunAlgorithmOnInputPoints(algorithm, inputPoints, options, scene, status);
    RestorePlaybackAfterInputEdit(playback, oldVisibleEvents, oldTimelineSize, scene);
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

    bool hasPoint = false;
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;

    const auto includePoint = [&](Point2 point) {
        if (!hasPoint) {
            minX = maxX = point.x;
            minY = maxY = point.y;
            hasPoint = true;
            return;
        }
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    };

    if (!scene.fitPointIndices.empty()) {
        for (const std::size_t index : scene.fitPointIndices) {
            if (index < scene.points.size()) includePoint(scene.points[index]);
        }
    }
    else {
        for (const Point2 point : scene.points) includePoint(point);
    }

    if (!hasPoint) {
        view = {};
        return;
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
    std::size_t algorithmCount,
    const std::string& seedText,
    bool seedBoxFocused)
{
    const int panelWidth = std::min(GetScreenWidth() - 28, 790);
    DrawRectangle(14, 14, panelWidth, 220, Fade(RAYWHITE, 0.94F));
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

    DrawText("Seed", 28, 190, 15, GRAY);
    const SeedBoxLayout seedBox = CurrentSeedBoxLayout();
    DrawRectangleRec(seedBox.bounds, Fade(RAYWHITE, 0.98F));
    DrawRectangleLinesEx(seedBox.bounds, seedBoxFocused ? 2.0F : 1.0F,
        seedBoxFocused ? ORANGE : GRAY);
    const std::string shownSeed = seedText.empty() ? "<random>" : seedText;
    DrawText(shownSeed.c_str(), static_cast<int>(seedBox.bounds.x + 8.0F),
        static_cast<int>(seedBox.bounds.y + 5.0F), 15,
        seedText.empty() ? GRAY : DARKGRAY);
    DrawText("empty = reshuffle every run", 336, 190, 15, GRAY);

    const ProgressBarLayout bar = CurrentProgressBarLayout();
    DrawRectangle(bar.x, bar.y, bar.width, bar.height, Color{205, 211, 219, 255});
    const float progress = scene.timeline.empty()
        ? 0.0F
        : static_cast<float>(playback.visibleEvents) / static_cast<float>(scene.timeline.size());
    DrawRectangle(bar.x, bar.y, static_cast<int>(bar.width * progress), bar.height,
        Color{218, 67, 78, 255});
    DrawFPS(GetScreenWidth() - 95, 16);
}

bool RunSmokeChecks(const std::vector<AlgorithmDefinition>& algorithms)
{
    const AlgorithmRunOptions options;
    for (const AlgorithmDefinition& algorithm : algorithms) {
        if (algorithm.view == AlgorithmView::Workspace3D) {
            try {
                if (!algorithm.run3D) {
                    std::cerr << algorithm.name << " smoke test failed: missing 3D runner\n";
                    return false;
                }
                const AlgorithmVisualization3D visualization = algorithm.run3D({}, options);
                if (!visualization.succeeded || visualization.scene.vertices.empty()) {
                    std::cerr << algorithm.name << " smoke test failed: "
                        << visualization.status << '\n';
                    return false;
                }
            }
            catch (const std::exception& exception) {
                std::cerr << algorithm.name << " smoke test failed: " << exception.what() << '\n';
                return false;
            }
            continue;
        }
        try {
            const std::vector<Point2> points = LoadPointsFromFile(algorithm.inputFile);
            const AlgorithmVisualization visualization = algorithm.run(points, options);
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
    AlgorithmRunOptions runOptions;
    LoadAlgorithmInput(algorithms[activeAlgorithm], runOptions, scene, status);

    View2D view;
    FitViewToScene(scene, view);
    Playback playback;
    int selectedPoint = -1;
    std::size_t dragStartVisibleEvents = 0;
    std::size_t dragStartTimelineSize = 0;
    bool panning = false;
    bool scrubbingProgress = false;
    bool seedBoxFocused = false;

    while (true) {
        const InputRegister input = CollectInputRegister();
        if (input.closeRequested) break;
        const ApplicationActions actions = MapInputToApplicationActions(input);

        if (!seedBoxFocused) {
            std::size_t requestedAlgorithm = activeAlgorithm;
            if (actions.nextAlgorithm) {
                requestedAlgorithm = (activeAlgorithm + 1) % algorithms.size();
            }
            if (actions.selectedAlgorithm >= 0 &&
                static_cast<std::size_t>(actions.selectedAlgorithm) < algorithms.size()) {
                requestedAlgorithm = static_cast<std::size_t>(actions.selectedAlgorithm);
            }
            if (requestedAlgorithm != activeAlgorithm) {
                if (algorithms[activeAlgorithm].view == AlgorithmView::Workspace3D) {
                    Deactivate3DVisualizer();
                }
                activeAlgorithm = requestedAlgorithm;
                selectedPoint = -1;
                panning = false;
                scrubbingProgress = false;
                if (algorithms[activeAlgorithm].view == AlgorithmView::Timeline2D) {
                    LoadAlgorithmInput(algorithms[activeAlgorithm], runOptions, scene, status);
                    RestartPlayback(playback);
                    FitViewToScene(scene, view);
                }
                else if (algorithms[activeAlgorithm].run3D) {
                    Set3DVisualization(
                        algorithms[activeAlgorithm].run3D({}, runOptions));
                }
                else {
                    AlgorithmVisualization3D visualization;
                    visualization.status =
                        "The selected 3D tab has no registered API runner.";
                    Set3DVisualization(std::move(visualization));
                }
            }
        }

        if (algorithms[activeAlgorithm].view == AlgorithmView::Workspace3D) {
            Draw3DVisualizerFrame();
            if (smokeTest) break;
            continue;
        }

        const Vector2 mouse = actions.pointerPosition;
        const ProgressBarLayout progressBar = CurrentProgressBarLayout();
        const SeedBoxLayout seedBox = CurrentSeedBoxLayout();
        const bool mouseOverProgressBar = PointInProgressBar(mouse, progressBar);
        const bool mouseOverSeedBox = PointInSeedBox(mouse, seedBox);
        int hoveredPoint = PointUnderMouse(scene, view, playback, mouse);

        if (seedBoxFocused) {
            ApplySeedTextInput(runOptions.randomSeed, actions);
            if (actions.textCancel) {
                seedBoxFocused = false;
            }
            if (actions.textConfirm) {
                seedBoxFocused = false;
                RunAlgorithm(algorithms[activeAlgorithm], runOptions, scene, status);
                RestartPlayback(playback);
            }
        }

        const float wheel = actions.zoomDelta;
        if (wheel != 0.0F) {
            const Point2 worldBeforeZoom = ScreenToWorld(mouse, view);
            view.zoom = std::clamp(view.zoom * std::pow(1.15F, wheel), 0.1F, 80.0F);
            const Point2 worldAfterZoom = ScreenToWorld(mouse, view);
            view.center.x += worldBeforeZoom.x - worldAfterZoom.x;
            view.center.y += worldBeforeZoom.y - worldAfterZoom.y;
        }

        if (actions.secondaryPressed) {
            if (IsEditablePointIndex(scene, hoveredPoint)) {
                const std::size_t oldVisibleEvents = playback.visibleEvents;
                const std::size_t oldTimelineSize = scene.timeline.size();
                std::vector<Point2> points = EditablePoints(scene);
                points.erase(points.begin() + hoveredPoint);
                RerunAfterInputEdit(algorithms[activeAlgorithm], points, runOptions, scene, status,
                    playback, oldVisibleEvents, oldTimelineSize);
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
            const bool hoveredEditablePoint = IsEditablePointIndex(scene, hoveredPoint);
            if (mouseOverSeedBox) {
                seedBoxFocused = true;
                selectedPoint = -1;
                panning = false;
                scrubbingProgress = false;
            }
            else {
                seedBoxFocused = false;
            }
            if (seedBoxFocused) {
                hoveredPoint = -1;
            }
            else if (!actions.addPointModifier && mouseOverProgressBar) {
                scrubbingProgress = true;
                selectedPoint = -1;
                SeekPlaybackFromProgressBar(playback, scene, mouse, progressBar);
                hoveredPoint = -1;
            }
            else if (actions.addPointModifier && !hoveredEditablePoint) {
                const std::size_t oldVisibleEvents = playback.visibleEvents;
                const std::size_t oldTimelineSize = scene.timeline.size();
                std::vector<Point2> points = EditablePoints(scene);
                points.push_back(SnapToInteger(ScreenToWorld(mouse, view)));
                RerunAfterInputEdit(algorithms[activeAlgorithm], points, runOptions, scene, status,
                    playback, oldVisibleEvents, oldTimelineSize);
                hoveredPoint = -1;
            }
            else {
                selectedPoint = hoveredEditablePoint ? hoveredPoint : -1;
                if (selectedPoint >= 0) {
                    dragStartVisibleEvents = playback.visibleEvents;
                    dragStartTimelineSize = scene.timeline.size();
                    playback.playing = false;
                }
            }
        }
        if (scrubbingProgress && actions.primaryDown) {
            SeekPlaybackFromProgressBar(playback, scene, mouse, progressBar);
        }
        if (selectedPoint >= 0 && actions.primaryDown) {
            scene.points[static_cast<std::size_t>(selectedPoint)] =
                SnapToInteger(ScreenToWorld(mouse, view));
            playback.playing = false;
            playback.visibleEvents = 0;
        }
        if (actions.primaryReleased) {
            scrubbingProgress = false;
            if (selectedPoint >= 0) {
                scene.points[static_cast<std::size_t>(selectedPoint)] =
                    SnapToInteger(ScreenToWorld(mouse, view));
                RerunAfterInputEdit(algorithms[activeAlgorithm], EditablePoints(scene), runOptions,
                    scene, status, playback, dragStartVisibleEvents, dragStartTimelineSize);
            }
            selectedPoint = -1;
        }

        if (!seedBoxFocused) {
            if (actions.load) {
                LoadAlgorithmInput(algorithms[activeAlgorithm], runOptions, scene, status);
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
                RunAlgorithm(algorithms[activeAlgorithm], runOptions, scene, status);
                RestartPlayback(playback);
            }
            if (actions.runAlgorithm) {
                RunAlgorithm(algorithms[activeAlgorithm], runOptions, scene, status);
                RestartPlayback(playback);
            }
            if (actions.fitView) FitViewToScene(scene, view);

            UpdatePlayback(playback, scene, actions);
        }

        BeginDrawing();
        ClearBackground(Color{245, 247, 250, 255});
        DrawGrid(view);
        DrawScene(scene, view, playback, selectedPoint, hoveredPoint);
        DrawInterface(scene, playback, status, algorithms[activeAlgorithm],
            activeAlgorithm, algorithms.size(), runOptions.randomSeed, seedBoxFocused);
        EndDrawing();

        if (smokeTest) break;
    }

    CloseWindow();
    return 0;
}
