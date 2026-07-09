#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include "Commons.h"
#include "TriangulationAPI.h"

enum class VertexType
{
    Regular,
    Start,
    End,
    Split,
    Merge
};

bool ComesBefore(const Vertex& p, const Vertex& q)
{
    return p.y > q.y || (p.y == q.y && p.x < q.x);
}

struct VertexPriority
{
    bool operator()(const Vertex* p, const Vertex* q) const
    {
        return ComesBefore(*q, *p);
    }
};

long double EdgeXAtY(const HalfEdge* edge, long double y)
{
    const Vertex* origin = edge->origin;
    const Vertex* destination = edge->next->origin;

    if (origin->y == destination->y) {
        return static_cast<long double>(std::min(origin->x, destination->x));
    }

    const long double dy = static_cast<long double>(destination->y - origin->y);
    const long double t = (y - origin->y) / dy;
    return origin->x + t * (destination->x - origin->x);
}

struct SweepPosition
{
    long double x;
};

struct SweepEdgeLess
{
    using is_transparent = void;

    const long double* sweepY;

    bool operator()(const HalfEdge* lhs, const HalfEdge* rhs) const
    {
        const long double lhsX = EdgeXAtY(lhs, *sweepY);
        const long double rhsX = EdgeXAtY(rhs, *sweepY);
        if (lhsX != rhsX) {
            return lhsX < rhsX;
        }
        return std::less<const HalfEdge*>{}(lhs, rhs);
    }

    bool operator()(const HalfEdge* edge, SweepPosition position) const
    {
        return EdgeXAtY(edge, *sweepY) < position.x;
    }

    bool operator()(SweepPosition position, const HalfEdge* edge) const
    {
        return position.x < EdgeXAtY(edge, *sweepY);
    }
};

using SweepStatus = std::set<HalfEdge*, SweepEdgeLess>;
using Diagonal = std::pair<Vertex*, Vertex*>;
using Polygon = std::vector<Vertex*>;

void InitializePolygonDCEL(std::vector<Vertex>& vertices, DCEL& dcel)
{
    const std::size_t count = vertices.size();
    dcel.halfEdges.clear();
    dcel.halfEdges.resize(2 * count);

    for (Vertex& vertex : vertices) {
        vertex.incidentEdges.clear();
        vertex.incidentEdges.reserve(2);
    }

    for (std::size_t i = 0; i < count; ++i) {
        HalfEdge& interior = dcel.halfEdges[2 * i];
        HalfEdge& exterior = dcel.halfEdges[2 * i + 1];

        interior.origin = &vertices[i];
        interior.twin = &exterior;
        interior.next = &dcel.halfEdges[2 * ((i + 1) % count)];
        interior.prev = &dcel.halfEdges[2 * ((i + count - 1) % count)];
        interior.face = &dcel.interiorFace;
        interior.helper = nullptr;

        exterior.origin = &vertices[(i + 1) % count];
        exterior.twin = &interior;
        exterior.next = &dcel.halfEdges[2 * ((i + count - 1) % count) + 1];
        exterior.prev = &dcel.halfEdges[2 * ((i + 1) % count) + 1];
        exterior.face = &dcel.exteriorFace;
        exterior.helper = nullptr;

        interior.origin->incidentEdges.push_back(&interior);
        exterior.origin->incidentEdges.push_back(&exterior);
    }

    dcel.interiorFace.boundary = &dcel.halfEdges[0];
    dcel.exteriorFace.boundary = &dcel.halfEdges[1];
}

VertexType ClassifyVertex(const std::vector<Vertex>& vertices, std::size_t index)
{
    const std::size_t count = vertices.size();
    const Vertex& previous = vertices[(index + count - 1) % count];
    const Vertex& current = vertices[index];
    const Vertex& next = vertices[(index + 1) % count];

    const bool neighborsAreBelow =
        ComesBefore(current, previous) && ComesBefore(current, next);
    const bool neighborsAreAbove =
        ComesBefore(previous, current) && ComesBefore(next, current);
    const bool isConvex = Orient2D(previous, current, next) > 0;

    if (neighborsAreBelow) {
        return isConvex ? VertexType::Start : VertexType::Split;
    }
    if (neighborsAreAbove) {
        return isConvex ? VertexType::End : VertexType::Merge;
    }
    return VertexType::Regular;
}

