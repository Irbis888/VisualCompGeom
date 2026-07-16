#include "Geometry3DAPI.h"

#include <numeric>
#include <string>

namespace {

std::vector<Point3> DefaultVertices()
{
    return {
        {-3.0, 0.0, -1.0}, {-1.0, 0.0, -1.0},
        {-1.0, 2.0, -1.0}, {-3.0, 2.0, -1.0},
        {-3.0, 0.0,  1.0}, {-1.0, 0.0,  1.0},
        {-1.0, 2.0,  1.0}, {-3.0, 2.0,  1.0},
        { 1.0, 0.0, -1.2}, { 3.4, 0.0, -1.2},
        { 3.4, 0.0,  1.2}, { 1.0, 0.0,  1.2},
        { 2.2, 2.6,  0.0}
    };
}

} // namespace

geometry_3d::Result geometry_3d::Run(const std::vector<Point3>& inputVertices)
{
    Result result;
    GeometryScene3D& scene = result.visualization.scene;
    scene.vertices = inputVertices.empty() ? DefaultVertices() : inputVertices;
    scene.initialVisibleVertexCount = scene.vertices.size();
    scene.fitVertexIndices.resize(scene.vertices.size());
    std::iota(scene.fitVertexIndices.begin(), scene.fitVertexIndices.end(), std::size_t{0});
    scene.vertexStyles.assign(scene.vertices.size(), SceneVertexStyle3D{});

    if (scene.vertices.size() != 13) {
        result.visualization.status =
            "3D demo expects 13 vertices: 8 for the cube and 5 for the pyramid.";
        return result;
    }

    result.edges = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7},
        {8,9},{9,10},{10,11},{11,8},{8,12},{9,12},{10,12},{11,12}
    };
    for (const Edge3 edge : result.edges) {
        scene.persistentEdges.push_back({edge, EdgeLayer3D::Input});
    }

    const std::vector<Triangle3> cubeTriangles = {
        {0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,4,7},{0,7,3},
        {1,2,6},{1,6,5},{0,1,5},{0,5,4},{3,7,6},{3,6,2}
    };
    const std::vector<Triangle3> pyramidTriangles = {
        {8,9,10},{8,10,11},{8,12,9},{9,12,10},{10,12,11},{11,12,8}
    };
    result.triangles.reserve(cubeTriangles.size() + pyramidTriangles.size());
    for (const Triangle3 triangle : cubeTriangles) {
        result.triangles.push_back(triangle);
        scene.persistentTriangles.push_back({
            triangle, SceneColor3D{79, 142, 213, 210}
        });
    }
    for (const Triangle3 triangle : pyramidTriangles) {
        result.triangles.push_back(triangle);
        scene.persistentTriangles.push_back({
            triangle, SceneColor3D{220, 137, 75, 210}
        });
    }

    result.visualization.status =
        "3D scene API: 13 editable vertices, 20 fixed edges, 18 triangles.";
    result.visualization.succeeded = true;
    return result;
}
