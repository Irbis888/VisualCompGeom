#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

struct Point2 {
    double x;
    double y;
};

struct Edge2 {
    std::size_t first;
    std::size_t second;
};

enum class EdgeLayer {
    Input,
    Intermediate,
    Result
};

struct SceneEdge {
    Edge2 edge;
    EdgeLayer layer = EdgeLayer::Input;
};

enum class EdgeAction {
    Add,
    Remove,
    Replace
};

struct SceneColor {
    unsigned char r = 23;
    unsigned char g = 49;
    unsigned char b = 73;
    unsigned char a = 255;
};

struct ScenePointStyle {
    double radius = 6.0;
    SceneColor color{};
};

enum class PointAction {
    Show,
    Hide,
    Move,
    Restyle
};

struct SweepLineState {
    bool visible = false;
    double y = 0.0;
    SceneColor color{96, 125, 139, 210};
    double thickness = 2.5;
};

struct ParametricCurveState {
    bool visible = false;
    std::vector<Point2> samples;
    SceneColor color{126, 87, 194, 230};
    double thickness = 3.0;
};

enum class TimelineEventKind {
    Edge,
    Point,
    SweepLine,
    ParametricCurve
};

struct TimelineEdgeChange {
    EdgeAction action = EdgeAction::Add;
    SceneEdge edge;
    SceneEdge replacementEdge;
};

struct TimelinePointChange {
    PointAction action = PointAction::Show;
    std::size_t pointIndex = 0;
    Point2 point{};
    ScenePointStyle style{};
};

struct TimelineEvent {
    EdgeAction action = EdgeAction::Add;
    SceneEdge edge;
    std::string caption;
    TimelineEventKind kind = TimelineEventKind::Edge;
    PointAction pointAction = PointAction::Show;
    std::size_t pointIndex = 0;
    Point2 point{};
    ScenePointStyle pointStyle{};
    SweepLineState sweepLine{};
    ParametricCurveState parametricCurve{};
    SceneEdge replacementEdge;
    std::vector<TimelineEdgeChange> extraEdgeChanges;
    std::vector<TimelinePointChange> extraPointChanges;
};

// Algorithm-independent data consumed by the renderer.
struct GeometryScene {
    std::vector<Point2> points;
    std::vector<ScenePointStyle> pointStyles;
    std::vector<std::size_t> fitPointIndices;
    std::size_t initialVisiblePointCount = std::numeric_limits<std::size_t>::max();
    std::vector<SceneEdge> persistentEdges;
    SweepLineState persistentSweepLine;
    ParametricCurveState persistentParametricCurve;
    std::vector<TimelineEvent> timeline;
};

struct AlgorithmVisualization {
    GeometryScene scene;
    std::string status;
    bool succeeded = false;
};
