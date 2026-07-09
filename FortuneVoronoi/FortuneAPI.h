#pragma once

#include "GeometryScene.h"

#include <cstddef>
#include <vector>

namespace fortune_voronoi {

struct VoronoiSegment {
    Point2 first{};
    Point2 second{};
    std::size_t leftSite = 0;
    std::size_t rightSite = 0;
};

struct Result {
    AlgorithmVisualization visualization;
    std::vector<Point2> voronoiVertices;
    std::vector<VoronoiSegment> clippedSegments;
    std::size_t rawEdgeCount = 0;
    std::size_t siteEventCount = 0;
    std::size_t circleEventCount = 0;
};

Result Run(const std::vector<Point2>& sites);

} // namespace fortune_voronoi
