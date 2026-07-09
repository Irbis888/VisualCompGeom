#include "FortuneAPI.h"

#include "Commons.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr double Epsilon = 1.0e-8;
constexpr std::size_t NoIndex = std::numeric_limits<std::size_t>::max();

double SquaredDistance(Point2 first, Point2 second)
{
    const double dx = first.x - second.x;
    const double dy = first.y - second.y;
    return dx * dx + dy * dy;
}

double Orient(Point2 first, Point2 second, Point2 third)
{
    return (second.x - first.x) * (third.y - first.y) -
        (second.y - first.y) * (third.x - first.x);
}

bool SamePoint(Point2 first, Point2 second)
{
    return SquaredDistance(first, second) <= Epsilon * Epsilon;
}

std::string FormatPoint(Point2 point)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(3)
        << '(' << point.x << ", " << point.y << ')';
    return out.str();
}

enum class FortuneEventKind {
    Site,
    Circle
};

struct Site {
    Point2 position{};
    std::size_t inputIndex = 0;
};

struct RawEdge {
    Site* leftSite = nullptr;
    Site* rightSite = nullptr;
    std::size_t firstVertex = NoIndex;
    std::size_t secondVertex = NoIndex;
    bool hasRayDirection = false;
    Point2 rayDirection{};
};

struct RawSegment {
    Point2 first{};
    Point2 second{};
    Site* leftSite = nullptr;
    Site* rightSite = nullptr;
};

struct RawDiagram {
    std::vector<Point2> vertices;
    std::vector<std::unique_ptr<RawEdge>> edges;

    RawEdge* CreateEdge(Site* leftSite, Site* rightSite)
    {
        auto edge = std::make_unique<RawEdge>();
        edge->leftSite = leftSite;
        edge->rightSite = rightSite;
        RawEdge* raw = edge.get();
        edges.push_back(std::move(edge));
        return raw;
    }

    std::size_t CreateVertex(Point2 point)
    {
        vertices.push_back(point);
        return vertices.size() - 1;
    }

    void AttachVertex(
        RawEdge* edge,
        std::size_t vertexIndex,
        Site* rejectedSite = nullptr)
    {
        if (edge == nullptr || vertexIndex >= vertices.size()) return;
        if (edge->firstVertex == vertexIndex || edge->secondVertex == vertexIndex) return;

        if (edge->firstVertex == NoIndex) {
            edge->firstVertex = vertexIndex;
            SetRayDirectionAwayFromSite(edge, vertices[vertexIndex], rejectedSite);
            return;
        }
        if (edge->secondVertex == NoIndex) {
            edge->secondVertex = vertexIndex;
        }
    }

private:
    static Point2 Normalize(Point2 vector)
    {
        const double length = std::sqrt(vector.x * vector.x + vector.y * vector.y);
        if (length < Epsilon) return {};
        return {vector.x / length, vector.y / length};
    }

    static Point2 BisectorDirection(const RawEdge& edge)
    {
        if (edge.leftSite == nullptr || edge.rightSite == nullptr) return {};
        const Point2 left = edge.leftSite->position;
        const Point2 right = edge.rightSite->position;
        return Normalize({-(right.y - left.y), right.x - left.x});
    }

    static Point2 Add(Point2 point, Point2 direction, double scale)
    {
        return {point.x + direction.x * scale, point.y + direction.y * scale};
    }

    static void SetRayDirectionAwayFromSite(
        RawEdge* edge,
        Point2 vertex,
        Site* rejectedSite)
    {
        if (edge == nullptr) return;
        Point2 direction = BisectorDirection(*edge);
        if (std::abs(direction.x) < Epsilon && std::abs(direction.y) < Epsilon) return;

        if (rejectedSite != nullptr && edge->leftSite != nullptr) {
            const double probe = 1.0;
            const Point2 forward = Add(vertex, direction, probe);
            const Point2 backward = Add(vertex, direction, -probe);

            const double forwardMargin =
                SquaredDistance(forward, edge->leftSite->position) -
                SquaredDistance(forward, rejectedSite->position);
            const double backwardMargin =
                SquaredDistance(backward, edge->leftSite->position) -
                SquaredDistance(backward, rejectedSite->position);

            // Keep the side where the edge's two sites are closer than the
            // third site that caused the circle event.
            if (backwardMargin < forwardMargin) {
                direction = {-direction.x, -direction.y};
            }
        }

        edge->rayDirection = direction;
        edge->hasRayDirection = true;
    }
};

struct BeachNode;

struct CircleEvent {
    Point2 bottom{};
    BeachNode* disappearingArc = nullptr;
    bool valid = true;
};

struct FortuneEvent {
    FortuneEventKind kind = FortuneEventKind::Site;
    Point2 point{};
    double priorityY = 0.0;
    Site* site = nullptr;
    CircleEvent* circleEvent = nullptr;
};

struct FortuneEventPriority {
    bool operator()(const FortuneEvent& lhs, const FortuneEvent& rhs) const
    {
        if (std::abs(lhs.priorityY - rhs.priorityY) > Epsilon) {
            return lhs.priorityY < rhs.priorityY;
        }
        if (std::abs(lhs.point.x - rhs.point.x) > Epsilon) {
            return lhs.point.x > rhs.point.x;
        }
        if (lhs.kind != rhs.kind) return lhs.kind == FortuneEventKind::Circle;

        const std::size_t lhsIndex = lhs.site == nullptr
            ? std::numeric_limits<std::size_t>::max()
            : lhs.site->inputIndex;
        const std::size_t rhsIndex = rhs.site == nullptr
            ? std::numeric_limits<std::size_t>::max()
            : rhs.site->inputIndex;
        return lhsIndex > rhsIndex;
    }
};

