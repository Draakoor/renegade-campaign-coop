// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "PhysicsSceneClass.h"
#include "physcoltest.h"
namespace CoopPortals {
struct Portal { int zone=0; Vector3 position; };
static std::vector<Portal> portals;
static Vector3 previous;
static bool initialized=false;
static int checkpoint=0;
static void Loaded() { portals.clear(); initialized=false; checkpoint=0; }
static bool Ground(Vector3& p) {
    CastResultStruct hit;
    hit.ComputeContactPoint=true;
    PhysRayCollisionTestClass ray(LineSegClass(p+Vector3(0,0,2),p-Vector3(0,0,4)),&hit,DEFAULT_COLLISION_GROUP,COLLISION_TYPE_PHYSICAL);
    ray.CheckDynamicObjs=false;
    PhysicsSceneClass::Get_Instance()->Cast_Ray(ray);
    if (hit.StartBad || hit.Fraction>=1 || hit.Normal.Z<.85f) return false;
    p=hit.ContactPoint; return true;
}
static bool Site(const Vector3& origin,Vector3& ground) {
    // Fit the complete original 4m-wide, 5m-high laser frame, not just a player.
    for (float radius : {5.0f,7.0f,9.0f,11.0f,13.0f}) for (int i=0; i<16; ++i) {
        const float angle=i*6.2831853f/16;
        Vector3 p=origin+Vector3(cosf(angle)*radius,sinf(angle)*radius,0);
        if (!Ground(p) || fabsf(p.Z-origin.Z)>2) continue;
        bool walkable=true; float last=origin.Z;
        for (int step=1; step<=8; ++step) {
            Vector3 sample=origin+(p-origin)*(step/8.0f);
            if (!Ground(sample) || fabsf(sample.Z-last)>.8f) { walkable=false; break; }
            last=sample.Z;
        }
        if (!walkable) continue;
        CastResultStruct pathHit;
        PhysRayCollisionTestClass path(LineSegClass(origin+Vector3(0,0,1),p+Vector3(0,0,1)),&pathHit,DEFAULT_COLLISION_GROUP,COLLISION_TYPE_PHYSICAL);
        path.CheckDynamicObjs=false; PhysicsSceneClass::Get_Instance()->Cast_Ray(path);
        if (pathHit.StartBad || pathHit.Fraction<1) continue;
        CastResultStruct space;
        OBBoxClass bounds(p+Vector3(0,0,2.8f),Vector3(2.2f,2.2f,2.55f),Matrix3(true));
        PhysOBBoxCollisionTestClass box(bounds,Vector3(0,0,.01f),&space,DEFAULT_COLLISION_GROUP);
        box.CheckDynamicObjs=false; PhysicsSceneClass::Get_Instance()->Cast_OBBox(box);
        if (!space.StartBad && space.Fraction==1) { ground=p; return true; }
    }
    return false;
}
static void Target(int id,const Vector3& destination) {
    if (auto* zone=Commands->Find_Object(id)) {
        Remove_Script(zone,"TDA_Teleport_Zone");
        char params[128]; sprintf(params,"%.4f %.4f %.4f,0",destination.X,destination.Y,destination.Z);
        Attach_Script_Once(zone,"TDA_Teleport_Zone",params);
    }
}
static bool Add(const Vector3& origin,const Vector3& destination) {
    Vector3 point;
    if (!Site(origin,point)) {
        Console_Output("[Novara] Checkpoint portal has no clear site near %.2f,%.2f,%.2f; !cp remains available.\n",origin.X,origin.Y,origin.Z);
        return false;
    }
    std::vector<GameObject*> props;
    auto* flag=Commands->Create_Object("Marker Flag",point+Vector3(0,0,1.03f));
    if (flag) props.push_back(flag);
    // Match CoopBeta's actual assets. Compensate for the wall model's offset
    // origin (local Y=2.484), measured from the original M01 objects.
    for (float angle : {0.0f,90.0f,180.0f}) {
        const float rad=angle*.01745329252f;
        const Vector3 normal(-sinf(rad),cosf(rad),0);
        auto* wall=Commands->Create_Object("Simple_Sydney_SandM_Wall",point+normal*(2.03f-2.484f)+Vector3(0,0,.165f));
        if (wall) { Commands->Set_Facing(wall,angle); props.push_back(wall); }
    }
    OBBoxClass bounds(point+Vector3(0,0,1.7f),Vector3(1.65f,1.65f,1.65f),Matrix3(true));
    auto* zone=Create_Zone("Script_Zone_Star",bounds);
    if (!zone || props.size()!=4) {
        for (auto* prop : props) Commands->Destroy_Object(prop);
        if (zone) Commands->Destroy_Object(zone);
        return false;
    }
    for (auto* prop : props) {
        auto* physical=prop->As_PhysicalGameObj();
        physical->Set_Collision_Group(UNCOLLIDEABLE_GROUP);
        physical->Get_Defense_Object()->Set_Can_Object_Die(false);
    }
    const int id=Commands->Get_ID(zone); Target(id,destination);
    portals.push_back({id,point});
    Console_Output("[Novara] CoopBeta-style checkpoint portal %.2f,%.2f,%.2f -> %.2f,%.2f,%.2f\n",point.X,point.Y,point.Z,destination.X,destination.Y,destination.Z);
    return true;
}
static void Tick() {
    if (!Campaign::state.active() || !Campaign::ready || !Campaign::spawnValid) return;
    if (!initialized) { previous=Campaign::spawn; checkpoint=Campaign::state.checkpoint; initialized=true; }
    if (Campaign::state.checkpoint<=checkpoint) return;
    const auto target=Campaign::spawn;
    for (const auto& portal : portals) Target(portal.zone,target);
    Add(previous,target);
    previous=target; checkpoint=Campaign::state.checkpoint;
}
static void Probe() {
    if (!Campaign::state.active() || !Campaign::ready || !DASettingsManager::Get_Bool("CampaignDiagnostics",false) || The_Game()->Get_Current_Players()!=0) return;
    const bool added=Add(Campaign::spawn,Campaign::spawn);
    Console_Output("[Novara] DIAGNOSTIC portal result=%s; original assets and TDA script, no client crossing test.\n",added ? "PASS" : "FAIL");
}
}
