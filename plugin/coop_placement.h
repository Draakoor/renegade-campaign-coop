// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "PhysicsSceneClass.h"
#include "physcoltest.h"
namespace CoopPlacement {
static bool Ground(Vector3& p) {
    CastResultStruct hit; hit.ComputeContactPoint=true;
    PhysRayCollisionTestClass ray(LineSegClass(p+Vector3(0,0,2),p-Vector3(0,0,4)),&hit,DEFAULT_COLLISION_GROUP,COLLISION_TYPE_PHYSICAL);
    ray.CheckDynamicObjs=false; PhysicsSceneClass::Get_Instance()->Cast_Ray(ray);
    if (hit.StartBad || hit.Fraction>=1 || hit.Normal.Z<.85f) return false;
    p=hit.ContactPoint; return true;
}
static bool Station(const Vector3& origin,float facing,Vector3& point,float& angle) {
    for (float radius : {3.5f,5.0f,6.5f}) for (float turn : {0.0f,90.0f,-90.0f,180.0f}) {
        const float direction=(facing+turn)*.01745329252f;
        Vector3 p=origin+Vector3(cosf(direction)*radius,sinf(direction)*radius,0);
        if (!Ground(p) || fabsf(p.Z-origin.Z)>2) continue;
        CastResultStruct sight;
        PhysRayCollisionTestClass ray(LineSegClass(origin+Vector3(0,0,1),p+Vector3(0,0,1)),&sight,DEFAULT_COLLISION_GROUP,COLLISION_TYPE_PHYSICAL);
        ray.CheckDynamicObjs=false; PhysicsSceneClass::Get_Instance()->Cast_Ray(ray);
        if (sight.StartBad || sight.Fraction<1) continue;
        Matrix3 rotation(true); rotation.Rotate_Z(direction+1.5707963f);
        CastResultStruct space;
        OBBoxClass bounds(p+Vector3(0,0,1.45f),Vector3(1.95f,.45f,1.35f),rotation);
        PhysOBBoxCollisionTestClass test(bounds,Vector3(0,0,.01f),&space,DEFAULT_COLLISION_GROUP);
        test.CheckDynamicObjs=false; PhysicsSceneClass::Get_Instance()->Cast_OBBox(test);
        if (!space.StartBad && space.Fraction==1) { point=p; angle=facing+turn+90; return true; }
    }
    return false;
}
}