using FortuneEventQueue =
    std::priority_queue<FortuneEvent, std::vector<FortuneEvent>, FortuneEventPriority>;

struct BeachNode {
    bool isLeaf = true;

    // Leaf: one symbolic beach-line arc, defined only by this site.
    Site* site = nullptr;
    CircleEvent* circleEvent = nullptr;

    // Internal node: symbolic breakpoint tuple <leftSite, rightSite>.
    Site* leftSite = nullptr;
    Site* rightSite = nullptr;
    RawEdge* rawEdge = nullptr;
    HalfEdge* halfEdge = nullptr;

    BeachNode* left = nullptr;
    BeachNode* right = nullptr;
    BeachNode* parent = nullptr;
    BeachNode* prevLeaf = nullptr;
    BeachNode* nextLeaf = nullptr;
    int height = 1;
};

struct ArcSplit {
    BeachNode* leftArc = nullptr;
    BeachNode* middleArc = nullptr;
    BeachNode* rightArc = nullptr;
};

struct CircleGeometry {
    Point2 center{};
    Point2 bottom{};
    double radius = 0.0;
};

double ParabolaYAtX(const Site& site, double directrixY, double x)
{
    const double denominator = 2.0 * (site.position.y - directrixY);
    if (std::abs(denominator) < Epsilon) {
        return std::numeric_limits<double>::infinity();
    }
    return ((x - site.position.x) * (x - site.position.x) +
        site.position.y * site.position.y - directrixY * directrixY) / denominator;
}

double BreakpointX(const Site& leftSite, const Site& rightSite, double sweepY)
{
    const Point2 left = leftSite.position;
    const Point2 right = rightSite.position;
    const double leftDistance = left.y - sweepY;
    const double rightDistance = right.y - sweepY;

    if (std::abs(left.y - right.y) < Epsilon) {
        return (left.x + right.x) * 0.5;
    }
    if (std::abs(leftDistance) < Epsilon) return left.x;
    if (std::abs(rightDistance) < Epsilon) return right.x;

    const double aLeft = 1.0 / (2.0 * leftDistance);
    const double bLeft = -left.x / leftDistance;
    const double cLeft =
        (left.x * left.x + left.y * left.y - sweepY * sweepY) / (2.0 * leftDistance);

    const double aRight = 1.0 / (2.0 * rightDistance);
    const double bRight = -right.x / rightDistance;
    const double cRight =
        (right.x * right.x + right.y * right.y - sweepY * sweepY) / (2.0 * rightDistance);

    const double a = aLeft - aRight;
    const double b = bLeft - bRight;
    const double c = cLeft - cRight;

    if (std::abs(a) < Epsilon) {
        if (std::abs(b) < Epsilon) return (left.x + right.x) * 0.5;
        return -c / b;
    }

    const double discriminant = std::max(0.0, b * b - 4.0 * a * c);
    const double sqrtDiscriminant = std::sqrt(discriminant);
    const double firstRoot = (-b - sqrtDiscriminant) / (2.0 * a);
    const double secondRoot = (-b + sqrtDiscriminant) / (2.0 * a);

    const auto separatesTuple = [&](double root) {
        const double offset = std::max(1.0e-5, std::abs(root) * 1.0e-7);
        const double before = root - offset;
        const double after = root + offset;
        return ParabolaYAtX(leftSite, sweepY, before) <=
            ParabolaYAtX(rightSite, sweepY, before) &&
            ParabolaYAtX(rightSite, sweepY, after) <=
            ParabolaYAtX(leftSite, sweepY, after);
    };

    if (separatesTuple(firstRoot)) return firstRoot;
    if (separatesTuple(secondRoot)) return secondRoot;
    return left.x < right.x ? std::min(firstRoot, secondRoot) : std::max(firstRoot, secondRoot);
}

bool ComputeCircle(Site* left, Site* middle, Site* right, CircleGeometry& circle)
{
    if (left == nullptr || middle == nullptr || right == nullptr) return false;
    if (left == right || left == middle || middle == right) return false;
    if (SamePoint(left->position, middle->position) ||
        SamePoint(middle->position, right->position) ||
        SamePoint(left->position, right->position)) {
        return false;
    }

    // For a top-to-bottom sweep in ordinary y-up coordinates, only clockwise
    // triples of consecutive arcs produce a disappearing middle arc.
    if (Orient(left->position, middle->position, right->position) >= -Epsilon) {
        return false;
    }

    const double ax = left->position.x;
    const double ay = left->position.y;
    const double bx = middle->position.x;
    const double by = middle->position.y;
    const double cx = right->position.x;
    const double cy = right->position.y;

    const double determinant = 2.0 *
        (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::abs(determinant) < Epsilon) return false;

    const double ax2ay2 = ax * ax + ay * ay;
    const double bx2by2 = bx * bx + by * by;
    const double cx2cy2 = cx * cx + cy * cy;

    circle.center.x =
        (ax2ay2 * (by - cy) + bx2by2 * (cy - ay) + cx2cy2 * (ay - by)) /
        determinant;
    circle.center.y =
        (ax2ay2 * (cx - bx) + bx2by2 * (ax - cx) + cx2cy2 * (bx - ax)) /
        determinant;
    circle.radius = std::sqrt(SquaredDistance(circle.center, middle->position));
    circle.bottom = {circle.center.x, circle.center.y - circle.radius};
    return std::isfinite(circle.bottom.x) && std::isfinite(circle.bottom.y);
}

