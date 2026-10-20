// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "coop_rules.h"
#include "PhysDefClass.h"

namespace CoopFeatures {
struct PendingTeleport {
    int soldier=0,target=0; unsigned targetConnection=0;
    Vector3 origin,destination; float facing=0; DWORD started=0;
};
static std::map<int,PendingTeleport> teleports;
static std::map<int,DWORD> lastCombat,lastTeleport;
static std::map<int,unsigned> connections;
static unsigned nextConnection=0;
static std::map<int,std::string> skins;
static std::map<int,std::string> defaultModels;
static std::map<int,unsigned> modelPresets;
static bool active=false;

static int Seconds(const char* key,int fallback,int maximum=300) {
    return (std::max)(0,(std::min)(maximum,DASettingsManager::Get_Int(key,fallback)));
}
static bool Session(cPlayer* p) { return p && p->Is_Active() && p->Get_Player_Type()==1; }
static std::vector<cPlayer*> Team() {
    std::vector<cPlayer*> result;
    for (int id=1; id<=127; ++id) if (Session(Find_Player(id))) result.push_back(Find_Player(id));
    return result;
}
static bool Mobile(SoldierGameObj* obj) {
    return Campaign::Human(obj) && Commands->Get_Health(obj)>0 && !Get_Vehicle(obj) &&
        !obj->Is_On_Ladder() && !obj->Is_In_Elevator() && obj->Is_Control_Enabled() &&
        !obj->Get_Fly_Mode() && !Is_Script_Attached(obj,"MPF_Player_Cant_Use_Goto") &&
        !Is_Script_Attached(obj,"JFW_Spy_Switch_Team") && Commands->Get_Player_Type(obj)==1;
}
static bool InCombat(int id,DWORD now) {
    auto it=lastCombat.find(id);
    return it!=lastCombat.end() && !CoopRules::Elapsed(now,it->second,Seconds("CampaignCatchupCombatSeconds",10)*1000);
}
static bool Destination(Vector3& position,float& angle) {
    if (Campaign::state.active() && Campaign::ready && Campaign::spawnValid) {
        position=Campaign::spawn; angle=Campaign::facing; return true;
    }
    // Legacy missions gain a shared catch-up point at the latest supply
    // terminal reached by a player, never at an unvisited objective marker.
    for (auto it=CoopServices::reachedTerminals.rbegin(); it!=CoopServices::reachedTerminals.rend(); ++it) {
        if (auto* obj=Commands->Find_Object(*it)) {
            position=Commands->Get_Position(obj); angle=Commands->Get_Facing(obj); return true;
        }
    }
    return false;
}
static bool Move(SoldierGameObj* obj,const Vector3& destination,float angle) {
    auto* movable=obj->Peek_Physical_Object()->As_MoveablePhysClass();
    Vector3 safe;
    if (!movable || !movable->Find_Teleport_Location(destination,4.0f,&safe)) return false;
    movable->Set_Velocity(Vector3(0,0,0));
    Commands->Set_Position(obj,safe); Commands->Set_Facing(obj,angle); return true;
}
static void PlaceLegacyRespawn(GameObject* obj) {
    if (!obj || !active || Campaign::state.active() || Campaign::purchasedCharacters.count(Commands->Get_ID(obj)) ||
        !DASettingsManager::Get_Bool("CampaignCatchup",true) ||
        !Mobile(obj->As_SoldierGameObj())) return;
    // Keep the original CoopBeta spawn/teleport progression.
    Campaign::ApplyKeys(obj->As_SoldierGameObj());
}
static std::string SkinModel(const char* name) { return Cosmetics::Resolve(name); }
static void ApplySkin(cPlayer* player) {
    auto* obj=player->Get_GameObj();
    if (!Mobile(obj)) return;
    const int id=Commands->Get_ID(obj);
    const auto preset=obj->Get_Definition().Get_ID();
    if (!defaultModels.count(id) || modelPresets[id]!=preset) {
        defaultModels[id]=Get_Model(obj); modelPresets[id]=preset;
    }
    auto it=skins.find(player->Get_Id()); if (it==skins.end()) return;
    const auto model=SkinModel(it->second.c_str());
    if (!model.empty() && _stricmp(model.c_str(),Get_Model(obj))) Commands->Set_Model(obj,model.c_str());
}
static void Loaded() {
    active=Campaign::CoopMap(); teleports.clear(); lastCombat.clear(); lastTeleport.clear();
    defaultModels.clear(); modelPresets.clear();
}
static void Tick() {
    if (!active) return;
    const DWORD now=GetTickCount(); const auto players=Team();
    for (auto* p : players) {
        if (DASettingsManager::Get_Bool("CampaignSkins",true)) ApplySkin(p);
        auto it=teleports.find(p->Get_Id()); if (it==teleports.end()) continue;
        const auto request=it->second; auto* obj=p->Get_GameObj();
        auto* target=request.target ? Find_Player(request.target) : nullptr;
        if (!DASettingsManager::Get_Bool(request.target ? "CampaignPlayerGoto" : "CampaignCatchup",true) ||
            (Campaign::state.active() && Campaign::state.outcome!=Campaign::Outcome::Running) ||
            !Mobile(obj) || Commands->Get_ID(obj)!=request.soldier ||
            Commands->Get_Distance(request.origin,Commands->Get_Position(obj))>1.0f || InCombat(p->Get_Id(),now) ||
            (request.target && (!Session(target) || connections[request.target]!=request.targetConnection ||
                !Mobile(target->Get_GameObj()) || InCombat(request.target,now)))) {
            DA::Page_Player(p,"Catch-up cancelled: movement, combat or target unavailable."); teleports.erase(it); continue;
        }
        if (!CoopRules::Elapsed(now,request.started,Seconds("CampaignCatchupWarmupSeconds",3)*1000)) continue;
        const auto destination=target ? Commands->Get_Position(target->Get_GameObj()) : request.destination;
        const auto angle=target ? Commands->Get_Facing(target->Get_GameObj()) : request.facing;
        if (Move(obj,destination,angle)) {
            lastTeleport[p->Get_Id()]=now; Campaign::ApplyKeys(obj);
            DA::Page_Player(p,"Catch-up complete. Your weapons, health and credits are unchanged.");
        } else DA::Page_Player(p,"No safe standing position near the destination. Try again later.");
        teleports.erase(it);
    }
}
class Events : public DAEventClass {
    bool Enabled(cPlayer* p,const char* setting) {
        if (active && Session(p) && DASettingsManager::Get_Bool(setting,true)) return true;
        DA::Page_Player(p,"This coop command is disabled here."); return false;
    }
    bool Queue(cPlayer* p,cPlayer* target) {
        auto* obj=p->Get_GameObj(); const DWORD now=GetTickCount();
        if ((Campaign::state.active() && Campaign::state.outcome!=Campaign::Outcome::Running) ||
            !Mobile(obj) || InCombat(p->Get_Id(),now) ||
            (target && (!Mobile(target->Get_GameObj()) || InCombat(target->Get_Id(),now)))) {
            DA::Page_Player(p,"Catch-up requires living teammates on foot, out of combat and outside restricted sequences."); return false;
        }
        auto used=lastTeleport.find(p->Get_Id());
        if (used!=lastTeleport.end() && !CoopRules::Elapsed(now,used->second,Seconds("CampaignCatchupCooldownSeconds",30)*1000)) {
            DA::Page_Player(p,"Catch-up is cooling down. Please wait."); return false;
        }
        PendingTeleport request; request.origin=Commands->Get_Position(obj); request.started=now;
        request.soldier=Commands->Get_ID(obj);
        if (target) { request.target=target->Get_Id(); request.targetConnection=connections[target->Get_Id()]; }
        else if (!Destination(request.destination,request.facing)) {
            DA::Page_Player(p,"No shared checkpoint or supply point has been reached yet."); return false;
        }
        teleports[p->Get_Id()]=request;
        DA::Page_Player(p,"Catch-up in %d seconds. Stay still and out of combat.",Seconds("CampaignCatchupWarmupSeconds",3)); return false;
    }
    void Transfer(cPlayer* p,const std::vector<cPlayer*>& targets,int amount) {
        std::vector<CoopRules::Account> accounts;
        for (auto* t : targets) accounts.push_back({t->Get_Id(),Get_Money(t->Get_Id())});
        CoopRules::Transfer plan;
        if (!CoopRules::PlanTransfer({p->Get_Id(),Get_Money(p->Get_Id())},accounts,amount,plan)) {
            DA::Page_Player(p,"Transfer refused: check your credits, amount and connected teammates."); return;
        }
        // The event thread is single-threaded: validate every account before
        // changing any balance, including teammates who are currently dead.
        Set_Money(p->Get_Id(),plan.donorBalance);
        for (const auto& recipient : plan.recipients) {
            Set_Money(recipient.id,recipient.balance);
            DA::Page_Player(Find_Player(recipient.id),"%ls sent you %d credits.",p->Get_Name(),plan.each);
        }
        DA::Page_Player(p,"Sent %d credits (%d per teammate).",plan.total,plan.each);
    }
public:
    Events() {
        Register_Event(DAEvent::PLAYERJOIN); Register_Event(DAEvent::PLAYERLEAVE);
        Register_Event(DAEvent::CHANGECHARACTER);
        Register_Object_Event(DAObjectEvent::DESTROYED,DAObjectEvent::PLAYER);
        Register_Object_Event(DAObjectEvent::DAMAGERECEIVED,DAObjectEvent::ALL);
        Register_Chat_Command((DAECC)&Events::Checkpoint,"!cp|!checkpoint|!catchup",0);
        Register_Chat_Command((DAECC)&Events::Goto,"!go|!goto",1);
        Register_Chat_Command((DAECC)&Events::Skin,"!skin",0);
        Register_Chat_Command((DAECC)&Events::Skins,"!skins",0);
        Register_Chat_Command((DAECC)&Events::Donate,"!donate|!d",0);
        Register_Chat_Command((DAECC)&Events::TeamDonate,"!tdonate|!teamdonate",0);
    }
    void Player_Join_Event(cPlayer* p) override {
        Player_Leave_Event(p); connections[p->Get_Id()]=++nextConnection;
    }
    void Player_Leave_Event(cPlayer* p) override {
        const int id=p->Get_Id(); teleports.erase(id); lastCombat.erase(id); lastTeleport.erase(id); skins.erase(id); connections.erase(id);
    }
    void Change_Character_Event(cPlayer* p,const SoldierGameObjDef*) override {
        if (auto* obj=p->Get_GameObj()) { defaultModels.erase(Commands->Get_ID(obj)); modelPresets.erase(Commands->Get_ID(obj)); }
        teleports.erase(p->Get_Id());
    }
    void Object_Destroyed_Event(GameObject* obj) override {
        const int id=Commands->Get_ID(obj); defaultModels.erase(id); modelPresets.erase(id);
        Campaign::purchasedCharacters.erase(id); Campaign::observedKeys.erase(id);
    }
    void Damage_Event(DamageableGameObj* victim,ArmedGameObj* damager,float amount,unsigned int,float,DADamageType::Type) override {
        if (!active || amount<=0 || !std::isfinite(amount)) return;
        for (GameObject* obj : {static_cast<GameObject*>(victim),static_cast<GameObject*>(damager)}) {
            if (Campaign::Human(obj)) lastCombat[Get_Player_ID(obj)]=GetTickCount();
            if (auto* vehicle=obj ? obj->As_VehicleGameObj() : nullptr) {
                for (auto* occupant : {vehicle->Get_Driver(),vehicle->Get_Gunner()})
                    if (Campaign::Human(occupant)) lastCombat[Get_Player_ID(occupant)]=GetTickCount();
            }
        }
    }
    bool Checkpoint(cPlayer* p,const DATokenClass&,TextMessageEnum) {
        return Enabled(p,"CampaignCatchup") ? Queue(p,nullptr) : false;
    }
    bool Goto(cPlayer* p,const DATokenClass& text,TextMessageEnum) {
        if (!Enabled(p,"CampaignPlayerGoto")) return false;
        auto* target=Match_Player(p,text[1],true,false);
        return Session(target) ? Queue(p,target) : false;
    }
    bool Skins(cPlayer* p,const DATokenClass& text,TextMessageEnum) { if (Enabled(p,"CampaignSkins")) Cosmetics::List(p,text); return false; }
    bool Skin(cPlayer* p,const DATokenClass& text,TextMessageEnum) {
        if (!Enabled(p,"CampaignSkins")) return false;
        if (!text.Size()) { Cosmetics::List(p,text); return false; }
        if (text.Size()!=1 || !Mobile(p->Get_GameObj())) { DA::Page_Player(p,"Use !skin <name> while alive and on foot."); return false; }
        const char* name=text[1].Peek_Buffer(); ApplySkin(p);
        if (!_stricmp(name,"default")) {
            skins.erase(p->Get_Id()); const auto model=defaultModels[Commands->Get_ID(p->Get_GameObj())];
            if (!model.empty()) Commands->Set_Model(p->Get_GameObj(),model.c_str());
        } else {
            if (SkinModel(name).empty()) { DA::Page_Player(p,"Unknown or unavailable skin. Use !skin to list choices."); return false; }
            skins[p->Get_Id()]=SkinModel(name); ApplySkin(p);
        }
        DA::Page_Player(p,"Cosmetic skin updated. Weapons, health, armor and credits are unchanged."); return false;
    }
    bool Donate(cPlayer* p,const DATokenClass& text,TextMessageEnum) {
        if (!Enabled(p,"CampaignDonations")) return false;
        int amount=0;
        if (text.Size()!=2 || !CoopRules::Credits(text[2].Peek_Buffer(),amount)) {
            DA::Page_Player(p,"Use !donate <player name/ID> <whole credits, 1-1000000>."); return false;
        }
        auto* target=Match_Player(p,text[1],true,false);
        if (Session(target)) Transfer(p,{target},amount); return false;
    }
    bool TeamDonate(cPlayer* p,const DATokenClass& text,TextMessageEnum) {
        if (!Enabled(p,"CampaignDonations")) return false;
        int amount=0;
        if (text.Size()!=1 || !CoopRules::Credits(text[1].Peek_Buffer(),amount)) {
            DA::Page_Player(p,"Use !tdonate <whole credits, 1-1000000>. Remainder stays with you."); return false;
        }
        auto targets=Team(); targets.erase(std::remove(targets.begin(),targets.end(),p),targets.end());
        Transfer(p,targets,amount); return false;
    }

};
static void Dump() {
    Vector3 point; float angle=0; const bool checkpoint=Destination(point,angle);
    Console_Output("[Novara] features checkpoint=%d pending_teleports=%u skins=%u\n",
        checkpoint,static_cast<unsigned>(teleports.size()),static_cast<unsigned>(skins.size()));
}
static void Probe() {
    if (!active || !DASettingsManager::Get_Bool("CampaignDiagnostics",false) || The_Game()->Get_Current_Players()!=0) {
        Console_Output("[Novara] Feature probe requires an isolated empty diagnostic server.\n"); return;
    }
    auto* actor=Commands->Create_Object("CnC_GDI_MiniGunner_0",Vector3(10000,10000,10000));
    if (!actor || !actor->As_SoldierGameObj()) {
        Console_Output("[Novara] DIAGNOSTIC features result=FAIL actor unavailable\n"); return;
    }
    auto* soldier=actor->As_SoldierGameObj();
    const auto preset=soldier->Get_Definition().Get_ID();
    const float health=Commands->Get_Health(actor),shield=Commands->Get_Shield_Strength(actor);
    const int weapons=Get_Weapon_Count(actor),keys=soldier->Get_Key_Ring();
    const Vector3 position=Commands->Get_Position(actor);
    const std::string original=Get_Model(actor);
    bool pass=true; int tested=0;
    for (const auto& entry : Cosmetics::catalog) {
        const char* skin=entry.model.c_str();
        const auto model=SkinModel(skin);
        Console_Output("[Novara] DIAGNOSTIC loading skin=%s model=%s\n",skin,model.c_str());
        if (!model.empty()) Commands->Set_Model(actor,model.c_str());
        const bool valid=!model.empty() && !_stricmp(Get_Model(actor),model.c_str()) &&
            soldier->Get_Definition().Get_ID()==preset && Commands->Get_Health(actor)==health &&
            Commands->Get_Shield_Strength(actor)==shield && Get_Weapon_Count(actor)==weapons &&
            soldier->Get_Key_Ring()==keys && Commands->Get_Distance(position,Commands->Get_Position(actor))<.001f;
        Console_Output("[Novara] DIAGNOSTIC skin=%s model=%s preserved=%d\n",skin,model.c_str(),valid);
        pass=pass && valid; ++tested;
    }
    Commands->Set_Model(actor,original.c_str());
    Vector3 destination; float angle=0; const bool available=Destination(destination,angle);
    const bool moved=available && Move(soldier,destination,angle);
    const float distance=moved ? Commands->Get_Distance(destination,Commands->Get_Position(actor)) : -1;
    if (Campaign::state.active()) pass=pass && available && moved && distance<=4.1f;
    Console_Output("[Novara] DIAGNOSTIC features result=%s skins=%d checkpoint=%d moved=%d distance=%.3f; synthetic actor, no human playtest.\n",
        pass ? "PASS" : "FAIL",tested,available,moved,distance);
    Commands->Destroy_Object(actor);
}
}
