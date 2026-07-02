#pragma once

#include "GeometryScene.h"

#include <cstddef>
#include <vector>

namespace triangulation {

struct Edge {
    std::size_t first;
    std::size_t second;
};

struct Result {
    std::vector<Edge> partitionDiagonals;
    std::vector<Edge> triangulationDiagonals;
    std::size_t monotonePolygonCount = 0;
    AlgorithmVisualization visualization;
};

// Runs the algorithm implemented in Triangulation.cpp. Input vertices must
// describe a simple counterclockwise polygon in boundary order.
Result Run(const std::vector<Point2>& points);

} // namespace triangulation