class BeachLineTree {
public:
    bool Empty() const
    {
        return root_ == nullptr;
    }

    BeachNode* Initialize(Site* site)
    {
        root_ = CreateLeaf(site);
        return root_;
    }

    BeachNode* FindArcAbove(const Point2& point, double sweepY) const
    {
        BeachNode* current = root_;
        while (current != nullptr && !current->isLeaf) {
            const double breakpoint =
                BreakpointX(*current->leftSite, *current->rightSite, sweepY);
            current = point.x < breakpoint ? current->left : current->right;
        }
        return current;
    }

    ArcSplit ReplaceArcByThree(BeachNode* arc, Site* site)
    {
        ArcSplit split;
        if (arc == nullptr) {
            split.middleArc = Initialize(site);
            return split;
        }
        if (!arc->isLeaf || arc->site == nullptr) return split;

        Site* oldSite = arc->site;
        BeachNode* oldPrev = arc->prevLeaf;
        BeachNode* oldNext = arc->nextLeaf;

        split.leftArc = CreateLeaf(oldSite);
        split.middleArc = CreateLeaf(site);
        split.rightArc = CreateLeaf(oldSite);

        split.leftArc->prevLeaf = oldPrev;
        split.leftArc->nextLeaf = split.middleArc;
        split.middleArc->prevLeaf = split.leftArc;
        split.middleArc->nextLeaf = split.rightArc;
        split.rightArc->prevLeaf = split.middleArc;
        split.rightArc->nextLeaf = oldNext;
        if (oldPrev != nullptr) oldPrev->nextLeaf = split.leftArc;
        if (oldNext != nullptr) oldNext->prevLeaf = split.rightArc;

        TransferBreakpoint(oldPrev, arc, oldPrev, split.leftArc);
        TransferBreakpoint(arc, oldNext, split.rightArc, oldNext);

        BeachNode* rightBreakpoint = CreateBreakpoint(site, oldSite);
        rightBreakpoint->left = split.middleArc;
        rightBreakpoint->right = split.rightArc;
        split.middleArc->parent = rightBreakpoint;
        split.rightArc->parent = rightBreakpoint;

        BeachNode* leftBreakpoint = CreateBreakpoint(oldSite, site);
        leftBreakpoint->left = split.leftArc;
        leftBreakpoint->right = rightBreakpoint;
        split.leftArc->parent = leftBreakpoint;
        rightBreakpoint->parent = leftBreakpoint;

        UpdateNode(rightBreakpoint);
        UpdateNode(leftBreakpoint);
        ReplaceNode(arc, leftBreakpoint);
        RebalanceFrom(leftBreakpoint);
        RefreshAll();
        return split;
    }

    BeachNode* PrevArc(BeachNode* arc) const
    {
        return arc == nullptr ? nullptr : arc->prevLeaf;
    }

    BeachNode* NextArc(BeachNode* arc) const
    {
        return arc == nullptr ? nullptr : arc->nextLeaf;
    }

    void SetBreakpointEdge(BeachNode* leftArc, BeachNode* rightArc, RawEdge* edge)
    {
        if (leftArc == nullptr || rightArc == nullptr || edge == nullptr) return;
        for (BreakpointRecord& record : breakpoints_) {
            if (record.active && record.leftArc == leftArc && record.rightArc == rightArc) {
                record.rawEdge = edge;
                RefreshAll();
                return;
            }
        }
        breakpoints_.push_back({leftArc, rightArc, edge, true});
        RefreshAll();
    }

    RawEdge* GetBreakpointEdge(BeachNode* leftArc, BeachNode* rightArc) const
    {
        for (const BreakpointRecord& record : breakpoints_) {
            if (record.active && record.leftArc == leftArc && record.rightArc == rightArc) {
                return record.rawEdge;
            }
        }
        return nullptr;
    }

    void RemoveArc(BeachNode* arc)
    {
        if (arc == nullptr || !arc->isLeaf) return;
        BeachNode* previous = arc->prevLeaf;
        BeachNode* next = arc->nextLeaf;

        RemoveBreakpoint(previous, arc);
        RemoveBreakpoint(arc, next);

        if (previous != nullptr) previous->nextLeaf = next;
        if (next != nullptr) next->prevLeaf = previous;

        BeachNode* parent = arc->parent;
        if (parent == nullptr) {
            root_ = nullptr;
            return;
        }

        BeachNode* sibling = parent->left == arc ? parent->right : parent->left;
        BeachNode* grandparent = parent->parent;
        if (sibling != nullptr) sibling->parent = grandparent;
        if (grandparent == nullptr) {
            root_ = sibling;
        }
        else if (grandparent->left == parent) {
            grandparent->left = sibling;
        }
        else {
            grandparent->right = sibling;
        }

        RebalanceFrom(sibling != nullptr ? sibling : grandparent);
        RefreshAll();
    }

    std::vector<Site*> ArcSitesLeftToRight() const
    {
        std::vector<Site*> sites;
        CollectLeaves(root_, sites);
        return sites;
    }

private:
    struct BreakpointRecord {
        BeachNode* leftArc = nullptr;
        BeachNode* rightArc = nullptr;
        RawEdge* rawEdge = nullptr;
        bool active = false;
    };

