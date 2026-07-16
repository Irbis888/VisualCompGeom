#pragma once

#include "GeometryScene3D.h"

#include <vector>

namespace geometry_3d {

struct Result {
    std::vector<Edge3> edges;
    std::vector<Triangle3> triangles;
    AlgorithmVisualization3D visualization;
};

// API-template example for future spatial algorithms. With an empty input it
// produces the default editable cube and pyramid scene.
Result Run(const std::vector<Point3>& vertices);

} // namespace geometry_3d
