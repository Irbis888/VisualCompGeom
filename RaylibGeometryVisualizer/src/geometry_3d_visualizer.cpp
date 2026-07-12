#include "raylib.h"
#include "geometry_3d_visualizer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace {
struct Triangle { int a, b, c; };
struct Edge { int a, b; };
struct Solid {
    std::vector<Vector3> vertices;
    std::vector<Triangle> faces;
    std::vector<Edge> edges;
    Color color;
};
struct Selection { int solid = -1, vertex = -1; };
struct Drag { int axis = -1; float coordinate = 0; Vector3 start{}; };

float Dot(Vector3 a, Vector3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vector3 Add(Vector3 a, Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 Sub(Vector3 a, Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vector3 Scale(Vector3 v, float s) { return {v.x*s,v.y*s,v.z*s}; }
float Length(Vector3 v) { return std::sqrt(Dot(v,v)); }
Vector3 Normalized(Vector3 v) { const float n=Length(v); return n>0.00001F?Scale(v,1/n):Vector3{}; }
Vector3 Axis(int i) { return i==0?Vector3{1,0,0}:(i==1?Vector3{0,1,0}:Vector3{0,0,1}); }
Color AxisColor(int i, bool hot) {
    static constexpr std::array<Color,3> c{Color{221,63,67,255},Color{67,181,91,255},Color{62,115,224,255}};
    return hot?Color{255,199,65,255}:c[static_cast<std::size_t>(i)];
}
Solid Cube() {
    return {{{-3,0,-1},{-1,0,-1},{-1,2,-1},{-3,2,-1},{-3,0,1},{-1,0,1},{-1,2,1},{-3,2,1}},
        {{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,4,7},{0,7,3},{1,2,6},{1,6,5},{0,1,5},{0,5,4},{3,7,6},{3,6,2}},
        {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}},Color{79,142,213,210}};
}
Solid Pyramid() {
    return {{{1,0,-1.2F},{3.4F,0,-1.2F},{3.4F,0,1.2F},{1,0,1.2F},{2.2F,2.6F,0}},
        {{0,1,2},{0,2,3},{0,4,1},{1,4,2},{2,4,3},{3,4,0}},
        {{0,1},{1,2},{2,3},{3,0},{0,4},{1,4},{2,4},{3,4}},Color{220,137,75,210}};
}
void DrawSolid(const Solid& s) {
    for (const Triangle f:s.faces) DrawTriangle3D(s.vertices[f.a],s.vertices[f.b],s.vertices[f.c],s.color);
    for (const Edge e:s.edges) DrawLine3D(s.vertices[e.a],s.vertices[e.b],Color{38,48,60,255});
}
void DrawGrid3D() {
    for (int i=-20;i<=20;++i) {
        const float p=static_cast<float>(i);
        const Color c=(i==0||i%5==0)?Color{91,97,104,150}:Color{115,121,128,105};
        DrawLine3D({-20,0,p},{20,0,p},c); DrawLine3D({p,0,-20},{p,0,20},c);
    }
}
bool Valid(Selection s,const std::vector<Solid>& solids) {
    return s.solid>=0&&s.vertex>=0&&static_cast<std::size_t>(s.solid)<solids.size()&&
        static_cast<std::size_t>(s.vertex)<solids[s.solid].vertices.size();
}
Vector3& Position(Selection s,std::vector<Solid>& solids) { return solids[s.solid].vertices[s.vertex]; }
Selection Pick(Ray ray,const std::vector<Solid>& solids) {
    Selection result; float nearest=std::numeric_limits<float>::max();
    for (std::size_t s=0;s<solids.size();++s) for (std::size_t v=0;v<solids[s].vertices.size();++v) {
        const RayCollision hit=GetRayCollisionSphere(ray,solids[s].vertices[v],0.18F);
        if (hit.hit&&hit.distance<nearest) { nearest=hit.distance; result={static_cast<int>(s),static_cast<int>(v)}; }
    }
    return result;
}
float SegmentDistance(Vector2 p,Vector2 a,Vector2 b) {
    const Vector2 d{b.x-a.x,b.y-a.y}; const float n=d.x*d.x+d.y*d.y;
    if(n<0.001F)return 100000; const float t=std::clamp(((p.x-a.x)*d.x+(p.y-a.y)*d.y)/n,0.0F,1.0F);
    const float x=p.x-a.x-d.x*t,y=p.y-a.y-d.y*t; return std::sqrt(x*x+y*y);
}
int HotAxis(Vector2 mouse,Vector3 p,const Camera3D& camera,float size) {
    const Vector2 origin=GetWorldToScreen(p,camera); int result=-1; float nearest=10;
    for(int i=0;i<3;++i) { const float d=SegmentDistance(mouse,origin,GetWorldToScreen(Add(p,Scale(Axis(i),size)),camera));
        if(d<nearest){nearest=d;result=i;} } return result;
}
float AxisCoordinate(Ray ray,Vector3 origin,Vector3 axis) {
    const Vector3 offset=Sub(ray.position,origin); const float b=Dot(ray.direction,axis),den=1-b*b;
    return std::fabs(den)<0.0001F?Dot(offset,axis):(Dot(offset,axis)-b*Dot(ray.direction,offset))/den;
}
void DrawGizmo(Vector3 p,int hot,int dragged,float size) {
    for(int i=0;i<3;++i) { const Vector3 axis=Axis(i),end=Add(p,Scale(axis,size)); const bool active=i==hot||i==dragged;
        DrawCylinderEx(p,end,active?0.045F:0.03F,active?0.045F:0.03F,8,AxisColor(i,active));
        DrawCylinderEx(end,Add(end,Scale(axis,0.22F)),0.10F,0,8,AxisColor(i,active)); }
}
void UpdateCamera(Camera3D& camera,float& yaw,float& pitch,bool looking) {
    if(looking){const Vector2 d=GetMouseDelta();yaw-=d.x*0.0025F;pitch=std::clamp(pitch-d.y*0.0025F,-1.48F,1.48F);}
    const Vector3 forward{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
    const Vector3 flat=Normalized({forward.x,0,forward.z}),right{-flat.z,0,flat.x};
    const float speed=(IsKeyDown(KEY_LEFT_SHIFT)?9.0F:4.0F)*GetFrameTime();
    if(IsKeyDown(KEY_W))camera.position=Add(camera.position,Scale(flat,speed));
    if(IsKeyDown(KEY_S))camera.position=Add(camera.position,Scale(flat,-speed));
    if(IsKeyDown(KEY_D))camera.position=Add(camera.position,Scale(right,speed));
    if(IsKeyDown(KEY_A))camera.position=Add(camera.position,Scale(right,-speed));
    if(IsKeyDown(KEY_E))camera.position.y+=speed; if(IsKeyDown(KEY_Q))camera.position.y-=speed;
    camera.target=Add(camera.position,forward); camera.fovy=std::clamp(camera.fovy-GetMouseWheelMove()*3,20.0F,85.0F);
}
void Overlay(Selection selected,const std::vector<Solid>& solids) {
    DrawRectangle(14,14,650,Valid(selected,solids)?106:84,Fade(Color{24,28,34,255},0.88F));
    DrawText("3D Computational Geometry Workspace",28,26,22,RAYWHITE);
    DrawText("WASD fly | Q/E down/up | Shift faster | hold RMB + mouse look | wheel zoom",28,56,15,Color{201,207,214,255});
    DrawText("LMB vertex/gizmo: edit | edges locked | Tab or 1..5: switch tab",28,78,15,Color{201,207,214,255});
    if(Valid(selected,solids)){const Vector3 p=solids[selected.solid].vertices[selected.vertex];
        DrawText(TextFormat("Selected vertex %d:%d  (%.2f, %.2f, %.2f)",selected.solid,selected.vertex,p.x,p.y,p.z),28,100,15,Color{255,199,65,255});}
    DrawFPS(GetScreenWidth()-95,16);
}

Camera3D workspaceCamera{{8,6,10},{0,1,0},{0,1,0},50,CAMERA_PERSPECTIVE};
const Vector3 initialForward=Normalized(Sub(workspaceCamera.target,workspaceCamera.position));
float workspaceYaw=std::atan2(initialForward.x,initialForward.z);
float workspacePitch=std::asin(initialForward.y);
std::vector<Solid> workspaceSolids{Cube(),Pyramid()};
Selection workspaceSelection;
Drag workspaceDrag;
bool workspaceLooking=false;
}

void Draw3DVisualizerFrame() {
    Camera3D& camera=workspaceCamera;
    float& yaw=workspaceYaw;
    float& pitch=workspacePitch;
    std::vector<Solid>& solids=workspaceSolids;
    Selection& selected=workspaceSelection;
    Drag& drag=workspaceDrag;
    bool& looking=workspaceLooking;

        if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)){looking=true;DisableCursor();}
        if(IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)){looking=false;EnableCursor();}
        UpdateCamera(camera,yaw,pitch,looking);
        const Vector2 mouse=GetMousePosition(); const Ray ray=GetMouseRay(mouse,camera);
        const bool valid=Valid(selected,solids); const Vector3 p=valid?Position(selected,solids):Vector3{};
        const float size=valid?std::clamp(Length(Sub(camera.position,p))*0.12F,0.65F,2.0F):1;
        const int hot=valid&&!looking?HotAxis(mouse,p,camera,size):-1;
        if(!looking&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(valid&&hot>=0){drag={hot,AxisCoordinate(ray,p,Axis(hot)),p};}
            else{selected=Pick(ray,solids);drag.axis=-1;}
        }
        if(drag.axis>=0&&IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&Valid(selected,solids)) {
            const Vector3 axis=Axis(drag.axis); Position(selected,solids)=Add(drag.start,Scale(axis,AxisCoordinate(ray,drag.start,axis)-drag.coordinate));
        }
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))drag.axis=-1;
        BeginDrawing();ClearBackground(Color{32,36,42,255});BeginMode3D(camera);DrawGrid3D();
        for(const Solid& s:solids)DrawSolid(s);
        for(std::size_t s=0;s<solids.size();++s)for(std::size_t v=0;v<solids[s].vertices.size();++v){
            const bool active=selected.solid==static_cast<int>(s)&&selected.vertex==static_cast<int>(v);
            DrawSphere(solids[s].vertices[v],active?0.14F:0.10F,active?Color{255,199,65,255}:Color{235,239,244,255});
            DrawSphereWires(solids[s].vertices[v],active?0.145F:0.105F,8,8,Color{35,40,47,255});}
        if(Valid(selected,solids))DrawGizmo(Position(selected,solids),hot,drag.axis,size);
        EndMode3D();Overlay(selected,solids);EndDrawing();
}

void Deactivate3DVisualizer() {
    if(workspaceLooking)EnableCursor();
    workspaceLooking=false;
    workspaceDrag.axis=-1;
}