    BeachNode* CreateLeaf(Site* site)
    {
        auto node = std::make_unique<BeachNode>();
        node->isLeaf = true;
        node->site = site;
        BeachNode* raw = node.get();
        nodes_.push_back(std::move(node));
        return raw;
    }

    BeachNode* CreateBreakpoint(Site* leftSite, Site* rightSite)
    {
        auto node = std::make_unique<BeachNode>();
        node->isLeaf = false;
        node->leftSite = leftSite;
        node->rightSite = rightSite;
        BeachNode* raw = node.get();
        nodes_.push_back(std::move(node));
        return raw;
    }

    void TransferBreakpoint(
        BeachNode* oldLeft,
        BeachNode* oldRight,
        BeachNode* newLeft,
        BeachNode* newRight)
    {
        if (oldLeft == nullptr || oldRight == nullptr || newLeft == nullptr || newRight == nullptr) {
            return;
        }
        for (BreakpointRecord& record : breakpoints_) {
            if (record.active && record.leftArc == oldLeft && record.rightArc == oldRight) {
                record.leftArc = newLeft;
                record.rightArc = newRight;
                return;
            }
        }
    }

    void RemoveBreakpoint(BeachNode* leftArc, BeachNode* rightArc)
    {
        if (leftArc == nullptr || rightArc == nullptr) return;
        for (BreakpointRecord& record : breakpoints_) {
            if (record.active && record.leftArc == leftArc && record.rightArc == rightArc) {
                record.active = false;
                return;
            }
        }
    }

    RawEdge* FindRecordEdge(BeachNode* leftArc, BeachNode* rightArc) const
    {
        if (leftArc == nullptr || rightArc == nullptr) return nullptr;
        for (const BreakpointRecord& record : breakpoints_) {
            if (record.active && record.leftArc == leftArc && record.rightArc == rightArc) {
                return record.rawEdge;
            }
        }
        return nullptr;
    }

    void ReplaceNode(BeachNode* oldNode, BeachNode* replacement)
    {
        replacement->parent = oldNode->parent;
        if (oldNode->parent == nullptr) {
            root_ = replacement;
            return;
        }
        if (oldNode->parent->left == oldNode) {
            oldNode->parent->left = replacement;
        }
        else {
            oldNode->parent->right = replacement;
        }
    }

    static int Height(const BeachNode* node)
    {
        return node == nullptr ? 0 : node->height;
    }

    static BeachNode* LeftmostLeaf(BeachNode* node)
    {
        if (node == nullptr) return nullptr;
        while (!node->isLeaf && node->left != nullptr) node = node->left;
        return node;
    }

    static BeachNode* RightmostLeaf(BeachNode* node)
    {
        if (node == nullptr) return nullptr;
        while (!node->isLeaf && node->right != nullptr) node = node->right;
        return node;
    }

    void RefreshNode(BeachNode* node)
    {
        if (node == nullptr) return;
        RefreshNode(node->left);
        RefreshNode(node->right);
        if (!node->isLeaf) {
            BeachNode* leftArc = RightmostLeaf(node->left);
            BeachNode* rightArc = LeftmostLeaf(node->right);
            node->leftSite = leftArc == nullptr ? nullptr : leftArc->site;
            node->rightSite = rightArc == nullptr ? nullptr : rightArc->site;
            node->rawEdge = FindRecordEdge(leftArc, rightArc);
        }
        node->height = 1 + std::max(Height(node->left), Height(node->right));
    }

    void RefreshAll()
    {
        RefreshNode(root_);
    }

    static int BalanceFactor(const BeachNode* node)
    {
        return node == nullptr ? 0 : Height(node->left) - Height(node->right);
    }

    void UpdateNode(BeachNode* node)
    {
        if (node == nullptr) return;
        if (!node->isLeaf) {
            BeachNode* leftArc = RightmostLeaf(node->left);
            BeachNode* rightArc = LeftmostLeaf(node->right);
            node->leftSite = leftArc == nullptr ? nullptr : leftArc->site;
            node->rightSite = rightArc == nullptr ? nullptr : rightArc->site;
            node->rawEdge = FindRecordEdge(leftArc, rightArc);
        }
        node->height = 1 + std::max(Height(node->left), Height(node->right));
    }

    BeachNode* RotateLeft(BeachNode* pivot)
    {
        BeachNode* child = pivot->right;
        if (child == nullptr) return pivot;

        BeachNode* parent = pivot->parent;
        pivot->right = child->left;
        if (pivot->right != nullptr) pivot->right->parent = pivot;

        child->left = pivot;
        pivot->parent = child;
        child->parent = parent;

        if (parent == nullptr) {
            root_ = child;
        }
        else if (parent->left == pivot) {
            parent->left = child;
        }
        else {
            parent->right = child;
        }

        UpdateNode(pivot);
        UpdateNode(child);
        return child;
    }

    BeachNode* RotateRight(BeachNode* pivot)
    {
        BeachNode* child = pivot->left;
        if (child == nullptr) return pivot;

        BeachNode* parent = pivot->parent;
        pivot->left = child->right;
        if (pivot->left != nullptr) pivot->left->parent = pivot;

        child->right = pivot;
        pivot->parent = child;
        child->parent = parent;

        if (parent == nullptr) {
            root_ = child;
        }
        else if (parent->left == pivot) {
            parent->left = child;
        }
        else {
            parent->right = child;
        }

        UpdateNode(pivot);
        UpdateNode(child);
        return child;
    }

