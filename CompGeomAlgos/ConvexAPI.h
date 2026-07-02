#pragma once

#include "GeometryScene.h"

#include <cstddef>
#include <vector>

namespace convex_hull {

struct Result {
    std::vector<std::size_t> hullVertexIndices;
    AlgorithmVisualization visualization;
};

// Runs the CPU monotone-chain implementation in CPUConvexHull.cpp and
// records every chain-edge addition/removal for generic timeline playback.
Result Run(const std::vector<Point2>& points);

} // namespace convex_hull

