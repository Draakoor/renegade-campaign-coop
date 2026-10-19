// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "PowerUpGameObj.h"
#include "PowerUpGameObjDef.h"
#include "PurchaseSettingsDefClass.h"
#include "TeamPurchaseSettingsDefClass.h"
#include "SoldierGameObjDef.h"
#include "da_chatcommand.h"
#include <set>

namespace CoopServices {
struct Pickup {
    int object = 0, spawner = 0;
    std::string preset;
    Matrix3D transform;
    bool key = false, pending = false;
    DWORD collected = 0;
};
static std::vector<Pickup> pickups;
static std::vector<int> terminals;
static std::set<int> knownSpawners;
static bool active = false, startTerminal = false;
static int lastCheckpoint = 0;
static int probeActor = 0;
static std::vector<std::pair<int,int>> probePickups;
static DWORD probeStarted = 0;
static bool Eligible(const PowerUpGameObjDef& d) {
    return d.Get_Grant_Weapon_ID() != 0 || (d.GrantKey > 0 && d.GrantKey < 32);
}
static int Index(int id) {
    for (unsigned i=0; i<pickups.size(); ++i) if (pickups[i].object == id) return static_cast<int>(i);
    return -1;
}
static SpawnerClass* Spawner(int id) {
    if (!id) return nullptr;
    auto& list = Campaign::Spawners();
    for (int i=0; i<list.Count(); ++i) if (list[i]->id == id) return list[i];
    return nullptr;
}
static void Track(PowerUpGameObj* obj,int spawner = 0) {
    if (!obj || !Eligible(obj->Get_Definition())) return;
    const int id = Commands->Get_ID(obj);
    const int index = Index(id);
    if (index >= 0) { if (spawner) pickups[index].spawner = spawner; return; }
    Pickup p; p.object = id; p.spawner = spawner;
    p.preset = Commands->Get_Preset_Name(obj); p.transform = obj->Get_Transform();
    p.key = obj->Get_Definition().GrantKey > 0;
    pickups.push_back(p);
    Attach_Script_Once(obj,"Novara_Static_Pickup","");
}
static void Granted(GameObject* obj,GameObject* collector) {
    if (!active || !obj) return;
    auto* powerup = obj->As_PhysicalGameObj() ? obj->As_PhysicalGameObj()->As_PowerUpGameObj() : nullptr;
    if (!powerup) return;
    const auto& d = powerup->Get_Definition();
    if (Campaign::Human(collector) && d.GrantKey > 0 && d.GrantKey < 32)
        Campaign::Key(collector,d.GrantKey,true);
    const int index = Index(Commands->Get_ID(obj));
    if (index < 0 || pickups[index].pending || d.Is_Persistant()) return;
    auto& p = pickups[index];
    // Initial level load precedes the engine's ground placement. Remember the
    // position at collection, after the original pickup has settled.
    p.transform = powerup->Get_Transform();
    p.pending = true; p.collected = GetTickCount();
}
static void ConfigurePurchases() {
    // Campaigns have no multiplayer soldier factory. Expose existing GDI
    // infantry pages; costs, icons and normal character presets stay stock.
    Set_Can_Generate_Soldiers(1,true);
    for (auto type : {PurchaseSettingsDefClass::TYPE_CLASSES,PurchaseSettingsDefClass::TYPE_SECRET_CLASSES}) {
        auto* page = PurchaseSettingsDefClass::Find_Definition(type,PurchaseSettingsDefClass::TEAM_GDI);
        if (!page) continue;
        page->Set_Page_Hidden(false); page->Set_Page_Disabled(false); page->Set_Page_Busy(false);
        for (int i=0; i<10; ++i) {
            page->Set_Hidden(i,false); page->Set_Disabled(i,false); page->Set_Busy(i,false);
            page->Set_Tech_Level(i,0);
        }
    }
}
static bool NearTerminal(GameObject* player) {
    if (!Campaign::Human(player) || Commands->Get_Health(player) <= 0 || Get_Vehicle(player) || Commands->Get_Player_Type(player) != 1) return false;
    for (const auto id : terminals) if (auto* obj = Commands->Find_Object(id))
        if (Commands->Get_Distance(Commands->Get_Position(obj),Commands->Get_Position(player)) <= 6.0f) return true;
    return false;
}
static bool AddTerminal(const Vector3& position,float angle,float spacing) {
    if (!DASettingsManager::Get_Bool("CampaignTerminals",true)) return false;
    for (const auto id : terminals) if (auto* obj = Commands->Find_Object(id))
        if (Commands->Get_Distance(position,Commands->Get_Position(obj)) < spacing) return false;
    auto* obj = Commands->Create_Object("Daves Arrow",position);
    if (!obj) return false;
    StringClass model; DASettingsManager::Get_String(model,"CampaignTerminalModel","o_nod_console-o");
    Commands->Set_Model(obj,model.Peek_Buffer());
    Commands->Set_Facing(obj,angle); Commands->Set_Player_Type(obj,1);
    // Standalone stock console; no building destruction/objective connection.
    obj->As_PhysicalGameObj()->Set_Collision_Group(UNCOLLIDEABLE_GROUP);
    obj->As_PhysicalGameObj()->Get_Defense_Object()->Set_Can_Object_Die(false);
    Commands->Set_Obj_Radar_Blip_Shape(obj,RADAR_BLIP_SHAPE_OBJECTIVE);
    Commands->Set_Obj_Radar_Blip_Color(obj,RADAR_BLIP_COLOR_GDI);
    Attach_Script_Once(obj,"Novara_Coop_Terminal","");
    terminals.push_back(Commands->Get_ID(obj));
    Console_Output("[Novara] Coop terminal at %.2f,%.2f,%.2f\n",position.X,position.Y,position.Z);
    return true;
}
static void Loaded() {
    pickups.clear(); terminals.clear(); knownSpawners.clear();
    active = Campaign::CoopMap(); startTerminal = false; lastCheckpoint = 0;
    probeActor = 0; probePickups.clear(); probeStarted = 0;
    if (!active) return;
    // Snapshot only map-loaded objects. Later NPC/player drops never enter it.
    for (auto* n = GameObjManager::GameObjList.Head(); n; n=n->Next()) {
        auto* p = n->Data()->As_PhysicalGameObj();
        if (p) Track(p->As_PowerUpGameObj());
    }
    auto& list = Campaign::Spawners();
    for (int i=0; i<list.Count(); ++i) {
        const auto& defs = list[i]->definition->SpawnDefinitionIDList;
        bool eligible = defs.Count() > 0;
        for (int j=0; j<defs.Count(); ++j) {
            auto* d = DefinitionMgrClass::Find_Definition(defs[j],true);
            if (!d || d->Get_Class_ID() != CID_PowerUp || !Eligible(*static_cast<PowerUpGameObjDef*>(d))) eligible=false;
        }
        if (eligible) knownSpawners.insert(list[i]->id);
    }
    if (DASettingsManager::Get_Bool("CampaignTerminals",true)) ConfigurePurchases();
    Console_Output("[Novara] Resupply map=%s fixed_pickups=%u powerup_spawners=%u shared_keys=%d\n",
        The_Game()->Get_Map_Name(),static_cast<unsigned>(pickups.size()),static_cast<unsigned>(knownSpawners.size()),DASettingsManager::Get_Bool("CampaignSharedKeys",true));
}
static void Tick() {
    if (!active) return;
    Campaign::TickSharedKeys();
    // Only original map spawners qualify, including ones enabled later by a
    // mission trigger. Never revive a pickup merely destroyed by a script.
    for (int id : knownSpawners) {
        auto* s = Spawner(id); if (!s) continue;
        auto* obj = s->currentObject.Get_Ptr();
        if (obj && obj->As_PhysicalGameObj()) {
            auto* powerup = obj->As_PhysicalGameObj()->As_PowerUpGameObj();
            if (!powerup) continue;
            bool bound = false;
            for (auto& p : pickups) if (p.spawner == id) {
                bound = true;
                const int current = Commands->Get_ID(powerup);
                if (current != p.object) {
                    // Native spawner replenished first: discard our replacement
                    // if present, then follow the authoritative engine object.
                    if (auto* old = Commands->Find_Object(p.object)) Commands->Destroy_Object(old);
                    p.object = current; p.pending = false;
                    Attach_Script_Once(powerup,"Novara_Static_Pickup","");
                }
                break;
            }
            if (!bound) Track(powerup,id);
        }
    }
    const DWORD now = GetTickCount();
    for (auto& p : pickups) {
        if (!p.pending || Commands->Find_Object(p.object)) continue;
        int seconds = DASettingsManager::Get_Int(p.key ? "CampaignKeyRespawnSeconds" : "CampaignWeaponRespawnSeconds",p.key ? 5 : 15);
        if (seconds <= 0) continue;
        seconds = (std::min)(seconds,3600);
        if (now-p.collected < static_cast<DWORD>(seconds)*1000) continue;
        if (auto* s = Spawner(p.spawner)) {
            if (s->currentObject.Get_Ptr()) continue;
            // Unlimited/remaining native spawns already own replenishment.
            if (s->enabled && (s->definition->SpawnMax < 0 || s->spawnCount < s->definition->SpawnMax)) continue;
        }
        auto* obj = Commands->Create_Object(p.preset.c_str(),p.transform.Get_Translation());
        if (!obj || !obj->As_PhysicalGameObj()) continue;
        // Create_Object gives some weapon presets drop physics. These are
        // fixed supply points, so replacements must not roll/fall away.
        auto* physics = obj->As_PhysicalGameObj()->Peek_Physical_Object();
        physics->Set_Immovable(true);
        if (auto* movable = physics->As_MoveablePhysClass()) {
            movable->Set_Velocity(Vector3(0,0,0));
            movable->Set_Gravity_Multiplier(0);
        }
        obj->As_PhysicalGameObj()->Set_Transform(p.transform);
        p.object = Commands->Get_ID(obj); p.pending = false;
        // Instance mission scripts intentionally run only on the original pickup.
        Attach_Script_Once(obj,"Novara_Static_Pickup","");
        Console_Output("[Novara] Fixed %s respawned: %s\n",p.key ? "key" : "weapon",p.preset.c_str());
    }
    if (probeStarted && now-probeStarted > 18000) {
        bool pass = !probePickups.empty();
        for (const auto& test : probePickups) {
            const auto& p = pickups[test.first];
            auto* obj = Commands->Find_Object(p.object);
            const bool restored = obj && p.object != test.second && !p.pending &&
                Commands->Get_Distance(Commands->Get_Position(obj),p.transform.Get_Translation()) < .25f;
            pass &= restored;
            Console_Output("[Novara] DIAGNOSTIC resupply %s restored=%d original=%d current=%d alive=%d pending=%d distance=%.3f\n",p.preset.c_str(),restored,test.second,p.object,obj != nullptr,p.pending,obj ? Commands->Get_Distance(Commands->Get_Position(obj),p.transform.Get_Translation()) : -1.0f);
        }
        Console_Output("[Novara] DIAGNOSTIC resupply result=%s; synthetic actor, no client/menu playtest.\n",pass ? "PASS" : "FAIL");
        if (auto* obj = Commands->Find_Object(probeActor)) Commands->Destroy_Object(obj);
        probeActor=0; probeStarted=0; probePickups.clear();
    }
    if (!DASettingsManager::Get_Bool("CampaignTerminals",true)) return;
    const auto players = Campaign::Players();
    if (players.empty()) return;
    auto* player = players.front();
    if (!startTerminal && !Get_Vehicle(player)) {
        // A human-occupied position is reachable; the prop has no collision.
        AddTerminal(Commands->Get_Position(player),Commands->Get_Facing(player),12.0f);
        startTerminal = true;
    }
    if (Campaign::state.checkpoint > lastCheckpoint) {
        AddTerminal(Campaign::spawn,Campaign::facing,12.0f);
        lastCheckpoint = Campaign::state.checkpoint;
    }
    const float spacing = (std::max)(20.0f,DASettingsManager::Get_Float("CampaignTerminalSpacing",75.0f));
    for (const auto& pickup : pickups) {
        if (pickup.key) continue;
        for (auto* p : players) {
            if (Get_Vehicle(p)) continue;
            const auto pos = Commands->Get_Position(p);
            if (Commands->Get_Distance(pos,pickup.transform.Get_Translation()) <= 6.0f)
                AddTerminal(pos,Commands->Get_Facing(p),spacing);
        }
    }
}
static bool AvailableCharacter(const SoldierGameObjDef* def,int& cost) {
    const auto blamo = ArmorWarheadManager::Get_Armor_Type("Blamo");
    if (def->Get_Default_Player_Type() != 1 || def->Get_DefenseObjectDef().Skin == blamo || def->Get_DefenseObjectDef().ShieldType == blamo) return false;
    auto* free = TeamPurchaseSettingsDefClass::Get_Definition(TeamPurchaseSettingsDefClass::TEAM_GDI);
    if (free) for (int i=0; i<4; ++i) if (free->Get_Enlisted_Definition(i) == def->Get_ID() && !free->Get_Disabled(i) && !free->Get_Hidden(i)) {
        cost = free->Get_Enlisted_Cost(i); return true;
    }
    for (auto type : {PurchaseSettingsDefClass::TYPE_CLASSES,PurchaseSettingsDefClass::TYPE_SECRET_CLASSES}) {
        auto* page = PurchaseSettingsDefClass::Find_Definition(type,PurchaseSettingsDefClass::TEAM_GDI);
        if (!page || page->Get_Page_Hidden() || page->Get_Page_Disabled()) continue;
        for (int i=0; i<10; ++i) {
            if (page->Get_Hidden(i) || page->Get_Disabled(i)) continue;
            bool match = page->Get_Definition(i) == def->Get_ID();
            for (int j=0; j<3; ++j) match |= page->Get_Alt_Definition(i,j) == def->Get_ID();
            if (match) { cost = page->Get_Cost(i); return true; }
        }
    }
    return false;
}
class Events : public DAEventClass {
public:
    Events() {
        Register_Event(DAEvent::POWERUPGRANT);
        Register_Event(DAEvent::CHARACTERPURCHASEREQUEST,INT_MAX-10);
        Register_Chat_Command((DAECC)&Events::Buy,"!buy|!pt",0);
    }
    void PowerUp_Grant_Event(cPlayer* player,const PowerUpGameObjDef* def,PowerUpGameObj* obj) override {
        if (!active) return;
        if (obj) Granted(obj,player->Get_GameObj());
        if (def->GrantKey > 0 && def->GrantKey < 32) Campaign::Key(player->Get_GameObj(),def->GrantKey,true);
    }
    PurchaseStatus Character_Purchase_Request_Event(BaseControllerClass*,cPlayer* player,float& cost,const SoldierGameObjDef* item) override {
        if (!active || !DASettingsManager::Get_Bool("CampaignTerminals",true)) return PurchaseStatus_Allow;
        auto* obj = player->Get_GameObj(); int price = 0;
        if (!NearTerminal(obj) || !AvailableCharacter(item,price)) return PurchaseStatus_OutOfStock;
        cost = static_cast<float>((std::max)(0,price));
        if (!Purchase_Item(obj,static_cast<int>(cost))) return PurchaseStatus_InsufficientFunds;
        Campaign::TickSharedKeys();
        const uint32_t keys = static_cast<uint32_t>(obj->Get_Key_Ring());
        const int id = Commands->Get_ID(obj);
        Campaign::purchasedCharacters[id] = item->Get_Name();
        if (!Change_Character(obj,item->Get_Name())) {
            Commands->Give_Money(obj,cost,false); Campaign::purchasedCharacters.erase(id);
            return PurchaseStatus_OutOfStock;
        }
        if (DASettingsManager::Get_Bool("CampaignSharedKeys",true)) Campaign::ApplyKeys(obj);
        else for (int i=0; i<32; ++i) Campaign::originalKey(obj,i,(keys & (uint32_t(1) << i)) != 0);
        if (Campaign::state.mission == 6) Attach_Script_Once(obj,"M06_Havoc_DLS","");
        if (Campaign::state.mission == 7) Attach_Script_Once(obj,"M07_Havoc_DLS","");
        return PurchaseStatus_Granted;
    }
    bool Buy(cPlayer* player,const DATokenClass&,TextMessageEnum) {
        if (active && NearTerminal(player->Get_GameObj())) { ConfigurePurchases(); Display_GDI_Player_Terminal_Player(player->Get_GameObj()); }
        else DA::Page_Player(player,"Stand beside a Novara supply terminal to use !buy.");
        return false;
    }
};
static void Dump() {
    unsigned keys=0,pending=0;
    for (const auto& p : pickups) { keys += p.key; pending += p.pending; }
    Console_Output("[Novara] supplies map=%s weapons=%u keys=%u pending=%u terminals=%u shared_keys=%u\n",The_Game()->Get_Map_Name(),static_cast<unsigned>(pickups.size())-keys,keys,pending,static_cast<unsigned>(terminals.size()),Campaign::state.keys);
}
static void Probe() {
    if (!active || !DASettingsManager::Get_Bool("CampaignDiagnostics",false) || The_Game()->Get_Current_Players() != 0 || probeStarted) {
        Console_Output("[Novara] Supply probe requires an isolated empty diagnostic server.\n"); return;
    }
    auto* actor = Commands->Create_Object("Commando",Vector3(10000,10000,10000));
    if (!actor || !actor->As_SoldierGameObj()) return;
    probeActor=Commands->Get_ID(actor); probeStarted=GetTickCount();
    Commands->Clear_Weapons(actor);
    bool weapon=false,key=false;
    for (unsigned i=0; i<pickups.size(); ++i) {
        const auto& p=pickups[i];
        if (p.spawner || (p.key ? key : weapon)) continue;
        auto* obj=Commands->Find_Object(p.object);
        if (!obj || !obj->As_PhysicalGameObj() || !obj->As_PhysicalGameObj()->As_PowerUpGameObj()) continue;
        if (obj->As_PhysicalGameObj()->As_PowerUpGameObj()->Get_Definition().Is_Persistant()) continue;
        probePickups.emplace_back(i,p.object);
        const std::string preset=p.preset;
        (p.key ? key : weapon)=true;
        // The upstream DLL does not export the world-object Grant method.
        // Exercise its documented script notification, then simulated removal.
        // This validates replenishment, not real client collision/pickup.
        Commands->Give_PowerUp(actor,preset.c_str(),false);
        Commands->Send_Custom_Event(actor,obj,CUSTOM_EVENT_POWERUP_GRANTED,0,0);
        Console_Output("[Novara] DIAGNOSTIC simulated_collection=%s tracked=%d\n",preset.c_str(),pickups[i].pending);
        Commands->Destroy_Object(obj);
    }
    const auto count=pickups.size();
    auto* drop=Commands->Create_Object("POW_SniperRifle_Player",Vector3(10000,10000,10000));
    if (drop) {
        Console_Output("[Novara] DIAGNOSTIC runtime_drop_excluded=%d\n",Index(Commands->Get_ID(drop)) < 0 && pickups.size() == count);
        Commands->Destroy_Object(drop);
    }
    AddTerminal(Campaign::spawnValid ? Campaign::spawn : Vector3(10000,10000,10000),0,0);
    unsigned characters=0;
    for (auto* d=DefinitionMgrClass::Get_First(CID_Soldier); d; d=DefinitionMgrClass::Get_Next(d,CID_Soldier)) {
        int cost=0; if (AvailableCharacter(static_cast<SoldierGameObjDef*>(d),cost)) ++characters;
    }
    Console_Output("[Novara] DIAGNOSTIC terminal_objects=%u available_characters=%u\n",static_cast<unsigned>(terminals.size()),characters);
}
}
class NovaraStaticPickup : public ScriptImpClass {
    void Custom(GameObject* obj,int type,int,GameObject* sender) override {
        if (type == CUSTOM_EVENT_POWERUP_GRANTED) CoopServices::Granted(obj,sender);
    }
};
ScriptRegistrant<NovaraStaticPickup> novaraPickupRegistrant("Novara_Static_Pickup","");
class NovaraCoopTerminal : public ScriptImpClass {
    void Created(GameObject* obj) override { Commands->Enable_HUD_Pokable_Indicator(obj,true); }
    void Poked(GameObject*,GameObject* player) override {
        if (CoopServices::NearTerminal(player)) { CoopServices::ConfigurePurchases(); Display_GDI_Player_Terminal_Player(player); }
    }
};
ScriptRegistrant<NovaraCoopTerminal> novaraTerminalRegistrant("Novara_Coop_Terminal","");