    BeachNode* RebalanceNode(BeachNode* node)
    {
        UpdateNode(node);
        const int balance = BalanceFactor(node);

        if (balance > 1) {
            if (BalanceFactor(node->left) < 0) RotateLeft(node->left);
            return RotateRight(node);
        }
        if (balance < -1) {
            if (BalanceFactor(node->right) > 0) RotateRight(node->right);
            return RotateLeft(node);
        }
        return node;
    }

    void RebalanceFrom(BeachNode* node)
    {
        while (node != nullptr) {
            BeachNode* rebalanced = RebalanceNode(node);
            node = rebalanced->parent;
        }
    }

    static void CollectLeaves(BeachNode* node, std::vector<Site*>& sites)
    {
        if (node == nullptr) return;
        if (node->isLeaf) {
            if (node->site != nullptr) sites.push_back(node->site);
            return;
        }
        CollectLeaves(node->left, sites);
        CollectLeaves(node->right, sites);
    }

    BeachNode* root_ = nullptr;
    std::vector<std::unique_ptr<BeachNode>> nodes_;
    std::vector<BreakpointRecord> breakpoints_;
};

struct BoundingBox {
    double minX = -1.0;
    double maxX = 1.0;
    double minY = -1.0;
    double maxY = 1.0;
};

struct FortuneState {
    std::vector<Site> sites;
    FortuneEventQueue events;
    BeachLineTree beachLine;
    RawDiagram raw;
    std::vector<std::unique_ptr<CircleEvent>> circleEvents;
    std::size_t siteEventCount = 0;
    std::size_t circleEventCount = 0;
};

std::vector<Site> MakeSites(const std::vector<Point2>& points)
{
    std::vector<Site> sites;
    sites.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        sites.push_back({points[i], i});
    }
    return sites;
}

FortuneEventQueue InitializeSiteEventQueue(std::vector<Site>& sites)
{
    FortuneEventQueue events;
    for (Site& site : sites) {
        FortuneEvent event;
        event.kind = FortuneEventKind::Site;
        event.point = site.position;
        event.priorityY = site.position.y;
        event.site = &site;
        events.push(event);
    }
    return events;
}

void InvalidateCircleEvent(BeachNode* arc)
{
    if (arc == nullptr || arc->circleEvent == nullptr) return;
    arc->circleEvent->valid = false;
    arc->circleEvent = nullptr;
}

CircleEvent* QueueCircleEvent(FortuneState& state, Point2 bottom, BeachNode* disappearingArc)
{
    auto circleEvent = std::make_unique<CircleEvent>();
    circleEvent->bottom = bottom;
    circleEvent->disappearingArc = disappearingArc;
    CircleEvent* raw = circleEvent.get();
    state.circleEvents.push_back(std::move(circleEvent));

    if (disappearingArc != nullptr) {
        disappearingArc->circleEvent = raw;
    }

    FortuneEvent event;
    event.kind = FortuneEventKind::Circle;
    event.point = bottom;
    event.priorityY = bottom.y;
    event.circleEvent = raw;
    state.events.push(event);
    return raw;
}

bool EventIsValid(const FortuneEvent& event)
{
    if (event.kind == FortuneEventKind::Site) return event.site != nullptr;
    return event.circleEvent != nullptr && event.circleEvent->valid;
}

void CheckCircleEvent(FortuneState& fortune, BeachNode* arc, double sweepY)
{
    if (arc == nullptr || !arc->isLeaf) return;
    InvalidateCircleEvent(arc);

    BeachNode* previous = fortune.beachLine.PrevArc(arc);
    BeachNode* next = fortune.beachLine.NextArc(arc);
    if (previous == nullptr || next == nullptr) return;

    CircleGeometry circle;
    if (!ComputeCircle(previous->site, arc->site, next->site, circle)) return;
    if (circle.bottom.y >= sweepY - Epsilon) return;
    QueueCircleEvent(fortune, circle.bottom, arc);
}

ParametricCurveState SampleBeachLineForDisplay(
    const std::vector<Site*>& arcSites,
    double directrixY,
    double minX,
    double maxX)
{
    ParametricCurveState curve;
    curve.visible = !arcSites.empty();
    curve.color = SceneColor{126, 87, 194, 230};
    curve.thickness = 3.0;

    const int sampleCount = 140;
    curve.samples.reserve(sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(sampleCount - 1);
        const double x = minX + (maxX - minX) * t;

        double bestY = std::numeric_limits<double>::infinity();
        for (const Site* site : arcSites) {
            if (site == nullptr) continue;
            const Point2 position = site->position;
            const double denominator = 2.0 * (position.y - directrixY);
            if (std::abs(denominator) < Epsilon) continue;
            const double y =
                ((x - position.x) * (x - position.x) +
                    position.y * position.y - directrixY * directrixY) /
                denominator;
            bestY = std::min(bestY, y);
        }

        if (std::isfinite(bestY)) curve.samples.push_back({x, bestY});
    }
    return curve;
}

BoundingBox MakeBoundingBox(const std::vector<Point2>& points)
{
    if (points.empty()) return {};

    double minX = points.front().x;
    double maxX = points.front().x;
    double minY = points.front().y;
    double maxY = points.front().y;
    for (Point2 point : points) {
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    }

    const double width = std::max(1.0, maxX - minX);
    const double height = std::max(1.0, maxY - minY);
    const double padding = std::max({80.0, width * 0.35, height * 0.35});
    return {minX - padding, maxX + padding, minY - padding, maxY + padding};
}