const char* ToString(VertexType type)
{
    switch (type) {
    case VertexType::Regular: return "regular";
    case VertexType::Start:   return "start";
    case VertexType::End:     return "end";
    case VertexType::Split:   return "split";
    case VertexType::Merge:   return "merge";
    }
    return "unknown";
}

std::size_t VertexIndex(const std::vector<Vertex>& vertices, const Vertex* vertex)
{
    return static_cast<std::size_t>(vertex - vertices.data());
}

bool IsMergeVertex(
    const std::vector<Vertex>& vertices,
    const std::vector<VertexType>& vertexTypes,
    const Vertex* vertex)
{
    return vertex != nullptr &&
        vertexTypes[VertexIndex(vertices, vertex)] == VertexType::Merge;
}

void InsertDiagonal(Vertex* first, Vertex* second, std::vector<Diagonal>& diagonals)
{
    if (first == nullptr || second == nullptr) {
        throw std::runtime_error("Cannot create a diagonal with a null endpoint");
    }
    diagonals.emplace_back(first, second);
}

void InsertEdge(HalfEdge* edge, Vertex* helper, SweepStatus& status)
{
    edge->helper = helper;
    status.insert(edge);
}

void DeleteEdge(HalfEdge* edge, SweepStatus& status)
{
    const auto position = status.find(edge);
    if (position == status.end()) {
        throw std::runtime_error("Attempted to delete an inactive sweep edge");
    }
    status.erase(position);
}

HalfEdge* FindEdgeDirectlyLeft(const Vertex& vertex, SweepStatus& status)
{
    const auto firstNotLeft = status.lower_bound(
        SweepPosition{ static_cast<long double>(vertex.x) });
    if (firstNotLeft == status.begin()) {
        throw std::runtime_error("No active edge exists directly left of the vertex");
    }
    return *std::prev(firstNotLeft);
}

void HandleStartVertex(Vertex* vertex, HalfEdge* outgoingEdge, SweepStatus& status)
{
    InsertEdge(outgoingEdge, vertex, status);
}

void HandleEndVertex(
    Vertex* vertex,
    HalfEdge* incomingEdge,
    SweepStatus& status,
    const std::vector<Vertex>& vertices,
    const std::vector<VertexType>& vertexTypes,
    std::vector<Diagonal>& diagonals)
{
    if (IsMergeVertex(vertices, vertexTypes, incomingEdge->helper)) {
        InsertDiagonal(vertex, incomingEdge->helper, diagonals);
    }
    DeleteEdge(incomingEdge, status);
}

void HandleSplitVertex(
    Vertex* vertex,
    HalfEdge* outgoingEdge,
    SweepStatus& status,
    std::vector<Diagonal>& diagonals)
{
    HalfEdge* leftEdge = FindEdgeDirectlyLeft(*vertex, status);
    InsertDiagonal(vertex, leftEdge->helper, diagonals);
    leftEdge->helper = vertex;
    InsertEdge(outgoingEdge, vertex, status);
}

void HandleMergeVertex(
    Vertex* vertex,
    HalfEdge* incomingEdge,
    SweepStatus& status,
    const std::vector<Vertex>& vertices,
    const std::vector<VertexType>& vertexTypes,
    std::vector<Diagonal>& diagonals)
{
    if (IsMergeVertex(vertices, vertexTypes, incomingEdge->helper)) {
        InsertDiagonal(vertex, incomingEdge->helper, diagonals);
    }
    DeleteEdge(incomingEdge, status);

    HalfEdge* leftEdge = FindEdgeDirectlyLeft(*vertex, status);
    if (IsMergeVertex(vertices, vertexTypes, leftEdge->helper)) {
        InsertDiagonal(vertex, leftEdge->helper, diagonals);
    }
    leftEdge->helper = vertex;
}

