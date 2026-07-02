#include "algorithm_registry.h"

#include "ConvexAPI.h"
#include "TriangulationAPI.h"

#ifndef CPU_CONVEX_INPUT_FILE
#define CPU_CONVEX_INPUT_FILE "input.txt"
#endif

#ifndef TRIANGULATION_INPUT_FILE
#define TRIANGULATION_INPUT_FILE "input.txt"
#endif

const std::vector<AlgorithmDefinition>& AvailableAlgorithms()
{
    static const std::vector<AlgorithmDefinition> algorithms = {
        {
            "CPU Convex Hull",
            CPU_CONVEX_INPUT_FILE,
            [](const std::vector<Point2>& points) {
                return convex_hull::Run(points).visualization;
            }
        },
        {
            "Polygon Triangulation",
            TRIANGULATION_INPUT_FILE,
            [](const std::vector<Point2>& points) {
                return triangulation::Run(points).visualization;
            }
        }
    };
    return algorithms;
}

