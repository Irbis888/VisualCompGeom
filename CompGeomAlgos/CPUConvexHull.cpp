// CPU convex hull construction for an arbitrary 2D point cloud.

#include "ConvexAPI.h"
#include "Commons.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct IndexedVertex {
    Vertex vertex;
    std::size_t originalIndex;
};

void RecordEdge(
    GeometryScene& scene,
    EdgeAction action,
    const IndexedVertex& first,
    const IndexedVertex& second,
    EdgeLayer layer,
    std::string caption)
{
    scene.timeline.push_back({
        action,
        {{first.originalIndex, second.originalIndex}, layer},
        std::move(caption)
    });
}

} // namespace

convex_hull::Result convex_hull::Run(const std::vector<Point2>& points)
{
    Result result;
    result.visualization.scene.points = points;

    try {
        std::vector<IndexedVertex> sorted;
        sorted.reserve(points.size()); 
        for (std::size_t i = 0; i < points.size(); ++i) {
            const Point2 point = points[i];
            const double minInt = static_cast<double>(std::numeric_limits<int>::min());
            const double maxInt = static_cast<double>(std::numeric_limits<int>::max());
            if (point.x < minInt || point.x > maxInt ||
                point.y < minInt || point.y > maxInt) {
                throw std::runtime_error("A coordinate is outside the integer range");
            }
            sorted.push_back({
                Vertex{
                    static_cast<int>(std::lround(point.x)),
                    static_cast<int>(std::lround(point.y)),
                    {}
                },
                i
            });                                                                         // creates vertices
        }

        std::sort(sorted.begin(), sorted.end(),                                         // sorts vertices from up to down
            [](const IndexedVertex& left, const IndexedVertex& right) {
                if (left.vertex.x != right.vertex.x) return left.vertex.x < right.vertex.x;
                if (left.vertex.y != right.vertex.y) return left.vertex.y < right.vertex.y;
                return left.originalIndex < right.originalIndex;
            });

        if (sorted.empty()) {
            result.visualization.status = "Add at least one point.";
            return result;
        }
        if (sorted.size() == 1) {
            result.hullVertexIndices.push_back(sorted.front().originalIndex);
            result.visualization.status = "Convex hull contains one point.";
            result.visualization.succeeded = true;
            return result;
        }

        std::vector<IndexedVertex> upperOut;
        upperOut.push_back(sorted[0]);
        upperOut.push_back(sorted[1]);
        RecordEdge(result.visualization.scene, EdgeAction::Add,
            upperOut[0], upperOut[1], EdgeLayer::Intermediate,
            "Upper hull: initialize first edge");

        const int count = static_cast<int>(sorted.size());
        for (int i = 2; i < count; ++i) {
            while (upperOut.size() >= 2) {
                const std::size_t end = upperOut.size();
                const long long turn = Orient2D(
                    upperOut[end - 2].vertex,
                    upperOut[end - 1].vertex,
                    sorted[static_cast<std::size_t>(i)].vertex);
                if (turn < 0) break;

                RecordEdge(result.visualization.scene, EdgeAction::Remove,
                    upperOut[end - 2], upperOut[end - 1], EdgeLayer::Intermediate,
                    "Upper hull: remove non-clockwise edge");
                upperOut.pop_back();
            }
            RecordEdge(result.visualization.scene, EdgeAction::Add,
                upperOut.back(), sorted[static_cast<std::size_t>(i)],
                EdgeLayer::Intermediate, "Upper hull: add candidate edge");
            upperOut.push_back(sorted[static_cast<std::size_t>(i)]);
        }

        std::vector<IndexedVertex> lowerOut;
        lowerOut.push_back(sorted[0]);
        lowerOut.push_back(sorted[1]);
        RecordEdge(result.visualization.scene, EdgeAction::Add,
            lowerOut[0], lowerOut[1], EdgeLayer::Result,
            "Lower hull: initialize first edge");

        for (int i = 2; i < count; ++i) {
            while (lowerOut.size() >= 2) {
                const std::size_t end = lowerOut.size();
                const long long turn = Orient2D(
                    lowerOut[end - 2].vertex,
                    lowerOut[end - 1].vertex,
                    sorted[static_cast<std::size_t>(i)].vertex);
                if (turn > 0) break;

                RecordEdge(result.visualization.scene, EdgeAction::Remove,
                    lowerOut[end - 2], lowerOut[end - 1], EdgeLayer::Result,
                    "Lower hull: remove non-counterclockwise edge");
                lowerOut.pop_back();
            }
            RecordEdge(result.visualization.scene, EdgeAction::Add,
                lowerOut.back(), sorted[static_cast<std::size_t>(i)],
                EdgeLayer::Result, "Lower hull: add candidate edge");
            lowerOut.push_back(sorted[static_cast<std::size_t>(i)]);
        }

        result.hullVertexIndices.reserve(upperOut.size() + lowerOut.size() - 2);
        for (const IndexedVertex& vertex : upperOut) {
            result.hullVertexIndices.push_back(vertex.originalIndex);
        }
        for (std::size_t i = 1; i + 1 < lowerOut.size(); ++i) {
            result.hullVertexIndices.push_back(lowerOut[i].originalIndex);
        }

        result.visualization.status = "Ready: " +
            std::to_string(result.hullVertexIndices.size()) +
            " convex-hull vertices, " +
            std::to_string(result.visualization.scene.timeline.size()) +
            " timeline operations.";
        result.visualization.succeeded = true;
    }
    catch (const std::exception& exception) {
        result.visualization.status = std::string("Convex hull error: ") + exception.what();
    }

    return result;
}

#ifndef CPU_CONVEX_HULL_NO_MAIN
int main()
{
    try {
        const std::vector<Vertex> vertices = ReadVerticesFromFile("input.txt");
        std::vector<Point2> points;
        points.reserve(vertices.size());
        for (const Vertex& vertex : vertices) {
            points.push_back({
                static_cast<double>(vertex.x),
                static_cast<double>(vertex.y)
            });
        }

        const convex_hull::Result result = convex_hull::Run(points);
        if (!result.visualization.succeeded) {
            std::cerr << result.visualization.status << '\n';
            return 1;
        }

        std::cout << "Read " << points.size() << " vertices.\n";
        std::cout << "_______________\n";
        for (const std::size_t index : result.hullVertexIndices) {
            std::cout << points[index].x << ' ' << points[index].y << '\n';
        }
    }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
#endif