void HandleRegularVertex(
    Vertex* vertex,
    HalfEdge* incomingEdge,
    HalfEdge* outgoingEdge,
    SweepStatus& status,
    const std::vector<Vertex>& vertices,
    const std::vector<VertexType>& vertexTypes,
    std::vector<Diagonal>& diagonals)
{
    const bool interiorLiesToRight = ComesBefore(*vertex, *outgoingEdge->next->origin);
    if (interiorLiesToRight) {
        if (IsMergeVertex(vertices, vertexTypes, incomingEdge->helper)) {
            InsertDiagonal(vertex, incomingEdge->helper, diagonals);
        }
        DeleteEdge(incomingEdge, status);
        InsertEdge(outgoingEdge, vertex, status);
        return;
    }

    HalfEdge* leftEdge = FindEdgeDirectlyLeft(*vertex, status);
    if (IsMergeVertex(vertices, vertexTypes, leftEdge->helper)) {
        InsertDiagonal(vertex, leftEdge->helper, diagonals);
    }
    leftEdge->helper = vertex;
}

bool AreAdjacent(std::size_t first, std::size_t second, std::size_t count)
{
    return (first + 1) % count == second || (second + 1) % count == first;
}

bool SplitPolygon(
    const Polygon& polygon,
    Vertex* first,
    Vertex* second,
    Polygon& firstPart,
    Polygon& secondPart)
{
    const auto firstPosition = std::find(polygon.begin(), polygon.end(), first);
    const auto secondPosition = std::find(polygon.begin(), polygon.end(), second);
    if (firstPosition == polygon.end() || secondPosition == polygon.end()) {
        return false;
    }

    const std::size_t firstIndex = static_cast<std::size_t>(
        firstPosition - polygon.begin());
    const std::size_t secondIndex = static_cast<std::size_t>(
        secondPosition - polygon.begin());
    if (firstIndex == secondIndex ||
        AreAdjacent(firstIndex, secondIndex, polygon.size())) {
        return false;
    }

    firstPart.clear();
    for (std::size_t i = firstIndex;; i = (i + 1) % polygon.size()) {
        firstPart.push_back(polygon[i]);
        if (i == secondIndex) {
            break;
        }
    }

    secondPart.clear();
    for (std::size_t i = secondIndex;; i = (i + 1) % polygon.size()) {
        secondPart.push_back(polygon[i]);
        if (i == firstIndex) {
            break;
        }
    }
    return true;
}

std::vector<Polygon> PartitionPolygon(
    std::vector<Vertex>& vertices,
    const std::vector<Diagonal>& diagonals)
{
    Polygon originalPolygon;
    originalPolygon.reserve(vertices.size());
    for (Vertex& vertex : vertices) {
        originalPolygon.push_back(&vertex);
    }

    std::vector<Polygon> polygons{ std::move(originalPolygon) };
    for (const Diagonal& diagonal : diagonals) {
        bool wasSplit = false;
        for (std::size_t i = 0; i < polygons.size(); ++i) {
            Polygon firstPart;
            Polygon secondPart;
            if (!SplitPolygon(
                polygons[i], diagonal.first, diagonal.second,
                firstPart, secondPart)) {
                continue;
            }

            polygons[i] = std::move(firstPart);
            polygons.push_back(std::move(secondPart));
            wasSplit = true;
            break;
        }
        if (!wasSplit) {
            throw std::runtime_error("A partition diagonal cannot split any polygon");
        }
    }
    return polygons;
}

bool PointOnSegment(
    long double x,
    long double y,
    const Vertex& first,
    const Vertex& second)
{
    const long double cross =
        (second.x - first.x) * (y - first.y) -
        (second.y - first.y) * (x - first.x);
    if (cross != 0.0L) {
        return false;
    }
    return x >= std::min(first.x, second.x) &&
        x <= std::max(first.x, second.x) &&
        y >= std::min(first.y, second.y) &&
        y <= std::max(first.y, second.y);
}