bool IsInside(Point2 point, const BoundingBox& box)
{
    return point.x >= box.minX - Epsilon && point.x <= box.maxX + Epsilon &&
        point.y >= box.minY - Epsilon && point.y <= box.maxY + Epsilon;
}

Point2 PointAt(Point2 origin, Point2 direction, double t)
{
    return {origin.x + direction.x * t, origin.y + direction.y * t};
}

bool ClipParametricLine(
    Point2 origin,
    Point2 direction,
    const BoundingBox& box,
    double& minT,
    double& maxT)
{
    const auto clipBoundary = [&](double p, double q) {
        if (std::abs(p) < Epsilon) return q >= -Epsilon;
        const double r = q / p;
        if (p < 0.0) {
            if (r > maxT) return false;
            if (r > minT) minT = r;
        }
        else {
            if (r < minT) return false;
            if (r < maxT) maxT = r;
        }
        return true;
    };

    return clipBoundary(-direction.x, origin.x - box.minX) &&
        clipBoundary(direction.x, box.maxX - origin.x) &&
        clipBoundary(-direction.y, origin.y - box.minY) &&
        clipBoundary(direction.y, box.maxY - origin.y);
}

bool ClipRawEdge(const RawDiagram& raw, const RawEdge& edge, const BoundingBox& box, RawSegment& segment)
{
    if (edge.leftSite == nullptr || edge.rightSite == nullptr) return false;
    const Point2 left = edge.leftSite->position;
    const Point2 right = edge.rightSite->position;
    const Point2 midpoint{(left.x + right.x) * 0.5, (left.y + right.y) * 0.5};
    Point2 direction{-(right.y - left.y), right.x - left.x};
    if (std::abs(direction.x) < Epsilon && std::abs(direction.y) < Epsilon) return false;

    if (edge.firstVertex != NoIndex && edge.secondVertex != NoIndex) {
        const Point2 first = raw.vertices[edge.firstVertex];
        const Point2 second = raw.vertices[edge.secondVertex];
        Point2 segmentDirection{second.x - first.x, second.y - first.y};
        double minT = 0.0;
        double maxT = 1.0;
        if (!ClipParametricLine(first, segmentDirection, box, minT, maxT)) return false;
        segment = {PointAt(first, segmentDirection, minT), PointAt(first, segmentDirection, maxT),
            edge.leftSite, edge.rightSite};
        return !SamePoint(segment.first, segment.second);
    }

    if (edge.firstVertex != NoIndex || edge.secondVertex != NoIndex) {
        const std::size_t vertexIndex =
            edge.firstVertex != NoIndex ? edge.firstVertex : edge.secondVertex;
        const Point2 vertex = raw.vertices[vertexIndex];

        if (edge.hasRayDirection) {
            double minT = 0.0;
            double maxT = 1.0e9;
            if (!ClipParametricLine(vertex, edge.rayDirection, box, minT, maxT)) return false;
            segment = {
                PointAt(vertex, edge.rayDirection, minT),
                PointAt(vertex, edge.rayDirection, maxT),
                edge.leftSite,
                edge.rightSite
            };
            return !SamePoint(segment.first, segment.second);
        }

        double minT = -1.0e9;
        double maxT = 1.0e9;
        if (!ClipParametricLine(midpoint, direction, box, minT, maxT)) return false;
        const Point2 firstClip = PointAt(midpoint, direction, minT);
        const Point2 secondClip = PointAt(midpoint, direction, maxT);
        segment = {firstClip, secondClip, edge.leftSite, edge.rightSite};
        return !SamePoint(segment.first, segment.second);
    }

    double minT = -1.0e9;
    double maxT = 1.0e9;
    if (!ClipParametricLine(midpoint, direction, box, minT, maxT)) return false;
    segment = {PointAt(midpoint, direction, minT), PointAt(midpoint, direction, maxT),
        edge.leftSite, edge.rightSite};
    return !SamePoint(segment.first, segment.second);
}

std::vector<RawSegment> FinishAndClipRawEdges(const RawDiagram& raw, const BoundingBox& box)
{
    std::vector<RawSegment> segments;
    for (const std::unique_ptr<RawEdge>& edge : raw.edges) {
        RawSegment segment;
        if (edge != nullptr && ClipRawEdge(raw, *edge, box, segment)) {
            segments.push_back(segment);
        }
    }
    return segments;
}

std::string SiteEventCaption(const FortuneEvent& event)
{
    const Site* site = event.site;
    if (site == nullptr) return "Invalid site event";
    return "Site event " + std::to_string(site->inputIndex) + " at " +
        FormatPoint(site->position);
}

void AddSweepEvent(GeometryScene& scene, double sweepY, const std::string& caption)
{
    TimelineEvent event;
    event.kind = TimelineEventKind::SweepLine;
    event.caption = caption;
    event.sweepLine.visible = true;
    event.sweepLine.y = sweepY;
    event.sweepLine.color = SceneColor{96, 125, 139, 210};
    event.sweepLine.thickness = 2.5;
    scene.timeline.push_back(std::move(event));
}

