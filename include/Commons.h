#pragma once

#include <string>
#include <vector>

class Commons
{};

struct HalfEdge;
struct Face;

struct Vertex {
    int x;
    int y;
    std::vector<HalfEdge*> incidentEdges;
};

struct Vertex3D {
    int x;
    int y;
    int z;
};

struct HalfEdge {
    Vertex* origin = nullptr;
    HalfEdge* twin = nullptr;
    HalfEdge* next = nullptr;
    HalfEdge* prev = nullptr;
    Face* face = nullptr;
    Vertex* helper = nullptr;
};

struct Face {
    HalfEdge* boundary = nullptr;
};

struct DCEL {
    std::vector<HalfEdge> halfEdges;
    Face interiorFace;
    Face exteriorFace;
};

inline long long Orient2D(const Vertex& a, const Vertex& b, const Vertex& c)
{
	return 1LL * (b.x - a.x) * (c.y - a.y) - 1LL * (b.y - a.y) * (c.x - a.x);
}

std::vector<Vertex> ReadVerticesFromFile(const std::string& fileName);

// Signed volume determinant of tetrahedron (a, b, c, d).
// Opposite signs mean opposite orientations; zero means coplanar.
inline long double Orient3D(
    const Vertex3D& a,
    const Vertex3D& b,
    const Vertex3D& c,
    const Vertex3D& d)
{
    const long double bax = static_cast<long double>(b.x) - a.x;
    const long double bay = static_cast<long double>(b.y) - a.y;
    const long double baz = static_cast<long double>(b.z) - a.z;
    const long double cax = static_cast<long double>(c.x) - a.x;
    const long double cay = static_cast<long double>(c.y) - a.y;
    const long double caz = static_cast<long double>(c.z) - a.z;
    const long double dax = static_cast<long double>(d.x) - a.x;
    const long double day = static_cast<long double>(d.y) - a.y;
    const long double daz = static_cast<long double>(d.z) - a.z;

    return bax * (cay * daz - caz * day)
        - bay * (cax * daz - caz * dax)
        + baz * (cax * day - cay * dax);
}

std::vector<Vertex3D> ReadVertices3DFromFile(const std::string& fileName);
