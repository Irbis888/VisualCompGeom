#include "raylib.h"
#include "geometry_3d_visualizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Triangle { int a, b, c; };
struct Edge { int a, b; };
struct Solid {
    std::vector<Vector3> vertices;
    std::vector<Triangle> faces;
    std::vector<Edge> edges;
    std::vector<Color> faceColors;
    std::vector<Color> edgeColors;
    std::vector<SceneVertexStyle3D> vertexStyles;
    std::vector<bool> visibleVertices;
    std::size_t editableVertexCount = 0;
};
struct Selection { int solid = -1, vertex = -1; };
struct Drag { int axis = -1; float coordinate = 0; Vector3 start{}; };

float Dot(Vector3 a, Vector3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vector3 Add(Vector3 a, Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 Sub(Vector3 a, Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vector3 Scale(Vector3 value, float scalar) { return {value.x*scalar,value.y*scalar,value.z*scalar}; }
float Length(Vector3 value) { return std::sqrt(Dot(value,value)); }
Vector3 Normalized(Vector3 value) { const float length=Length(value); return length>0.00001F?Scale(value,1/length):Vector3{}; }
Vector3 Axis(int axis) { return axis==0?Vector3{1,0,0}:(axis==1?Vector3{0,1,0}:Vector3{0,0,1}); }
Color AxisColor(int axis, bool hot) {
    static constexpr std::array<Color,3> colors{Color{221,63,67,255},Color{67,181,91,255},Color{62,115,224,255}};
    return hot?Color{255,199,65,255}:colors[static_cast<std::size_t>(axis)];
}
Vector3 ToVector3(const Point3& point) {
    return {static_cast<float>(point.x),static_cast<float>(point.y),static_cast<float>(point.z)};
}
Color ToColor(const SceneColor3D& color) { return {color.r,color.g,color.b,color.a}; }
Color EdgeColor(EdgeLayer3D layer) {
    if(layer==EdgeLayer3D::Intermediate)return Color{230,145,56,255};
    if(layer==EdgeLayer3D::Result)return Color{192,57,43,255};
    return Color{38,48,60,255};
}

bool SameEdge(const SceneEdge3D& left,const SceneEdge3D& right) {
    return left.layer==right.layer&&
        ((left.edge.first==right.edge.first&&left.edge.second==right.edge.second)||
         (left.edge.first==right.edge.second&&left.edge.second==right.edge.first));
}
bool SameTriangle(const SceneTriangle3D& left,const SceneTriangle3D& right) {
    std::array<std::size_t,3> lhs{left.triangle.first,left.triangle.second,left.triangle.third};
    std::array<std::size_t,3> rhs{right.triangle.first,right.triangle.second,right.triangle.third};
    std::sort(lhs.begin(),lhs.end());
    std::sort(rhs.begin(),rhs.end());
    return lhs==rhs;
}
void ApplyEdge(std::vector<SceneEdge3D>& edges,EdgeAction3D action,
               const SceneEdge3D& edge,const SceneEdge3D& replacement) {
    const auto found=std::find_if(edges.begin(),edges.end(),[&](const SceneEdge3D& value){return SameEdge(value,edge);});
    if(action==EdgeAction3D::Add){if(found==edges.end())edges.push_back(edge);}
    else if(action==EdgeAction3D::Remove){if(found!=edges.end())edges.erase(found);}
    else if(found!=edges.end())*found=replacement;
    else edges.push_back(replacement);
}
void ApplyTriangle(std::vector<SceneTriangle3D>& triangles,TriangleAction3D action,
                   const SceneTriangle3D& triangle,const SceneTriangle3D& replacement) {
    const auto found=std::find_if(triangles.begin(),triangles.end(),
        [&](const SceneTriangle3D& value){return SameTriangle(value,triangle);});
    if(action==TriangleAction3D::Add){if(found==triangles.end())triangles.push_back(triangle);}
    else if(action==TriangleAction3D::Remove){if(found!=triangles.end())triangles.erase(found);}
    else if(found!=triangles.end())*found=replacement;
    else triangles.push_back(replacement);
}
void ApplyVertex(GeometryScene3D& scene,std::vector<bool>& visible,VertexAction3D action,
                 std::size_t index,const Point3& point,const SceneVertexStyle3D& style) {
    if(index>=scene.vertices.size())return;
    if(action==VertexAction3D::Show)visible[index]=true;
    else if(action==VertexAction3D::Hide)visible[index]=false;
    else if(action==VertexAction3D::Move)scene.vertices[index]=point;
    else scene.vertexStyles[index]=style;
}

Solid BuildSolid(GeometryScene3D scene) {
    Solid solid;
    if(scene.vertexStyles.size()<scene.vertices.size())scene.vertexStyles.resize(scene.vertices.size());
    const std::size_t initiallyVisible=std::min(scene.initialVisibleVertexCount,scene.vertices.size());
    std::vector<bool> visible(scene.vertices.size(),false);
    std::fill(visible.begin(),visible.begin()+initiallyVisible,true);
    auto edges=scene.persistentEdges;
    auto triangles=scene.persistentTriangles;
    for(const TimelineEvent3D& event:scene.timeline){
        if(event.kind==TimelineEventKind3D::Edge)ApplyEdge(edges,event.edgeAction,event.edge,event.replacementEdge);
        else if(event.kind==TimelineEventKind3D::Vertex)ApplyVertex(scene,visible,event.vertexAction,event.vertexIndex,event.point,event.vertexStyle);
        else ApplyTriangle(triangles,event.triangleAction,event.triangle,event.replacementTriangle);
        for(const TimelineEdgeChange3D& change:event.extraEdgeChanges)ApplyEdge(edges,change.action,change.edge,change.replacementEdge);
        for(const TimelineVertexChange3D& change:event.extraVertexChanges)ApplyVertex(scene,visible,change.action,change.vertexIndex,change.point,change.style);
        for(const TimelineTriangleChange3D& change:event.extraTriangleChanges)ApplyTriangle(triangles,change.action,change.triangle,change.replacementTriangle);
    }
    for(const Point3& point:scene.vertices)solid.vertices.push_back(ToVector3(point));
    for(const SceneTriangle3D& triangle:triangles){
        const Triangle3& value=triangle.triangle;
        if(value.first>=scene.vertices.size()||value.second>=scene.vertices.size()||value.third>=scene.vertices.size())continue;
        solid.faces.push_back({static_cast<int>(value.first),static_cast<int>(value.second),static_cast<int>(value.third)});
        solid.faceColors.push_back(ToColor(triangle.color));
    }
    for(const SceneEdge3D& edge:edges){
        if(edge.edge.first>=scene.vertices.size()||edge.edge.second>=scene.vertices.size())continue;
        solid.edges.push_back({static_cast<int>(edge.edge.first),static_cast<int>(edge.edge.second)});
        solid.edgeColors.push_back(EdgeColor(edge.layer));
    }
    solid.vertexStyles=std::move(scene.vertexStyles);
    solid.visibleVertices=std::move(visible);
    solid.editableVertexCount=initiallyVisible;
    return solid;
}

void DrawSolid(const Solid& solid) {
    for(std::size_t index=0;index<solid.faces.size();++index){
        const Triangle face=solid.faces[index];
        if(face.a<0||face.b<0||face.c<0||static_cast<std::size_t>(face.a)>=solid.vertices.size()||
           static_cast<std::size_t>(face.b)>=solid.vertices.size()||static_cast<std::size_t>(face.c)>=solid.vertices.size())continue;
        if(!solid.visibleVertices.empty()&&
           (!solid.visibleVertices[face.a]||!solid.visibleVertices[face.b]||!solid.visibleVertices[face.c]))continue;
        DrawTriangle3D(solid.vertices[face.a],solid.vertices[face.b],solid.vertices[face.c],solid.faceColors[index]);
    }
    for(std::size_t index=0;index<solid.edges.size();++index){
        const Edge edge=solid.edges[index];
        if(edge.a<0||edge.b<0||static_cast<std::size_t>(edge.a)>=solid.vertices.size()||
           static_cast<std::size_t>(edge.b)>=solid.vertices.size())continue;
        if(!solid.visibleVertices.empty()&&(!solid.visibleVertices[edge.a]||!solid.visibleVertices[edge.b]))continue;
        DrawLine3D(solid.vertices[edge.a],solid.vertices[edge.b],solid.edgeColors[index]);
    }
}
void DrawGrid3D() {
    for(int i=-20;i<=20;++i){
        const float position=static_cast<float>(i);
        const Color color=(i==0||i%5==0)?Color{91,97,104,150}:Color{115,121,128,105};
        DrawLine3D({-20,0,position},{20,0,position},color);
        DrawLine3D({position,0,-20},{position,0,20},color);
    }
}
bool Valid(Selection selection,const std::vector<Solid>& solids) {
    if(selection.solid<0||selection.vertex<0||static_cast<std::size_t>(selection.solid)>=solids.size())return false;
    const Solid& solid=solids[selection.solid];
    const std::size_t vertex=static_cast<std::size_t>(selection.vertex);
    return vertex<solid.vertices.size()&&vertex<solid.editableVertexCount&&
        (solid.visibleVertices.empty()||solid.visibleVertices[vertex]);
}
Vector3& Position(Selection selection,std::vector<Solid>& solids) { return solids[selection.solid].vertices[selection.vertex]; }
Selection Pick(Ray ray,const std::vector<Solid>& solids) {
    Selection result;
    float nearest=std::numeric_limits<float>::max();
    for(std::size_t solidIndex=0;solidIndex<solids.size();++solidIndex){
        const Solid& solid=solids[solidIndex];
        for(std::size_t vertex=0;vertex<solid.vertices.size();++vertex){
            if(vertex>=solid.editableVertexCount||(!solid.visibleVertices.empty()&&!solid.visibleVertices[vertex]))continue;
            const float radius=vertex<solid.vertexStyles.size()
                ?std::max(0.18F,static_cast<float>(solid.vertexStyles[vertex].radius)):0.18F;
            const RayCollision hit=GetRayCollisionSphere(ray,solid.vertices[vertex],radius);
            if(hit.hit&&hit.distance<nearest){nearest=hit.distance;result={static_cast<int>(solidIndex),static_cast<int>(vertex)};}
        }
    }
    return result;
}
float SegmentDistance(Vector2 point,Vector2 start,Vector2 end) {
    const Vector2 delta{end.x-start.x,end.y-start.y};
    const float squared=delta.x*delta.x+delta.y*delta.y;
    if(squared<0.001F)return 100000;
    const float t=std::clamp(((point.x-start.x)*delta.x+(point.y-start.y)*delta.y)/squared,0.0F,1.0F);
    const float x=point.x-start.x-delta.x*t,y=point.y-start.y-delta.y*t;
    return std::sqrt(x*x+y*y);
}
int HotAxis(Vector2 mouse,Vector3 position,const Camera3D& camera,float size) {
    const Vector2 origin=GetWorldToScreen(position,camera);
    int result=-1;float nearest=10;
    for(int axis=0;axis<3;++axis){
        const float distance=SegmentDistance(mouse,origin,GetWorldToScreen(Add(position,Scale(Axis(axis),size)),camera));
        if(distance<nearest){nearest=distance;result=axis;}
    }
    return result;
}
float AxisCoordinate(Ray ray,Vector3 origin,Vector3 axis) {
    const Vector3 offset=Sub(ray.position,origin);
    const float directionDot=Dot(ray.direction,axis),denominator=1-directionDot*directionDot;
    return std::fabs(denominator)<0.0001F?Dot(offset,axis):
        (Dot(offset,axis)-directionDot*Dot(ray.direction,offset))/denominator;
}
void DrawGizmo(Vector3 position,int hot,int dragged,float size) {
    for(int index=0;index<3;++index){
        const Vector3 axis=Axis(index),end=Add(position,Scale(axis,size));
        const bool active=index==hot||index==dragged;
        DrawCylinderEx(position,end,active?0.045F:0.03F,active?0.045F:0.03F,8,AxisColor(index,active));
        DrawCylinderEx(end,Add(end,Scale(axis,0.22F)),0.10F,0,8,AxisColor(index,active));
    }
}
void UpdateCamera(Camera3D& camera,float& yaw,float& pitch,bool looking) {
    if(looking){const Vector2 delta=GetMouseDelta();yaw-=delta.x*0.0025F;pitch=std::clamp(pitch-delta.y*0.0025F,-1.48F,1.48F);}
    const Vector3 forward{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
    const Vector3 flat=Normalized({forward.x,0,forward.z}),right{-flat.z,0,flat.x};
    const float speed=(IsKeyDown(KEY_LEFT_SHIFT)?9.0F:4.0F)*GetFrameTime();
    if(IsKeyDown(KEY_W))camera.position=Add(camera.position,Scale(flat,speed));
    if(IsKeyDown(KEY_S))camera.position=Add(camera.position,Scale(flat,-speed));
    if(IsKeyDown(KEY_D))camera.position=Add(camera.position,Scale(right,speed));
    if(IsKeyDown(KEY_A))camera.position=Add(camera.position,Scale(right,-speed));
    if(IsKeyDown(KEY_E))camera.position.y+=speed;
    if(IsKeyDown(KEY_Q))camera.position.y-=speed;
    camera.target=Add(camera.position,forward);
    camera.fovy=std::clamp(camera.fovy-GetMouseWheelMove()*3,20.0F,85.0F);
}

Camera3D workspaceCamera{{8,6,10},{0,1,0},{0,1,0},50,CAMERA_PERSPECTIVE};
const Vector3 initialForward=Normalized(Sub(workspaceCamera.target,workspaceCamera.position));
float workspaceYaw=std::atan2(initialForward.x,initialForward.z);
float workspacePitch=std::asin(initialForward.y);
std::vector<Solid> workspaceSolids;
Selection workspaceSelection;
Drag workspaceDrag;
bool workspaceLooking=false;
std::string workspaceStatus;

void Overlay() {
    const bool selected=Valid(workspaceSelection,workspaceSolids);
    DrawRectangle(14,14,690,selected?128:106,Fade(Color{24,28,34,255},0.88F));
    DrawText("3D Computational Geometry Workspace",28,26,22,RAYWHITE);
    DrawText("WASD fly | Q/E down/up | Shift faster | hold RMB + mouse look | wheel zoom",28,56,15,Color{201,207,214,255});
    DrawText("LMB vertex/gizmo: edit | edges locked | Tab or 1..5: switch tab",28,78,15,Color{201,207,214,255});
    if(!workspaceStatus.empty())DrawText(workspaceStatus.c_str(),28,100,15,Color{148,203,255,255});
    if(selected){
        const Vector3 point=Position(workspaceSelection,workspaceSolids);
        DrawText(TextFormat("Selected vertex %d  (%.2f, %.2f, %.2f)",workspaceSelection.vertex,point.x,point.y,point.z),
                 28,122,15,Color{255,199,65,255});
    }
    DrawFPS(GetScreenWidth()-95,16);
}

} // namespace

void Set3DVisualization(AlgorithmVisualization3D visualization) {
    workspaceSelection={};
    workspaceDrag={};
    workspaceDrag.axis=-1;
    workspaceLooking=false;
    workspaceSolids.clear();
    if(!visualization.scene.vertices.empty())workspaceSolids.push_back(BuildSolid(std::move(visualization.scene)));
    workspaceStatus=std::move(visualization.status);
    workspaceCamera={{8,6,10},{0,1,0},{0,1,0},50,CAMERA_PERSPECTIVE};
    const Vector3 forward=Normalized(Sub(workspaceCamera.target,workspaceCamera.position));
    workspaceYaw=std::atan2(forward.x,forward.z);
    workspacePitch=std::asin(forward.y);
}

void Draw3DVisualizerFrame() {
    if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)){workspaceLooking=true;DisableCursor();}
    if(IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)){workspaceLooking=false;EnableCursor();}
    UpdateCamera(workspaceCamera,workspaceYaw,workspacePitch,workspaceLooking);
    const Vector2 mouse=GetMousePosition();
    const Ray ray=GetMouseRay(mouse,workspaceCamera);
    const bool valid=Valid(workspaceSelection,workspaceSolids);
    const Vector3 position=valid?Position(workspaceSelection,workspaceSolids):Vector3{};
    const float gizmoSize=valid?std::clamp(Length(Sub(workspaceCamera.position,position))*0.12F,0.65F,2.0F):1;
    const int hot=valid&&!workspaceLooking?HotAxis(mouse,position,workspaceCamera,gizmoSize):-1;
    if(!workspaceLooking&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
        if(valid&&hot>=0)workspaceDrag={hot,AxisCoordinate(ray,position,Axis(hot)),position};
        else{workspaceSelection=Pick(ray,workspaceSolids);workspaceDrag.axis=-1;}
    }
    if(workspaceDrag.axis>=0&&IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&Valid(workspaceSelection,workspaceSolids)){
        const Vector3 axis=Axis(workspaceDrag.axis);
        Position(workspaceSelection,workspaceSolids)=Add(workspaceDrag.start,
            Scale(axis,AxisCoordinate(ray,workspaceDrag.start,axis)-workspaceDrag.coordinate));
    }
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))workspaceDrag.axis=-1;

    BeginDrawing();
    ClearBackground(Color{32,36,42,255});
    BeginMode3D(workspaceCamera);
    DrawGrid3D();
    for(const Solid& solid:workspaceSolids)DrawSolid(solid);
    for(std::size_t solidIndex=0;solidIndex<workspaceSolids.size();++solidIndex){
        const Solid& solid=workspaceSolids[solidIndex];
        for(std::size_t vertex=0;vertex<solid.vertices.size();++vertex){
            if(!solid.visibleVertices.empty()&&!solid.visibleVertices[vertex])continue;
            const bool active=workspaceSelection.solid==static_cast<int>(solidIndex)&&
                workspaceSelection.vertex==static_cast<int>(vertex);
            const SceneVertexStyle3D style=vertex<solid.vertexStyles.size()?solid.vertexStyles[vertex]:SceneVertexStyle3D{};
            const float baseRadius=static_cast<float>(style.radius);
            const float radius=active?std::max(0.14F,baseRadius+0.04F):baseRadius;
            DrawSphere(solid.vertices[vertex],radius,active?Color{255,199,65,255}:ToColor(style.color));
            DrawSphereWires(solid.vertices[vertex],radius+0.005F,8,8,Color{35,40,47,255});
        }
    }
    if(Valid(workspaceSelection,workspaceSolids))
        DrawGizmo(Position(workspaceSelection,workspaceSolids),hot,workspaceDrag.axis,gizmoSize);
    EndMode3D();
    Overlay();
    EndDrawing();
}

void Deactivate3DVisualizer() {
    if(workspaceLooking)EnableCursor();
    workspaceLooking=false;
    workspaceDrag.axis=-1;
}