void AddPointRestyleEvent(
    GeometryScene& scene,
    std::size_t pointIndex,
    Point2 point,
    ScenePointStyle style,
    const std::string& caption)
{
    TimelineEvent event;
    event.kind = TimelineEventKind::Point;
    event.pointAction = PointAction::Restyle;
    event.pointIndex = pointIndex;
    event.point = point;
    event.pointStyle = style;
    event.caption = caption;
    scene.timeline.push_back(std::move(event));
}

void AddGeneratedPointEvent(
    GeometryScene& scene,
    Point2 point,
    ScenePointStyle style,
    const std::string& caption)
{
    const std::size_t pointIndex = scene.points.size();
    scene.points.push_back(point);
    scene.pointStyles.push_back(style);

    TimelineEvent event;
    event.kind = TimelineEventKind::Point;
    event.pointAction = PointAction::Show;
    event.pointIndex = pointIndex;
    event.point = point;
    event.pointStyle = style;
    event.caption = caption;
    scene.timeline.push_back(std::move(event));
}

void AddCurveEvent(
    GeometryScene& scene,
    const BeachLineTree& beachLine,
    double sweepY,
    const BoundingBox& box)
{
    TimelineEvent event;
    event.kind = TimelineEventKind::ParametricCurve;
    event.caption = "Sample current symbolic beach line";
    event.parametricCurve = SampleBeachLineForDisplay(
        beachLine.ArcSitesLeftToRight(),
        sweepY - std::max(1.0, (box.maxY - box.minY) * 0.002),
        box.minX,
        box.maxX);
    scene.timeline.push_back(std::move(event));
}

void AddHideSweepOverlaysEvent(GeometryScene& scene)
{
    TimelineEvent hideSweep;
    hideSweep.kind = TimelineEventKind::SweepLine;
    hideSweep.caption = "Finish sweep";
    hideSweep.sweepLine.visible = false;
    scene.timeline.push_back(std::move(hideSweep));

    TimelineEvent hideCurve;
    hideCurve.kind = TimelineEventKind::ParametricCurve;
    hideCurve.caption = "Hide sampled beach line";
    hideCurve.parametricCurve.visible = false;
    scene.timeline.push_back(std::move(hideCurve));
}

void AddFinalSegmentEvents(GeometryScene& scene, const std::vector<RawSegment>& segments)
{
    for (const RawSegment& segment : segments) {
        const std::size_t firstIndex = scene.points.size();
        scene.points.push_back(segment.first);
        scene.pointStyles.push_back(ScenePointStyle{3.5, SceneColor{218, 67, 78, 255}});

        const std::size_t secondIndex = scene.points.size();
        scene.points.push_back(segment.second);
        scene.pointStyles.push_back(ScenePointStyle{3.5, SceneColor{218, 67, 78, 255}});

        TimelineEvent firstPoint;
        firstPoint.kind = TimelineEventKind::Point;
        firstPoint.pointAction = PointAction::Show;
        firstPoint.pointIndex = firstIndex;
        firstPoint.point = segment.first;
        firstPoint.pointStyle = ScenePointStyle{3.5, SceneColor{218, 67, 78, 255}};
        firstPoint.caption = "Reveal clipped Voronoi edge endpoint";
        scene.timeline.push_back(std::move(firstPoint));

        TimelineEvent secondPoint = firstPoint;
        secondPoint.pointIndex = secondIndex;
        secondPoint.point = segment.second;
        scene.timeline.push_back(std::move(secondPoint));

        scene.timeline.push_back({
            EdgeAction::Add,
            {{firstIndex, secondIndex}, EdgeLayer::Result},
            "Draw clipped Voronoi edge"
        });
    }
}

void HandleSiteEvent(
    FortuneState& fortune,
    GeometryScene& scene,
    const FortuneEvent& event,
    double sweepY,
    const BoundingBox& box)
{
    Site* site = event.site;
    if (site == nullptr) return;

    ++fortune.siteEventCount;
    AddSweepEvent(scene, sweepY, SiteEventCaption(event));
    AddPointRestyleEvent(scene, site->inputIndex, site->position,
        ScenePointStyle{10.0, SceneColor{232, 139, 45, 255}},
        "Process site event");

    if (fortune.beachLine.Empty()) {
        fortune.beachLine.Initialize(site);
        AddCurveEvent(scene, fortune.beachLine, sweepY, box);
        return;
    }

    BeachNode* arcAbove = fortune.beachLine.FindArcAbove(site->position, sweepY);
    if (arcAbove == nullptr || arcAbove->site == nullptr) return;

    InvalidateCircleEvent(arcAbove);
    Site* oldSite = arcAbove->site;
    ArcSplit split = fortune.beachLine.ReplaceArcByThree(arcAbove, site);

    RawEdge* edge = fortune.raw.CreateEdge(oldSite, site);
    fortune.beachLine.SetBreakpointEdge(split.leftArc, split.middleArc, edge);
    fortune.beachLine.SetBreakpointEdge(split.middleArc, split.rightArc, edge);

    CheckCircleEvent(fortune, split.leftArc, sweepY);
    CheckCircleEvent(fortune, split.rightArc, sweepY);
    AddCurveEvent(scene, fortune.beachLine, sweepY, box);
}

