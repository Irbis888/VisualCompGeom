#pragma once

#include "Commons.h"
#include "GeometryScene.h"

#include <cstddef>
#include <string>
#include <vector>

namespace delaunay_triangulation {

struct Result {
    AlgorithmVisualization visualization;
    DCEL triangulation;
    std::vector<Vertex> dcelVertices;
    std::vector<Edge2> triangulationEdges;
    std::size_t siteCount = 0;
    unsigned long long permutationSeed = 0;
    bool algorithmImplemented = false;
};

Result Run(const std::vector<Point2>& sites, const std::string& seedText = {});

} // namespace delaunay_triangulation
