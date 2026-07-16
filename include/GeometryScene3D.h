#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

struct Point3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Edge3 {
    std::size_t first = 0;
    std::size_t second = 0;
};

struct Triangle3 {
    std::size_t first = 0;
    std::size_t second = 0;
    std::size_t third = 0;
};

enum class EdgeLayer3D {
    Input,
    Intermediate,
    Result
};

struct SceneColor3D {
    unsigned char r = 235;
    unsigned char g = 239;
    unsigned char b = 244;
    unsigned char a = 255;
};

struct SceneVertexStyle3D {
    double radius = 0.10;
    SceneColor3D color{};
};

struct SceneEdge3D {
    Edge3 edge;
    EdgeLayer3D layer = EdgeLayer3D::Input;
};

struct SceneTriangle3D {
    Triangle3 triangle;
    SceneColor3D color{79, 142, 213, 210};
};

enum class EdgeAction3D {
    Add,
    Remove,
    Replace
};

enum class VertexAction3D {
    Show,
    Hide,
    Move,
    Restyle
};

enum class TriangleAction3D {
    Add,
    Remove,
    Replace
};

enum class TimelineEventKind3D {
    Edge,
    Vertex,
    Triangle
};

struct TimelineEdgeChange3D {
    EdgeAction3D action = EdgeAction3D::Add;
    SceneEdge3D edge;
    SceneEdge3D replacementEdge;
};

struct TimelineVertexChange3D {
    VertexAction3D action = VertexAction3D::Show;
    std::size_t vertexIndex = 0;
    Point3 point{};
    SceneVertexStyle3D style{};
};

struct TimelineTriangleChange3D {
    TriangleAction3D action = TriangleAction3D::Add;
    SceneTriangle3D triangle;
    SceneTriangle3D replacementTriangle;
};

struct TimelineEvent3D {
    TimelineEventKind3D kind = TimelineEventKind3D::Edge;
    std::string caption;

    EdgeAction3D edgeAction = EdgeAction3D::Add;
    SceneEdge3D edge;
    SceneEdge3D replacementEdge;

    VertexAction3D vertexAction = VertexAction3D::Show;
    std::size_t vertexIndex = 0;
    Point3 point{};
    SceneVertexStyle3D vertexStyle{};

    TriangleAction3D triangleAction = TriangleAction3D::Add;
    SceneTriangle3D triangle;
    SceneTriangle3D replacementTriangle;

    std::vector<TimelineEdgeChange3D> extraEdgeChanges;
    std::vector<TimelineVertexChange3D> extraVertexChanges;
    std::vector<TimelineTriangleChange3D> extraTriangleChanges;
};

struct GeometryScene3D {
    std::vector<Point3> vertices;
    std::vector<SceneVertexStyle3D> vertexStyles;
    std::vector<std::size_t> fitVertexIndices;
    std::size_t initialVisibleVertexCount = std::numeric_limits<std::size_t>::max();
    std::vector<SceneEdge3D> persistentEdges;
    std::vector<SceneTriangle3D> persistentTriangles;
    std::vector<TimelineEvent3D> timeline;
};

struct AlgorithmVisualization3D {
    GeometryScene3D scene;
    std::string status;
    bool succeeded = false;
};