bool PointStrictlyInsidePolygon(long double x, long double y, const Polygon& polygon)
{
    bool inside = false;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Vertex& first = *polygon[i];
        const Vertex& second = *polygon[(i + 1) % polygon.size()];
        if (PointOnSegment(x, y, first, second)) {
            return false;
        }

        const bool crossesY = (first.y > y) != (second.y > y);
        if (crossesY) {
            const long double intersectionX = first.x +
                (y - first.y) * (second.x - first.x) /
                static_cast<long double>(second.y - first.y);
            if (x < intersectionX) {
                inside = !inside;
            }
        }
    }
    return inside;
}

bool OnClosedSegment(const Vertex& first, const Vertex& second, const Vertex& point)
{
    return Orient2D(first, second, point) == 0 &&
        point.x >= std::min(first.x, second.x) &&
        point.x <= std::max(first.x, second.x) &&
        point.y >= std::min(first.y, second.y) &&
        point.y <= std::max(first.y, second.y);
}

bool SegmentsIntersect(
    const Vertex& a,
    const Vertex& b,
    const Vertex& c,
    const Vertex& d)
{
    const long long abc = Orient2D(a, b, c);
    const long long abd = Orient2D(a, b, d);
    const long long cda = Orient2D(c, d, a);
    const long long cdb = Orient2D(c, d, b);

    if (((abc > 0 && abd < 0) || (abc < 0 && abd > 0)) &&
        ((cda > 0 && cdb < 0) || (cda < 0 && cdb > 0))) {
        return true;
    }
    return (abc == 0 && OnClosedSegment(a, b, c)) ||
        (abd == 0 && OnClosedSegment(a, b, d)) ||
        (cda == 0 && OnClosedSegment(c, d, a)) ||
        (cdb == 0 && OnClosedSegment(c, d, b));
}

bool IsValidStrictDiagonal(
    const Polygon& polygon,
    std::size_t firstIndex,
    std::size_t secondIndex)
{
    if (firstIndex == secondIndex ||
        AreAdjacent(firstIndex, secondIndex, polygon.size())) {
        return false;
    }

    const Vertex& first = *polygon[firstIndex];
    const Vertex& second = *polygon[secondIndex];
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Vertex* edgeFirst = polygon[i];
        const Vertex* edgeSecond = polygon[(i + 1) % polygon.size()];
        const bool isIncident = edgeFirst == &first || edgeSecond == &first ||
            edgeFirst == &second || edgeSecond == &second;

        if (isIncident) {
            const Vertex* other = edgeFirst == &first || edgeFirst == &second
                ? edgeSecond : edgeFirst;
            if (Orient2D(first, second, *other) == 0 &&
                OnClosedSegment(first, second, *other)) {
                return false;
            }
            continue;
        }
        if (SegmentsIntersect(first, second, *edgeFirst, *edgeSecond)) {
            return false;
        }
    }

    const long double midpointX =
        (static_cast<long double>(first.x) + second.x) / 2.0L;
    const long double midpointY =
        (static_cast<long double>(first.y) + second.y) / 2.0L;
    return PointStrictlyInsidePolygon(midpointX, midpointY, polygon);
}

