#pragma once

#include "GeometryScene.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

struct AlgorithmRunOptions {
    std::string randomSeed;
};

enum class AlgorithmView {
    Timeline2D,
    Workspace3D
};

struct AlgorithmDefinition {
    std::string name;
    std::filesystem::path inputFile;
    std::function<AlgorithmVisualization(const std::vector<Point2>&, const AlgorithmRunOptions&)> run;
    AlgorithmView view = AlgorithmView::Timeline2D;
};

const std::vector<AlgorithmDefinition>& AvailableAlgorithms();

