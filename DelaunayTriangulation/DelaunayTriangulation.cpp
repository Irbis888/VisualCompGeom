#include "DelaunayAPI.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace delaunay_triangulation {
namespace {

constexpr double kEpsilon = 1.0e-9;
constexpr double kRotationAngle = 0.123456789; // breaks horizontal ties without changing Delaunay topology.

struct WorkPoint {
    Point2 original{};
    Point2 work{};
    bool artificial = false;
};

struct EdgeKey {
    int first = 0;
    int second = 0;

    EdgeKey() = default;
    EdgeKey(int a, int b)
    {
        if (a <= b) {
            first = a;
            second = b;
        }
        else {
            first = b;
            second = a;
        }
    }

    bool operator<(const EdgeKey& other) const
    {
        if (first != other.first) return first < other.first;
        return second < other.second;
    }
};

struct Triangle {
    std::array<int, 3> v{};
    bool alive = true;
};

enum class LocationKind {
    Outside,
    InsideTriangle,
    OnEdge
};

struct PointLocation {
    LocationKind kind = LocationKind::Outside;
    int triangle = -1;
    int edgeA = -1;
    int edgeB = -1;
};

Point2 Rotate(Point2 point)
{
    const double c = std::cos(kRotationAngle);
    const double s = std::sin(kRotationAngle);
    return {
        point.x * c - point.y * s,
        point.x * s + point.y * c
    };
}

Point2 InverseRotate(Point2 point)
{
    const double c = std::cos(kRotationAngle);
    const double s = std::sin(kRotationAngle);
    return {
        point.x * c + point.y * s,
        -point.x * s + point.y * c
    };
}

double Orient(Point2 a, Point2 b, Point2 c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

double InCircle(Point2 a, Point2 b, Point2 c, Point2 d)
{
    const double adx = a.x - d.x;
    const double ady = a.y - d.y;
    const double bdx = b.x - d.x;
    const double bdy = b.y - d.y;
    const double cdx = c.x - d.x;
    const double cdy = c.y - d.y;

    const double abdet = adx * bdy - bdx * ady;
    const double bcdet = bdx * cdy - cdx * bdy;
    const double cadet = cdx * ady - adx * cdy;

    const double alift = adx * adx + ady * ady;
    const double blift = bdx * bdx + bdy * bdy;
    const double clift = cdx * cdx + cdy * cdy;

    return alift * bcdet + blift * cadet + clift * abdet;
}

bool SamePoint(Point2 a, Point2 b)
{
    return a.x == b.x && a.y == b.y;
}

std::uint64_t ResolveSeed(const std::string& seedText)
{
    if (seedText.empty()) {
        const std::uint64_t clockSeed = static_cast<std::uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::random_device randomDevice;
        const std::uint64_t upper = static_cast<std::uint64_t>(randomDevice()) << 32U;
        return clockSeed ^ upper ^ static_cast<std::uint64_t>(randomDevice());
    }

    try {
        std::size_t consumed = 0;
        const unsigned long long parsed = std::stoull(seedText, &consumed, 10);
        if (consumed == seedText.size()) return static_cast<std::uint64_t>(parsed);
    }
    catch (const std::exception&) {
    }

    return static_cast<std::uint64_t>(std::hash<std::string>{}(seedText));
}

class Builder {
public:
    Builder(const std::vector<Point2>& inputSites, const std::string& seedText)
        : sites(inputSites), inputSeedText(seedText)
    {
        result.siteCount = sites.size();
        result.algorithmImplemented = true;
        scene.points = sites;
        scene.initialVisiblePointCount = sites.size();
        scene.pointStyles.assign(
            sites.size(),
            ScenePointStyle{7.0, SceneColor{35, 88, 135, 255}});
        scene.fitPointIndices.resize(sites.size());
        std::iota(scene.fitPointIndices.begin(), scene.fitPointIndices.end(), std::size_t{0});
    }

    Result Run()
    {
        if (!ValidateInput()) return result;

        result.permutationSeed = ResolveSeed(inputSeedText);
        PrepareWorkPoints();
        if (!CreateInitialTriangle()) return result;

        std::vector<int> permutation = BuildPermutation();
        for (int pointIndex : permutation) {
            InsertPoint(pointIndex);
            if (!result.visualization.succeeded) return result;
        }

        RecordFinalCleanup();
        FillResultEdges();
        BuildDCEL();

        std::ostringstream status;
        status << "Delaunay triangulation: " << result.siteCount << " sites, "
               << finalTriangleCount << " triangles, " << result.triangulationEdges.size()
               << " edges, " << flipCount << " flips, seed " << result.permutationSeed;
        if (inputSeedText.empty()) {
            status << " (random this run).";
        }
        else {
            status << " (from textbox).";
        }
        result.visualization.status = status.str();
        result.visualization.succeeded = true;
        return result;
    }

private:
    const std::vector<Point2>& sites;
    std::string inputSeedText;
    Result result;
    GeometryScene& scene = result.visualization.scene;
    std::vector<WorkPoint> points;
    std::vector<Triangle> triangles;
    std::set<EdgeKey> visibleEdges;
    int topRealIndex = -1;
    int artificialLeft = -1;
    int artificialRight = -1;
    std::size_t flipCount = 0;
    std::size_t finalTriangleCount = 0;

    bool ValidateInput()
    {
        if (sites.size() < 3) {
            Fail("Delaunay triangulation needs at least 3 sites.");
            return false;
        }

        for (std::size_t i = 0; i < sites.size(); ++i) {
            for (std::size_t j = i + 1; j < sites.size(); ++j) {
                if (SamePoint(sites[i], sites[j])) {
                    Fail("Delaunay triangulation rejected duplicate input sites.");
                    return false;
                }
            }
        }

        bool hasArea = false;
        for (std::size_t i = 1; i + 1 < sites.size(); ++i) {
            if (std::abs(Orient(sites[0], sites[i], sites[i + 1])) > kEpsilon) {
                hasArea = true;
                break;
            }
        }
        if (!hasArea) {
            Fail("Delaunay triangulation rejected all-collinear input sites.");
            return false;
        }

        result.visualization.succeeded = true;
        return true;
    }

    void Fail(const std::string& message)
    {
        result.visualization.status = message;
        result.visualization.succeeded = false;
    }

    void PrepareWorkPoints()
    {
        points.clear();
        points.reserve(sites.size() + 2);
        for (Point2 site : sites) {
            points.push_back({site, Rotate(site), false});
        }
    }

    double OrientIndex(int a, int b, int c) const
    {
        return Orient(points[static_cast<std::size_t>(a)].work,
            points[static_cast<std::size_t>(b)].work,
            points[static_cast<std::size_t>(c)].work);
    }

    double InCircleIndex(int a, int b, int c, int d) const
    {
        return InCircle(points[static_cast<std::size_t>(a)].work,
            points[static_cast<std::size_t>(b)].work,
            points[static_cast<std::size_t>(c)].work,
            points[static_cast<std::size_t>(d)].work);
    }

    bool CreateInitialTriangle()
    {
        topRealIndex = 0;
        for (std::size_t i = 1; i < sites.size(); ++i) {
            const Point2 current = points[i].work;
            const Point2 top = points[static_cast<std::size_t>(topRealIndex)].work;
            if (current.y > top.y + kEpsilon ||
                (std::abs(current.y - top.y) <= kEpsilon && current.x < top.x)) {
                topRealIndex = static_cast<int>(i);
            }
        }

        double minX = points.front().work.x;
        double maxX = minX;
        double minY = points.front().work.y;
        double maxY = minY;
        for (const WorkPoint& point : points) {
            minX = std::min(minX, point.work.x);
            maxX = std::max(maxX, point.work.x);
            minY = std::min(minY, point.work.y);
            maxY = std::max(maxY, point.work.y);
        }

        const double span = std::max({maxX - minX, maxY - minY, 1.0});
        Point2 leftWork{};
        Point2 rightWork{};
        bool containsAll = false;
        double horizontal = span * 1024.0 + 1000.0;
        double vertical = span * 8.0 + 1000.0;
        for (int attempt = 0; attempt < 10 && !containsAll; ++attempt) {
            leftWork = {minX - horizontal, minY - vertical};
            rightWork = {maxX + horizontal, minY - vertical};

            const Point2 top = points[static_cast<std::size_t>(topRealIndex)].work;
            if (Orient(leftWork, rightWork, top) < 0.0) {
                std::swap(leftWork, rightWork);
            }

            containsAll = true;
            for (std::size_t i = 0; i < sites.size(); ++i) {
                const Point2 p = points[i].work;
                if (Orient(leftWork, rightWork, p) < -kEpsilon ||
                    Orient(rightWork, top, p) < -kEpsilon ||
                    Orient(top, leftWork, p) < -kEpsilon) {
                    containsAll = false;
                    break;
                }
            }
            horizontal *= 10.0;
            vertical *= 2.0;
        }

        if (!containsAll) {
            Fail("Failed to create an enclosing artificial Delaunay triangle.");
            return false;
        }

        artificialLeft = static_cast<int>(points.size());
        points.push_back({InverseRotate(leftWork), leftWork, true});
        artificialRight = static_cast<int>(points.size());
        points.push_back({InverseRotate(rightWork), rightWork, true});

        scene.points.push_back(points[static_cast<std::size_t>(artificialLeft)].original);
        scene.pointStyles.push_back(ScenePointStyle{5.5, SceneColor{100, 110, 120, 190}});
        scene.points.push_back(points[static_cast<std::size_t>(artificialRight)].original);
        scene.pointStyles.push_back(ScenePointStyle{5.5, SceneColor{100, 110, 120, 190}});

        TimelineEvent showArtificial;
        showArtificial.kind = TimelineEventKind::Point;
        showArtificial.pointAction = PointAction::Show;
        showArtificial.pointIndex = static_cast<std::size_t>(artificialLeft);
        showArtificial.point = points[static_cast<std::size_t>(artificialLeft)].original;
        showArtificial.pointStyle = ScenePointStyle{5.5, SceneColor{100, 110, 120, 190}};
        showArtificial.caption = "Create two artificial vertices and the initial super-triangle.";
        showArtificial.extraPointChanges.push_back({
            PointAction::Show,
            static_cast<std::size_t>(artificialRight),
            points[static_cast<std::size_t>(artificialRight)].original,
            ScenePointStyle{5.5, SceneColor{100, 110, 120, 190}}
        });
        scene.timeline.push_back(showArtificial);

        AddTriangle(artificialLeft, artificialRight, topRealIndex);
        RecordAddEdge(artificialLeft, artificialRight, "Add initial super-triangle edge.");
        RecordAddEdge(artificialRight, topRealIndex, "Add initial super-triangle edge.");
        RecordAddEdge(topRealIndex, artificialLeft, "Add initial super-triangle edge.");
        return true;
    }

    std::vector<int> BuildPermutation() const
    {
        std::vector<int> permutation;
        permutation.reserve(sites.size() - 1);
        for (std::size_t i = 0; i < sites.size(); ++i) {
            if (static_cast<int>(i) != topRealIndex) {
                permutation.push_back(static_cast<int>(i));
            }
        }
        std::mt19937_64 generator(result.permutationSeed);
        std::shuffle(permutation.begin(), permutation.end(), generator);
        return permutation;
    }

    int AddTriangle(int a, int b, int c)
    {
        const double orientation = OrientIndex(a, b, c);
        if (std::abs(orientation) <= kEpsilon) return -1;
        if (orientation < 0.0) std::swap(b, c);
        triangles.push_back({{a, b, c}, true});
        return static_cast<int>(triangles.size() - 1);
    }

    SceneEdge MakeSceneEdge(int a, int b) const
    {
        return SceneEdge{
            Edge2{static_cast<std::size_t>(a), static_cast<std::size_t>(b)},
            EdgeLayer::Result
        };
    }

    bool IsArtificial(int index) const
    {
        return index == artificialLeft || index == artificialRight;
    }

    void RecordAddEdge(int a, int b, const std::string& caption)
    {
        if (a == b) return;
        const EdgeKey key(a, b);
        if (!visibleEdges.insert(key).second) return;
        TimelineEvent event;
        event.action = EdgeAction::Add;
        event.edge = MakeSceneEdge(a, b);
        event.caption = caption;
        scene.timeline.push_back(event);
    }

    void RecordRemoveEdge(int a, int b, const std::string& caption)
    {
        if (a == b) return;
        const EdgeKey key(a, b);
        if (visibleEdges.erase(key) == 0) return;
        TimelineEvent event;
        event.action = EdgeAction::Remove;
        event.edge = MakeSceneEdge(a, b);
        event.caption = caption;
        scene.timeline.push_back(event);
    }

    void RecordReplaceEdge(int oldA, int oldB, int newA, int newB)
    {
        if (oldA == oldB || newA == newB) return;
        const EdgeKey oldKey(oldA, oldB);
        const EdgeKey newKey(newA, newB);
        if (oldKey.first == newKey.first && oldKey.second == newKey.second) return;

        const bool hadOld = visibleEdges.erase(oldKey) > 0;
        const bool insertedNew = visibleEdges.insert(newKey).second;
        if (hadOld && insertedNew) {
            TimelineEvent event;
            event.action = EdgeAction::Replace;
            event.edge = MakeSceneEdge(oldA, oldB);
            event.replacementEdge = MakeSceneEdge(newA, newB);
            event.caption = "Replace illegal edge by a Delaunay edge.";
            scene.timeline.push_back(event);
        }
        else if (hadOld) {
            TimelineEvent event;
            event.action = EdgeAction::Remove;
            event.edge = MakeSceneEdge(oldA, oldB);
            event.caption = "Remove illegal edge.";
            scene.timeline.push_back(event);
        }
        else if (insertedNew) {
            TimelineEvent event;
            event.action = EdgeAction::Add;
            event.edge = MakeSceneEdge(newA, newB);
            event.caption = "Add Delaunay edge after legalization.";
            scene.timeline.push_back(event);
        }
    }

    bool TriangleContainsVertex(const Triangle& triangle, int vertex) const
    {
        return triangle.v[0] == vertex || triangle.v[1] == vertex || triangle.v[2] == vertex;
    }

    bool TriangleHasEdge(const Triangle& triangle, int a, int b) const
    {
        return TriangleContainsVertex(triangle, a) && TriangleContainsVertex(triangle, b);
    }

    std::vector<int> TrianglesWithEdge(int a, int b) const
    {
        std::vector<int> adjacent;
        for (std::size_t i = 0; i < triangles.size(); ++i) {
            const Triangle& triangle = triangles[i];
            if (triangle.alive && TriangleHasEdge(triangle, a, b)) {
                adjacent.push_back(static_cast<int>(i));
            }
        }
        return adjacent;
    }

    int OppositeVertex(const Triangle& triangle, int a, int b) const
    {
        for (int vertex : triangle.v) {
            if (vertex != a && vertex != b) return vertex;
        }
        return -1;
    }

    PointLocation LocatePoint(int pointIndex) const
    {
        const Point2 p = points[static_cast<std::size_t>(pointIndex)].work;
        for (std::size_t i = 0; i < triangles.size(); ++i) {
            const Triangle& triangle = triangles[i];
            if (!triangle.alive) continue;

            const std::array<double, 3> orientations = {
                Orient(points[static_cast<std::size_t>(triangle.v[0])].work,
                    points[static_cast<std::size_t>(triangle.v[1])].work, p),
                Orient(points[static_cast<std::size_t>(triangle.v[1])].work,
                    points[static_cast<std::size_t>(triangle.v[2])].work, p),
                Orient(points[static_cast<std::size_t>(triangle.v[2])].work,
                    points[static_cast<std::size_t>(triangle.v[0])].work, p)
            };

            if (orientations[0] >= -kEpsilon &&
                orientations[1] >= -kEpsilon &&
                orientations[2] >= -kEpsilon) {
                for (int edge = 0; edge < 3; ++edge) {
                    if (std::abs(orientations[static_cast<std::size_t>(edge)]) <= kEpsilon) {
                        return {
                            LocationKind::OnEdge,
                            static_cast<int>(i),
                            triangle.v[static_cast<std::size_t>(edge)],
                            triangle.v[static_cast<std::size_t>((edge + 1) % 3)]
                        };
                    }
                }
                return {LocationKind::InsideTriangle, static_cast<int>(i), -1, -1};
            }
        }
        return {};
    }

    void InsertPoint(int pointIndex)
    {
        const PointLocation location = LocatePoint(pointIndex);
        if (location.kind == LocationKind::Outside) {
            std::ostringstream message;
            message << "Failed to locate containing triangle for site " << pointIndex << '.';
            Fail(message.str());
            return;
        }

        std::ostringstream caption;
        caption << "Insert site " << pointIndex << '.';
        TimelineEvent restyle;
        restyle.kind = TimelineEventKind::Point;
        restyle.pointAction = PointAction::Restyle;
        restyle.pointIndex = static_cast<std::size_t>(pointIndex);
        restyle.point = points[static_cast<std::size_t>(pointIndex)].original;
        restyle.pointStyle = ScenePointStyle{8.5, SceneColor{232, 139, 45, 255}};
        restyle.caption = caption.str();
        restyle.extraPointChanges.push_back({
            PointAction::Restyle,
            static_cast<std::size_t>(pointIndex),
            points[static_cast<std::size_t>(pointIndex)].original,
            ScenePointStyle{7.0, SceneColor{35, 88, 135, 255}}
        });
        scene.timeline.push_back(restyle);

        if (location.kind == LocationKind::InsideTriangle) {
            SplitTriangle(location.triangle, pointIndex);
        }
        else {
            SplitEdge(location, pointIndex);
        }
    }

    void SplitTriangle(int triangleIndex, int pointIndex)
    {
        Triangle old = triangles[static_cast<std::size_t>(triangleIndex)];
        triangles[static_cast<std::size_t>(triangleIndex)].alive = false;

        const int a = old.v[0];
        const int b = old.v[1];
        const int c = old.v[2];

        AddTriangle(a, b, pointIndex);
        AddTriangle(b, c, pointIndex);
        AddTriangle(c, a, pointIndex);

        RecordAddEdge(a, pointIndex, "Connect inserted site to containing triangle vertex.");
        RecordAddEdge(b, pointIndex, "Connect inserted site to containing triangle vertex.");
        RecordAddEdge(c, pointIndex, "Connect inserted site to containing triangle vertex.");

        LegalizeEdge(pointIndex, a, b);
        LegalizeEdge(pointIndex, b, c);
        LegalizeEdge(pointIndex, c, a);
    }

    void SplitEdge(const PointLocation& location, int pointIndex)
    {
        const int a = location.edgeA;
        const int b = location.edgeB;
        std::vector<int> adjacent = TrianglesWithEdge(a, b);
        if (adjacent.empty()) {
            Fail("Located an edge that is no longer present in the triangulation.");
            return;
        }

        RecordRemoveEdge(a, b, "Split edge that contains the inserted site.");

        if (adjacent.size() == 1) {
            Triangle first = triangles[static_cast<std::size_t>(adjacent[0])];
            triangles[static_cast<std::size_t>(adjacent[0])].alive = false;
            const int c = OppositeVertex(first, a, b);

            AddTriangle(a, pointIndex, c);
            AddTriangle(pointIndex, b, c);

            RecordAddEdge(a, pointIndex, "Connect inserted site along split boundary edge.");
            RecordAddEdge(pointIndex, b, "Connect inserted site along split boundary edge.");
            RecordAddEdge(pointIndex, c, "Connect inserted site to opposite vertex.");

            LegalizeEdge(pointIndex, c, a);
            LegalizeEdge(pointIndex, b, c);
            return;
        }

        Triangle first = triangles[static_cast<std::size_t>(adjacent[0])];
        Triangle second = triangles[static_cast<std::size_t>(adjacent[1])];
        triangles[static_cast<std::size_t>(adjacent[0])].alive = false;
        triangles[static_cast<std::size_t>(adjacent[1])].alive = false;

        const int c = OppositeVertex(first, a, b);
        const int d = OppositeVertex(second, a, b);

        AddTriangle(a, pointIndex, c);
        AddTriangle(pointIndex, b, c);
        AddTriangle(b, pointIndex, d);
        AddTriangle(pointIndex, a, d);

        RecordAddEdge(a, pointIndex, "Connect inserted site along split internal edge.");
        RecordAddEdge(pointIndex, b, "Connect inserted site along split internal edge.");
        RecordAddEdge(pointIndex, c, "Connect inserted site to first opposite vertex.");
        RecordAddEdge(pointIndex, d, "Connect inserted site to second opposite vertex.");

        LegalizeEdge(pointIndex, a, c);
        LegalizeEdge(pointIndex, c, b);
        LegalizeEdge(pointIndex, b, d);
        LegalizeEdge(pointIndex, d, a);
    }

    void LegalizeEdge(int pointIndex, int a, int b)
    {
        if (a == b || a == pointIndex || b == pointIndex) return;

        const std::vector<int> adjacent = TrianglesWithEdge(a, b);
        if (adjacent.size() != 2) return;

        int triangleWithPoint = -1;
        int oppositeTriangle = -1;
        for (int triangleIndex : adjacent) {
            if (TriangleContainsVertex(triangles[static_cast<std::size_t>(triangleIndex)], pointIndex)) {
                triangleWithPoint = triangleIndex;
            }
            else {
                oppositeTriangle = triangleIndex;
            }
        }
        if (triangleWithPoint < 0 || oppositeTriangle < 0) return;

        const int q = OppositeVertex(triangles[static_cast<std::size_t>(oppositeTriangle)], a, b);
        if (q < 0 || q == pointIndex) return;

        const double orientation = OrientIndex(a, b, pointIndex);
        if (std::abs(orientation) <= kEpsilon) return;
        const double incircle = orientation > 0.0
            ? InCircleIndex(a, b, pointIndex, q)
            : InCircleIndex(b, a, pointIndex, q);

        if (incircle <= kEpsilon) return;

        triangles[static_cast<std::size_t>(triangleWithPoint)].alive = false;
        triangles[static_cast<std::size_t>(oppositeTriangle)].alive = false;

        RecordReplaceEdge(a, b, pointIndex, q);
        ++flipCount;

        AddTriangle(a, q, pointIndex);
        AddTriangle(q, b, pointIndex);

        LegalizeEdge(pointIndex, a, q);
        LegalizeEdge(pointIndex, q, b);
    }

    void RecordFinalCleanup()
    {
        std::vector<EdgeKey> artificialEdges;
        for (const EdgeKey& edge : visibleEdges) {
            if (IsArtificial(edge.first) || IsArtificial(edge.second)) {
                artificialEdges.push_back(edge);
            }
        }
        for (const EdgeKey& edge : artificialEdges) {
            visibleEdges.erase(edge);
        }

        TimelineEvent cleanup;
        cleanup.caption = "Remove artificial vertices and all remaining super-triangle edges.";
        if (!artificialEdges.empty()) {
            cleanup.kind = TimelineEventKind::Edge;
            cleanup.action = EdgeAction::Remove;
            cleanup.edge = MakeSceneEdge(artificialEdges.front().first, artificialEdges.front().second);
            for (std::size_t i = 1; i < artificialEdges.size(); ++i) {
                cleanup.extraEdgeChanges.push_back({
                    EdgeAction::Remove,
                    MakeSceneEdge(artificialEdges[i].first, artificialEdges[i].second),
                    {}
                });
            }
        }
        else {
            cleanup.kind = TimelineEventKind::Point;
            cleanup.pointAction = PointAction::Hide;
            cleanup.pointIndex = static_cast<std::size_t>(artificialLeft);
        }

        cleanup.extraPointChanges.push_back({
            PointAction::Hide,
            static_cast<std::size_t>(artificialLeft),
            points[static_cast<std::size_t>(artificialLeft)].original,
            ScenePointStyle{}
        });
        cleanup.extraPointChanges.push_back({
            PointAction::Hide,
            static_cast<std::size_t>(artificialRight),
            points[static_cast<std::size_t>(artificialRight)].original,
            ScenePointStyle{}
        });
        scene.timeline.push_back(cleanup);
    }

    void FillResultEdges()
    {
        result.triangulationEdges.clear();
        for (const EdgeKey& edge : visibleEdges) {
            if (IsArtificial(edge.first) || IsArtificial(edge.second)) continue;
            result.triangulationEdges.push_back({
                static_cast<std::size_t>(edge.first),
                static_cast<std::size_t>(edge.second)
            });
        }
    }

    void BuildDCEL()
    {
        std::vector<Triangle> finalTriangles;
        for (const Triangle& triangle : triangles) {
            if (!triangle.alive) continue;
            if (IsArtificial(triangle.v[0]) || IsArtificial(triangle.v[1]) || IsArtificial(triangle.v[2])) {
                continue;
            }
            finalTriangles.push_back(triangle);
        }
        finalTriangleCount = finalTriangles.size();

        result.dcelVertices.clear();
        result.dcelVertices.reserve(sites.size());
        for (Point2 site : sites) {
            result.dcelVertices.push_back({
                static_cast<int>(std::lround(site.x)),
                static_cast<int>(std::lround(site.y)),
                {}
            });
        }

        DCEL& dcel = result.triangulation;
        dcel.halfEdges.clear();
        dcel.halfEdges.resize(finalTriangles.size() * 3U);
        std::map<std::pair<int, int>, std::size_t> directedEdges;

        for (std::size_t triangleIndex = 0; triangleIndex < finalTriangles.size(); ++triangleIndex) {
            const Triangle& triangle = finalTriangles[triangleIndex];
            const std::size_t base = triangleIndex * 3U;
            for (std::size_t edge = 0; edge < 3U; ++edge) {
                const int origin = triangle.v[edge];
                const int destination = triangle.v[(edge + 1U) % 3U];
                HalfEdge& halfEdge = dcel.halfEdges[base + edge];
                halfEdge.origin = &result.dcelVertices[static_cast<std::size_t>(origin)];
                halfEdge.face = &dcel.interiorFace;
                halfEdge.next = &dcel.halfEdges[base + ((edge + 1U) % 3U)];
                halfEdge.prev = &dcel.halfEdges[base + ((edge + 2U) % 3U)];
                result.dcelVertices[static_cast<std::size_t>(origin)].incidentEdges.push_back(&halfEdge);
                directedEdges[{origin, destination}] = base + edge;
            }
        }

        for (const auto& item : directedEdges) {
            const int origin = item.first.first;
            const int destination = item.first.second;
            const auto twin = directedEdges.find({destination, origin});
            if (twin != directedEdges.end()) {
                dcel.halfEdges[item.second].twin = &dcel.halfEdges[twin->second];
            }
        }

        if (!dcel.halfEdges.empty()) {
            dcel.interiorFace.boundary = &dcel.halfEdges.front();
        }
    }
};

} // namespace

Result Run(const std::vector<Point2>& sites, const std::string& seedText)
{
    Builder builder(sites, seedText);
    return builder.Run();
}

} // namespace delaunay_triangulation

#ifndef DELAUNAY_TRIANGULATION_NO_MAIN
int main()
{
    try {
        const std::vector<Vertex> vertices = ReadVerticesFromFile("input.txt");
        std::vector<Point2> sites;
        sites.reserve(vertices.size());
        for (const Vertex& vertex : vertices) {
            sites.push_back({
                static_cast<double>(vertex.x),
                static_cast<double>(vertex.y)
            });
        }

        const delaunay_triangulation::Result result =
            delaunay_triangulation::Run(sites);

        std::cout << result.visualization.status << '\n';
        std::cout << "Loaded sites: " << result.siteCount << '\n';
        std::cout << "Permutation seed: " << result.permutationSeed << '\n';
        std::cout << "DCEL half-edges: "
                  << result.triangulation.halfEdges.size() << '\n';
        std::cout << "Drawable triangulation edges: "
                  << result.triangulationEdges.size() << '\n';
        for (const Edge2 edge : result.triangulationEdges) {
            std::cout << "edge " << edge.first << ' ' << edge.second << '\n';
        }
        return result.visualization.succeeded ? 0 : 2;
    }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
#endif