void GreedyTriangulate(
    const Polygon& polygon,
    std::vector<Diagonal>& triangulationDiagonals)
{
    if (polygon.size() == 3) {
        return;
    }
    if (polygon.size() < 3) {
        throw std::runtime_error("Cannot triangulate a degenerate polygon");
    }

    std::vector<std::size_t> topFirst(polygon.size());
    std::iota(topFirst.begin(), topFirst.end(), 0);
    std::sort(topFirst.begin(), topFirst.end(),
        [&polygon](std::size_t first, std::size_t second)
        {
            return ComesBefore(*polygon[first], *polygon[second]);
        });

    for (const std::size_t firstIndex : topFirst) {
        for (const std::size_t secondIndex : topFirst) {
            if (!IsValidStrictDiagonal(polygon, firstIndex, secondIndex)) {
                continue;
            }

            Vertex* first = polygon[firstIndex];
            Vertex* second = polygon[secondIndex];
            triangulationDiagonals.emplace_back(first, second);

            Polygon firstPart;
            Polygon secondPart;
            if (!SplitPolygon(
                polygon, first, second, firstPart, secondPart)) {
                throw std::runtime_error("A valid diagonal failed to split its polygon");
            }
            GreedyTriangulate(firstPart, triangulationDiagonals);
            GreedyTriangulate(secondPart, triangulationDiagonals);
            return;
        }
    }

    throw std::runtime_error("No strictly interior diagonal was found");
}

triangulation::Result triangulation::Run(const std::vector<Point2>& points)
{
    Result result;
    result.visualization.scene.points = points;
    if (points.size() >= 2) {
        for (std::size_t i = 0; i + 1 < points.size(); ++i) {
            result.visualization.scene.persistentEdges.push_back({
                {i, i + 1}, EdgeLayer::Input
            });
        }
        if (points.size() >= 3) {
            result.visualization.scene.persistentEdges.push_back({
                {points.size() - 1, 0}, EdgeLayer::Input
            });
        }
    }

    try {
        if (points.size() < 3) {
            throw std::runtime_error("A polygon must have at least three vertices");
        }

        std::vector<Vertex> vertices;
        vertices.reserve(points.size());
        for (const Point2 point : points) {
            const double minInt = static_cast<double>(std::numeric_limits<int>::min());
            const double maxInt = static_cast<double>(std::numeric_limits<int>::max());
            if (point.x < minInt || point.x > maxInt ||
                point.y < minInt || point.y > maxInt) {
                throw std::runtime_error("A coordinate is outside the integer range");
            }
            vertices.push_back(Vertex{
                static_cast<int>(std::lround(point.x)),
                static_cast<int>(std::lround(point.y)),
                {}
            });
        }

        long long twiceSignedArea = 0;
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const Vertex& first = vertices[i];
            const Vertex& second = vertices[(i + 1) % vertices.size()];
            if (first.x == second.x && first.y == second.y) {
                throw std::runtime_error("Consecutive polygon vertices must be distinct");
            }
            twiceSignedArea += 1LL * first.x * second.y - 1LL * first.y * second.x;
        }
        if (twiceSignedArea <= 0) {
            throw std::runtime_error("Polygon vertices must be counterclockwise");
        }

        DCEL dcel{};
        InitializePolygonDCEL(vertices, dcel);

        std::priority_queue<Vertex*, std::vector<Vertex*>, VertexPriority> Q;
        for (Vertex& vertex : vertices) Q.push(&vertex);

        std::vector<VertexType> vertexTypes(vertices.size());
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            vertexTypes[i] = ClassifyVertex(vertices, i);
        }

        long double sweepY = 0.0L;
        SweepStatus T(SweepEdgeLess{ &sweepY });
        std::vector<Diagonal> D;

        while (!Q.empty()) {
            Vertex* vertex = Q.top();
            Q.pop();

            sweepY = static_cast<long double>(vertex->y);
            const std::size_t index = VertexIndex(vertices, vertex);
            HalfEdge* outgoingEdge = &dcel.halfEdges[2 * index];
            HalfEdge* incomingEdge = &dcel.halfEdges[
                2 * ((index + vertices.size() - 1) % vertices.size())];

            switch (vertexTypes[index]) {
            case VertexType::Start:
                HandleStartVertex(vertex, outgoingEdge, T);
                break;
            case VertexType::End:
                HandleEndVertex(vertex, incomingEdge, T, vertices, vertexTypes, D);
                break;
            case VertexType::Split:
                HandleSplitVertex(vertex, outgoingEdge, T, D);
                break;
            case VertexType::Merge:
                HandleMergeVertex(vertex, incomingEdge, T, vertices, vertexTypes, D);
                break;
            case VertexType::Regular:
                HandleRegularVertex(
                    vertex, incomingEdge, outgoingEdge, T,
                    vertices, vertexTypes, D);
                break;
            }
        }

        const std::vector<Polygon> monotonePolygons = PartitionPolygon(vertices, D);
        std::vector<Diagonal> triangulationDiagonals;
        for (const Polygon& polygon : monotonePolygons) {
            GreedyTriangulate(polygon, triangulationDiagonals);
        }

        result.monotonePolygonCount = monotonePolygons.size();
        result.partitionDiagonals.reserve(D.size());
        result.triangulationDiagonals.reserve(triangulationDiagonals.size());

        for (const Diagonal& diagonal : D) {
            result.partitionDiagonals.push_back({
                VertexIndex(vertices, diagonal.first),
                VertexIndex(vertices, diagonal.second)
            });
        }
        for (const Diagonal& diagonal : triangulationDiagonals) {
            result.triangulationDiagonals.push_back({
                VertexIndex(vertices, diagonal.first),
                VertexIndex(vertices, diagonal.second)
            });
        }

        for (std::size_t i = 0; i < result.partitionDiagonals.size(); ++i) {
            const Edge edge = result.partitionDiagonals[i];
            result.visualization.scene.timeline.push_back({
                EdgeAction::Add,
                {{edge.first, edge.second}, EdgeLayer::Intermediate},
                "Monotonization: partition diagonal " + std::to_string(i + 1) +
                    " / " + std::to_string(result.partitionDiagonals.size())
            });
        }
        for (std::size_t i = 0; i < result.triangulationDiagonals.size(); ++i) {
            const Edge edge = result.triangulationDiagonals[i];
            result.visualization.scene.timeline.push_back({
                EdgeAction::Add,
                {{edge.first, edge.second}, EdgeLayer::Result},
                "Triangulation: diagonal " + std::to_string(i + 1) +
                    " / " + std::to_string(result.triangulationDiagonals.size())
            });
        }

        result.visualization.status = "Ready: " +
            std::to_string(result.monotonePolygonCount) + " monotone polygons, " +
            std::to_string(result.partitionDiagonals.size() +
                result.triangulationDiagonals.size()) + " diagonals.";
        result.visualization.succeeded = true;
    }
    catch (const std::exception& exception) {
        result.visualization.status = std::string("Triangulation error: ") + exception.what();
    }
    return result;
}

