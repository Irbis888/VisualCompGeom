#pragma once

#include <cstddef>
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
    Remove
};

struct TimelineEvent {
    EdgeAction action = EdgeAction::Add;
    SceneEdge edge;
    std::string caption;
};

// Algorithm-independent data consumed by the renderer.
struct GeometryScene {
    std::vector<Point2> points;
    std::vector<SceneEdge> persistentEdges;
    std::vector<TimelineEvent> timeline;
};

struct AlgorithmVisualization {
    GeometryScene scene;
    std::string status;
    bool succeeded = false;
};