void HandleCircleEvent(
    FortuneState& fortune,
    GeometryScene& scene,
    const FortuneEvent& event,
    double sweepY,
    const BoundingBox& box)
{
    CircleEvent* circleEvent = event.circleEvent;
    if (circleEvent == nullptr || !circleEvent->valid) return;

    BeachNode* middleArc = circleEvent->disappearingArc;
    BeachNode* leftArc = fortune.beachLine.PrevArc(middleArc);
    BeachNode* rightArc = fortune.beachLine.NextArc(middleArc);
    if (middleArc == nullptr || leftArc == nullptr || rightArc == nullptr) {
        circleEvent->valid = false;
        return;
    }

    CircleGeometry circle;
    if (!ComputeCircle(leftArc->site, middleArc->site, rightArc->site, circle)) {
        circleEvent->valid = false;
        return;
    }

    ++fortune.circleEventCount;
    AddSweepEvent(scene, sweepY, "Circle event at " + FormatPoint(circleEvent->bottom));

    InvalidateCircleEvent(leftArc);
    InvalidateCircleEvent(rightArc);

    const std::size_t vertexIndex = fortune.raw.CreateVertex(circle.center);
    AddGeneratedPointEvent(scene, circle.center,
        ScenePointStyle{8.0, SceneColor{218, 67, 78, 255}},
        "Create Voronoi vertex from circle event");

    RawEdge* edgeAB = fortune.beachLine.GetBreakpointEdge(leftArc, middleArc);
    RawEdge* edgeBC = fortune.beachLine.GetBreakpointEdge(middleArc, rightArc);
    fortune.raw.AttachVertex(edgeAB, vertexIndex, rightArc->site);
    fortune.raw.AttachVertex(edgeBC, vertexIndex, leftArc->site);

    fortune.beachLine.RemoveArc(middleArc);
    circleEvent->valid = false;

    RawEdge* edgeAC = fortune.raw.CreateEdge(leftArc->site, rightArc->site);
    fortune.raw.AttachVertex(edgeAC, vertexIndex, middleArc->site);
    fortune.beachLine.SetBreakpointEdge(leftArc, rightArc, edgeAC);

    CheckCircleEvent(fortune, leftArc, sweepY);
    CheckCircleEvent(fortune, rightArc, sweepY);
    AddCurveEvent(scene, fortune.beachLine, sweepY, box);
}

} // namespace

fortune_voronoi::Result fortune_voronoi::Run(const std::vector<Point2>& inputSites)
{
    Result result;
    GeometryScene& scene = result.visualization.scene;
    scene.points = inputSites;
    scene.initialVisiblePointCount = inputSites.size();
    scene.pointStyles.assign(
        inputSites.size(),
        ScenePointStyle{7.0, SceneColor{35, 88, 135, 255}});

    if (inputSites.empty()) {
        result.visualization.status = "Fortune Voronoi: add at least one site.";
        result.visualization.succeeded = true;
        return result;
    }

    const BoundingBox box = MakeBoundingBox(inputSites);
    FortuneState fortune;
    fortune.sites = MakeSites(inputSites);
    fortune.events = InitializeSiteEventQueue(fortune.sites);

    while (!fortune.events.empty()) {
        const FortuneEvent event = fortune.events.top();
        fortune.events.pop();
        if (!EventIsValid(event)) continue;

        const double sweepY = event.priorityY;
        if (event.kind == FortuneEventKind::Site) {
            HandleSiteEvent(fortune, scene, event, sweepY, box);
        }
        else {
            HandleCircleEvent(fortune, scene, event, sweepY, box);
        }
    }

    const std::vector<RawSegment> segments = FinishAndClipRawEdges(fortune.raw, box);
    AddHideSweepOverlaysEvent(scene);
    AddFinalSegmentEvents(scene, segments);

    result.voronoiVertices = fortune.raw.vertices;
    result.rawEdgeCount = fortune.raw.edges.size();
    result.siteEventCount = fortune.siteEventCount;
    result.circleEventCount = fortune.circleEventCount;
    result.clippedSegments.reserve(segments.size());
    for (const RawSegment& segment : segments) {
        result.clippedSegments.push_back({
            segment.first,
            segment.second,
            segment.leftSite == nullptr ? NoIndex : segment.leftSite->inputIndex,
            segment.rightSite == nullptr ? NoIndex : segment.rightSite->inputIndex
        });
    }

    result.visualization.status =
        "Fortune Voronoi: " + std::to_string(result.siteEventCount) +
        " site events, " + std::to_string(result.circleEventCount) +
        " circle events, " + std::to_string(result.clippedSegments.size()) +
        " clipped edges.";
    result.visualization.succeeded = true;
    return result;
}

#ifndef FORTUNE_VORONOI_NO_MAIN
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

        const fortune_voronoi::Result result = fortune_voronoi::Run(sites);
        std::cout << result.visualization.status << '\n';
        std::cout << "Loaded sites: " << sites.size() << '\n';
        std::cout << "Raw Voronoi vertices: " << result.voronoiVertices.size() << '\n';
        for (std::size_t i = 0; i < result.voronoiVertices.size(); ++i) {
            std::cout << "  V" << i << ' ' << FormatPoint(result.voronoiVertices[i]) << '\n';
        }

        std::cout << "Raw edges created at site/circle events: "
            << result.rawEdgeCount << '\n';
        std::cout << "Clipped drawable segments: "
            << result.clippedSegments.size() << '\n';
        for (std::size_t i = 0; i < result.clippedSegments.size(); ++i) {
            const fortune_voronoi::VoronoiSegment& segment = result.clippedSegments[i];
            std::cout << "  E" << i << " sites("
                << segment.leftSite << ", " << segment.rightSite << ") "
                << FormatPoint(segment.first) << " -> "
                << FormatPoint(segment.second) << '\n';
        }
    }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
#endif