#ifndef TRIANGULATION_NO_MAIN
int main()
{
    const std::vector<Vertex> inputVertices = ReadVerticesFromFile("input.txt");
    std::vector<Point2> points;
    points.reserve(inputVertices.size());
    for (const Vertex& vertex : inputVertices) {
        points.push_back({
            static_cast<double>(vertex.x),
            static_cast<double>(vertex.y)
        });
    }

    const triangulation::Result result = triangulation::Run(points);
    if (!result.visualization.succeeded) {
        std::cerr << result.visualization.status << '\n';
        return 1;
    }

    std::cout << "Monotone polygons: " << result.monotonePolygonCount << '\n';
    for (const triangulation::Edge& diagonal : result.partitionDiagonals) {
        std::cout << "Partition diagonal: (" << points[diagonal.first].x << ", "
            << points[diagonal.first].y << ") -> (" << points[diagonal.second].x
            << ", " << points[diagonal.second].y << ")\n";
    }
    for (const triangulation::Edge& diagonal : result.triangulationDiagonals) {
        std::cout << "Triangulation diagonal: (" << points[diagonal.first].x << ", "
            << points[diagonal.first].y << ") -> (" << points[diagonal.second].x
            << ", " << points[diagonal.second].y << ")\n";
    }

    const std::size_t totalDiagonalCount =
        result.partitionDiagonals.size() + result.triangulationDiagonals.size();
    std::cout << "Total diagonals: " << totalDiagonalCount
        << " (expected " << points.size() - 3 << ")\n";
}
#endif
