#pragma once

#include <string>
#include <vector>

class Commons
{};

struct HalfEdge;

struct Vertex {
	int x;
	int y;
	std::vector<HalfEdge*> incidentEdges;
};

inline long long Orient2D(const Vertex& a, const Vertex& b, const Vertex& c)
{
	return 1LL * (b.x - a.x) * (c.y - a.y) - 1LL * (b.y - a.y) * (c.x - a.x);
}

std::vector<Vertex> ReadVerticesFromFile(const std::string& fileName);
