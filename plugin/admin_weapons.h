// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "WeaponBagClass.h"
#include "WeaponClass.h"
namespace AdminWeapons {
static std::vector<const WeaponDefinitionClass*> catalog;
static void Loaded() {
    catalog.clear();
    for (auto* d=DefinitionMgrClass::Get_First(CID_Weapon); d; d=DefinitionMgrClass::Get_Next(d,CID_Weapon)) {
        const auto* w=static_cast<const WeaponDefinitionClass*>(d);
        // Portable weapons must have a first-person model and valid ammunition.
        // Vehicle/turret-only and unfinished placeholder definitions are excluded.
        if (!w->FirstPersonModel.Is_Empty() && w->PrimaryAmmoDefID &&
            DefinitionMgrClass::Find_Definition(w->PrimaryAmmoDefID,true)) catalog.push_back(w);
    }
    std::sort(catalog.begin(),catalog.end(),[](const WeaponDefinitionClass* a,const WeaponDefinitionClass* b) {
        return strcmp(a->Get_Name(),b->Get_Name())<0;
    });
    Console_Output("[Novara] Admin catalog: %u portable weapon presets.\n",static_cast<unsigned>(catalog.size()));
}
static bool Fill(SoldierGameObj* soldier,const WeaponDefinitionClass* weapon) {
    if (!soldier || !weapon || !soldier->Get_Weapon_Bag()->Find_Weapon(weapon)) return false;
    Set_Bullets(soldier,weapon->Get_Name(),Get_Max_Bullets(soldier,weapon->Get_Name()));
    Set_Clip_Bullets(soldier,weapon->Get_Name(),Get_Max_Clip_Bullets(soldier,weapon->Get_Name()));
    soldier->Get_Weapon_Bag()->Force_Changed();
    return true;
}
static bool Give(SoldierGameObj* soldier,const WeaponDefinitionClass* weapon) {
    if (!soldier || !weapon) return false;
    auto* bag=soldier->Get_Weapon_Bag();
    if (!bag->Find_Weapon(weapon) && !bag->Add_Weapon(weapon,0,true)) return false;
    return Fill(soldier,weapon);
}
static unsigned Refill(SoldierGameObj* soldier) {
    unsigned count=0;
    if (soldier) for (int i=1; i<soldier->Get_Weapon_Bag()->Get_Count(); ++i)
        if (auto* w=soldier->Get_Weapon_Bag()->Peek_Weapon(i)) count+=Fill(soldier,w->Get_Definition()) ? 1:0;
    return count;
}
static void Probe() {
    if (!DASettingsManager::Get_Bool("CampaignDiagnostics",false) || The_Game()->Get_Current_Players()!=0) return;
    auto* object=Commands->Create_Object("CnC_GDI_MiniGunner_0",Vector3(10000,10000,10000));
    auto* soldier=object ? object->As_SoldierGameObj() : nullptr;
    bool pass=soldier && !catalog.empty(); unsigned tested=0;
    if (soldier) {
        Remove_All_Scripts(soldier);
        const float hp=Commands->Get_Health(soldier),shield=Commands->Get_Shield_Strength(soldier);
        const int keys=soldier->Get_Key_Ring();
        for (const auto* w : catalog) {
            bool valid=Give(soldier,w);
            const int count=Get_Weapon_Count(soldier);
            valid=Give(soldier,w) && Get_Weapon_Count(soldier)==count && valid;
            Set_Bullets(soldier,w->Get_Name(),0); Set_Clip_Bullets(soldier,w->Get_Name(),0);
            Refill(soldier);
            valid=valid && Get_Bullets(soldier,w->Get_Name())==Get_Max_Bullets(soldier,w->Get_Name())
                && Get_Clip_Bullets(soldier,w->Get_Name())==Get_Max_Clip_Bullets(soldier,w->Get_Name());
            pass=pass && valid; ++tested;
            if (!valid) Console_Output("[Novara] DIAGNOSTIC weapon FAIL: %s\n",w->Get_Name());
            soldier->Get_Weapon_Bag()->Remove_Weapon(w);
        }
        pass=pass && Commands->Get_Health(soldier)==hp && Commands->Get_Shield_Strength(soldier)==shield && soldier->Get_Key_Ring()==keys;
    }
    if (object) Commands->Destroy_Object(object);
    Console_Output("[Novara] DIAGNOSTIC weapons result=%s count=%u; inventory/refill only, no human client test.\n",pass?"PASS":"FAIL",tested);
}
}
