// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "text_rules.h"
#include "VehicleGameObjDef.h"
#include "VehicleGameObj.h"
#include "PhysDefClass.h"
#include "admin_weapons.h"

namespace AdminTools {
struct Vehicle { unsigned id; std::string preset; int seats; bool flying; };
static std::vector<Vehicle> vehicles;
static std::set<int> spawned;
static int probeObject=0,probeIndex=0;
static bool probeActive=false,probePass=true;
static DWORD probeTime=0;
static bool Drivable(const VehicleGameObjDef* def) {
    if (def->Get_Seat_Count()<1 || def->Get_Type()==VEHICLE_TYPE_TURRET || !def->Get_Transition_List().Count()) return false;
    auto* phys=DefinitionMgrClass::Find_Definition(def->Get_Phys_Def_ID(),true);
    return phys && !static_cast<PhysDefClass*>(phys)->Get_Model_Name().Is_Empty();
}
static void Loaded() {
    AdminWeapons::Loaded();
    vehicles.clear(); spawned.clear(); probeObject=0; probeActive=false;
    for (auto* d=DefinitionMgrClass::Get_First(CID_Vehicle); d; d=DefinitionMgrClass::Get_Next(d,CID_Vehicle)) {
        auto* def=static_cast<VehicleGameObjDef*>(d);
        if (Drivable(def)) vehicles.push_back({def->Get_ID(),def->Get_Name(),def->Get_Seat_Count(),def->Get_Type()==VEHICLE_TYPE_FLYING});
    }
    std::sort(vehicles.begin(),vehicles.end(),[](const Vehicle& a,const Vehicle& b){return a.preset<b.preset;});
    Console_Output("[Novara] Admin catalog: %u driveable vehicle presets.\n",static_cast<unsigned>(vehicles.size()));
}
static VehicleGameObj* Create(const Vehicle& item,const Vector3& position,float facing,bool diagnostic=false) {
    auto* object=Commands->Create_Object(item.preset.c_str(),position);
    auto* vehicle=object ? object->As_VehicleGameObj() : nullptr;
    if (!vehicle) { if (object) Commands->Destroy_Object(object); return nullptr; }
    // A fresh admin vehicle must not run campaign AI/objective scripts.
    Remove_All_Scripts(vehicle);
    vehicle->Set_Can_Drive(true); vehicle->Set_Immovable(false);
    vehicle->Set_Lock_Team(1); vehicle->Set_Can_Be_Stolen(true);
    Commands->Set_Player_Type(vehicle,1); Commands->Set_Facing(vehicle,facing);
    Commands->Enable_Vehicle_Transitions(vehicle,true);
    auto* physics=vehicle->Peek_Physical_Object()->As_MoveablePhysClass();
    Vector3 safe;
    if (!physics || (!diagnostic && !physics->Find_Teleport_Location(position,6.0f,&safe))) {
        Commands->Destroy_Object(vehicle); return nullptr;
    }
    physics->Set_Velocity(Vector3(0,0,0));
    if (diagnostic) { physics->Set_Immovable(true); physics->Set_Gravity_Multiplier(0); }
    else Commands->Set_Position(vehicle,safe);
    return vehicle;
}
static bool ChangeMap(const std::string& name) {
    std::vector<std::string> maps; int target=-1;
    for (int i=0; i<Get_Map_Count(); ++i) {
        maps.emplace_back(Get_Map(i));
        if (TextRules::MapName(maps.back())==TextRules::MapName(name)) target=i;
    }
    if (target<0) return false;
    const auto rotated=TextRules::Rotate(maps,Get_Current_Map_Index(),target);
    if (rotated.empty()) return false;
    for (unsigned i=0; i<rotated.size(); ++i) if (!Set_Map(rotated[i].c_str(),i)) {
        for (unsigned j=0; j<maps.size(); ++j) Set_Map(maps[j].c_str(),j);
        return false;
    }
    Console_Input("gameover"); return true;
}
static void Probe() {
    if (!DASettingsManager::Get_Bool("CampaignDiagnostics",false) || The_Game()->Get_Current_Players()!=0 || probeActive) return;
    probeActive=true; probeIndex=0; probeObject=0; probeTime=0; probePass=true;
}
static void Tick() {
    if (!probeActive || GetTickCount()-probeTime<500) return;
    if (The_Game()->Get_Current_Players()!=0) { probeActive=false; if (auto* o=Commands->Find_Object(probeObject)) Commands->Destroy_Object(o); return; }
    if (probeObject) {
        auto* object=Commands->Find_Object(probeObject);
        const bool valid=object && object->As_VehicleGameObj() && object->As_VehicleGameObj()->Can_Drive() && Commands->Get_Health(object)>0;
        probePass &= valid;
        Console_Output("[Novara] DIAGNOSTIC vehicle=%s result=%s\n",vehicles[probeIndex-1].preset.c_str(),valid ? "PASS" : "FAIL");
        if (object) Commands->Destroy_Object(object);
        probeObject=0;
    }
    if (probeIndex>=static_cast<int>(vehicles.size())) {
        Console_Output("[Novara] DIAGNOSTIC vehicles result=%s count=%u; no client driving test.\n",probePass ? "PASS" : "FAIL",static_cast<unsigned>(vehicles.size()));
        probeActive=false; return;
    }
    auto* vehicle=Create(vehicles[probeIndex++],Vector3(10000,10000,10000),0,true);
    if (vehicle) probeObject=Commands->Get_ID(vehicle); else probePass=false;
    probeTime=GetTickCount();
}
}
