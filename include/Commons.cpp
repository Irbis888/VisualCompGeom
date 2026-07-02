#include "Commons.h"

#include <fstream>
#include <stdexcept>

std::vector<Vertex> ReadVerticesFromFile(const std::string& fileName)
{
    std::ifstream input(fileName);
    if (!input) {
        throw std::runtime_error("Failed to open input file: " + fileName);
    }

    int vertexCount = 0;
    if (!(input >> vertexCount) || vertexCount < 0) {
        throw std::runtime_error("Invalid vertex count in input file: " + fileName);
    }

    std::vector<Vertex> vertices;
    vertices.reserve(vertexCount);

    for (int i = 0; i < vertexCount; ++i) {
        Vertex vertex{};
        if (!(input >> vertex.x >> vertex.y)) {
            throw std::runtime_error("Invalid vertex data in input file: " + fileName);
        }
        vertices.push_back(vertex);
    }

    return vertices;
}
