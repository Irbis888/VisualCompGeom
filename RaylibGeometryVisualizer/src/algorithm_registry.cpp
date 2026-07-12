#include "algorithm_registry.h"

#include "ConvexAPI.h"
#include "DelaunayAPI.h"
#include "FortuneAPI.h"
#include "TriangulationAPI.h"

#ifndef CPU_CONVEX_INPUT_FILE
#define CPU_CONVEX_INPUT_FILE "input.txt"
#endif

#ifndef TRIANGULATION_INPUT_FILE
#define TRIANGULATION_INPUT_FILE "input.txt"
#endif

#ifndef FORTUNE_VORONOI_INPUT_FILE
#define FORTUNE_VORONOI_INPUT_FILE "input.txt"
#endif

#ifndef DELAUNAY_TRIANGULATION_INPUT_FILE
#define DELAUNAY_TRIANGULATION_INPUT_FILE "input.txt"
#endif

const std::vector<AlgorithmDefinition>& AvailableAlgorithms()
{
    static const std::vector<AlgorithmDefinition> algorithms = {
        {
            "CPU Convex Hull",
            CPU_CONVEX_INPUT_FILE,
            [](const std::vector<Point2>& points, const AlgorithmRunOptions&) {
                return convex_hull::Run(points).visualization;
            }
        },
        {
            "Polygon Triangulation",
            TRIANGULATION_INPUT_FILE,
            [](const std::vector<Point2>& points, const AlgorithmRunOptions&) {
                return triangulation::Run(points).visualization;
            }
        },
        {
            "Fortune Voronoi",
            FORTUNE_VORONOI_INPUT_FILE,
            [](const std::vector<Point2>& points, const AlgorithmRunOptions&) {
                return fortune_voronoi::Run(points).visualization;
            }
        },
        {
            "Delaunay Triangulation",
            DELAUNAY_TRIANGULATION_INPUT_FILE,
            [](const std::vector<Point2>& points, const AlgorithmRunOptions& options) {
                return delaunay_triangulation::Run(points, options.randomSeed).visualization;
            }
        },
        {
            "3D Workspace",
            {},
            {},
            AlgorithmView::Workspace3D
        }
    };
    return algorithms;
}
