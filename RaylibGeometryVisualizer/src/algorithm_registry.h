#pragma once

#include "GeometryScene.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

struct AlgorithmDefinition {
    std::string name;
    std::filesystem::path inputFile;
    std::function<AlgorithmVisualization(const std::vector<Point2>&)> run;
};

const std::vector<AlgorithmDefinition>& AvailableAlgorithms();

