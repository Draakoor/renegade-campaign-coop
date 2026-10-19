// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mission_state.h"
#include "DefinitionMgrClass.h"
#include "MoveablePhysClass.h"
#include "ScriptZoneGameObj.h"
#include "ArmorWarheadManager.h"
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace Campaign {
static State state;
static bool ready = false, spawnValid = false, transitionIssued = false;
static Vector3 spawn(0,0,0);
static float facing = 0;
static DWORD outcomeTime = 0;
static std::string character;
static std::map<int, uint32_t> observedKeys;
static uint32_t revokedKeys = 0;
static std::map<int, std::string> purchasedCharacters;
static std::map<int, int> markers;
static std::map<int, Vector3> destinations;
static std::map<int,int> actorIds, actorOriginalIds;
static bool actorsRestored = false, actorLoadFailed = false;
static decltype(ScriptCommands::Find_Object) originalFind;
static decltype(ScriptCommands::Get_ID) originalId;
static decltype(ScriptCommands::Mission_Complete) originalFinish;
static decltype(ScriptCommands::Add_Objective) originalAdd;
static decltype(ScriptCommands::Set_Objective_Status) originalStatus;
static decltype(ScriptCommands::Set_Objective_Radar_Blip_Object) originalBlipObject;
static decltype(ScriptCommands::Set_Objective_Radar_Blip) originalBlip;
static decltype(ScriptCommands::Grant_Key) originalKey;
static decltype(ScriptCommands::Join_Conversation) originalJoin;

static bool Human(GameObject* obj) {
    return obj && obj->As_SoldierGameObj() && Commands->Is_A_Star(obj);
}
static std::vector<SoldierGameObj*> Players() {
    std::vector<SoldierGameObj*> result;
    for (auto* n = GameObjManager::StarGameObjList.Head(); n; n = n->Next()) {
        auto* p = n->Data()->As_SoldierGameObj();
        if (Human(p) && Commands->Get_Health(p) > 0) result.push_back(p);
    }
    return result;
}
static bool HasPlayers() { return !Players().empty(); }
static bool CoopMap() {
    const char* map = The_Game()->Get_Map_Name();
    if (!_stricmp(map,"M00_Tutorial.mix") || !_stricmp(map,"M00_Tutorial")) return true;
    for (int i=1; i<=11; ++i) {
        char name[20]; sprintf(name,"M%02d.mix",i);
        if (!_stricmp(map,name)) return true;
        name[3] = '\0'; if (!_stricmp(map,name)) return true;
    }
    return false;
}
static DynamicVectorClass<SpawnerClass*>& Spawners() {
    // Pinned TT 4.8.4 FDS, also used by Dragonade 1.11.1 engine_da.cpp.
    // prepare_bundle.py verifies the unpatched executable before the port patch.
    return *reinterpret_cast<DynamicVectorClass<SpawnerClass*>*>(0x008564A8);
}
static int MapNumber() {
    const char* name = The_Game()->Get_Map_Name();
    if (!_stricmp(name,"M06.mix") || !_stricmp(name,"M06")) return 6;
    if (!_stricmp(name,"M07.mix") || !_stricmp(name,"M07")) return 7;
    if (!_stricmp(name,"M09.mix") || !_stricmp(name,"M09")) return 9;
    return 0;
}
static void Begin(int number) {
    state = State(number);
    ready = spawnValid = transitionIssued = false;
    outcomeTime = 0;
    observedKeys.clear(); markers.clear(); destinations.clear(); character.clear();
    revokedKeys = 0; purchasedCharacters.clear();
    actorIds.clear(); actorOriginalIds.clear(); actorsRestored = actorLoadFailed = false;
    Console_Output("[Campaign] M%02d shared mission state reset.\n",number);
}
static GameObject* Find(int id) {
    auto it = actorIds.find(id);
    return originalFind(it == actorIds.end() ? id : it->second);
}
static int Id(GameObject* obj) {
    const int id = originalId(obj);
    auto it = actorOriginalIds.find(id);
    return it == actorOriginalIds.end() ? id : it->second;
}
struct Actor {
    uint32_t id, definition;
    float matrix[12];
    std::vector<std::pair<std::string,std::string>> scripts;
};
template<class T> static T Read(std::ifstream& in) {
    T value; in.read(reinterpret_cast<char*>(&value),sizeof(T));
    if (!in) throw std::runtime_error("Truncated actor table");
    return value;
}
static std::string ReadString(std::ifstream& in) {
    const auto size = Read<uint16_t>(in);
    if (size > 4095) throw std::runtime_error("Oversized actor script");
    std::string value(size,'\0');
    if (size) in.read(&value[0],size);
    if (!in || value.find('\0') != std::string::npos) throw std::runtime_error("Invalid actor script");
    return value;
}
static void RestoreActors() {
    if (actorsRestored) return;
    actorsRestored = true;
    try {
        char path[64]; sprintf(path,"data/M%02d.wcm",state.mission);
        std::ifstream in(path,std::ios::binary);
        if (!in || Read<uint32_t>(in) != 0x314d4357) throw std::runtime_error("Missing/invalid .wcm actor table; compile the original LVL source");
        const auto count = Read<uint32_t>(in);
        if (!count || count > 20000) throw std::runtime_error("Invalid actor count");
        std::vector<Actor> actors;
        for (uint32_t i=0; i<count; ++i) {
            Actor a; a.id = Read<uint32_t>(in); a.definition = Read<uint32_t>(in);
            for (auto& value : a.matrix) value = Read<float>(in);
            const auto scripts = Read<uint32_t>(in);
            if (scripts > 256) throw std::runtime_error("Invalid script count");
            for (uint32_t j=0; j<scripts; ++j) {
                auto name = ReadString(in); auto params = ReadString(in);
                a.scripts.emplace_back(name,params);
            }
            actors.push_back(a);
        }
        // Instantiate every missing soldier before attaching any instance script.
        // Script references retain original LVL IDs through the command bridge;
        // actual network IDs remain engine-assigned and unique for clients.
        unsigned restored = 0;
        for (const auto& a : actors) {
            if (originalFind(a.id)) continue;
            auto* def = DefinitionMgrClass::Find_Definition(a.definition,true);
            if (!def) { Console_Output("[Campaign] Missing actor definition %u (id %u)\n",a.definition,a.id); continue; }
            if (def->Get_Class_ID() != 0x3001) continue;
            float values[12]; memcpy(values,a.matrix,sizeof(values)); Matrix3D tm(values);
            auto* obj = Create_Object(def,tm);
            if (!obj) throw std::runtime_error("Actor creation failed");
            const int networkId = originalId(obj);
            actorIds[a.id] = networkId; actorOriginalIds[networkId] = a.id;
            ++restored;
        }
        for (const auto& a : actors) {
            if (!actorIds.count(a.id)) continue;
            auto* obj = Find(a.id);
            for (const auto& script : a.scripts) Attach_Script_Once(obj,script.first.c_str(),script.second.c_str());
        }
        Console_Output("[Campaign] Restored %u original actors from %s.\n",restored,path);
    } catch (const std::exception& e) {
        Console_Output("[Campaign] ERROR: %s. Mission stopped.\n",e.what());
        ready = false; actorLoadFailed = true;
    }
}
static void Show(GameObject* player, bool all = true) {
    if (!Human(player)) return;
    bool hud = false;
    for (const auto& entry : state.objectives) {
        const auto& o = entry.second;
        if (!o.announced || o.status == Hidden || !o.title) continue;
        if (all) {
            const char* title = Get_Translated_String(o.title);
            DA::Private_Host_Message(player,"[%s] %s",o.status == Accomplished ? "OK" : o.status == Failed ? "FAILED" : "TODO",title);
            delete[] title;
        }
        if (!hud && o.status == Pending && o.type == 1) {
            Set_HUD_Help_Text_Player(player,o.title,Vector3(.2f,.9f,.2f)); hud = true;
        }
    }
}
static void UpdateMarker(int id) {
    auto it = state.objectives.find(id);
    if (it == state.objectives.end()) return;
    GameObject* marker = Commands->Find_Object(markers[id]);
    if (it->second.status != Pending || !destinations.count(id)) {
        if (marker) Commands->Set_Obj_Radar_Blip_Shape(marker,RADAR_BLIP_SHAPE_NONE);
        return;
    }
    if (!marker) {
        marker = Commands->Create_Object("Daves Arrow",destinations[id]);
        if (!marker) return;
        Commands->Set_Model(marker,"null");
        Commands->Set_Is_Visible(marker,false);
        Commands->Set_Player_Type(marker,1);
        markers[id] = Commands->Get_ID(marker);
    }
    Commands->Set_Position(marker,destinations[id]);
    Commands->Set_Obj_Radar_Blip_Shape(marker,RADAR_BLIP_SHAPE_OBJECTIVE);
    Commands->Set_Obj_Radar_Blip_Color(marker,it->second.type == 1 ? RADAR_BLIP_COLOR_PRIMARY_OBJECTIVE : RADAR_BLIP_COLOR_SECONDARY_OBJECTIVE);
}
static void Checkpoint(int id) {
    int rank = 0;
    if (state.mission == 6) rank = id == 601 ? 1 : id == 603 ? 2 : id == 611 ? 3 : 0;
    if (state.mission == 7) rank = id == 710 ? 1 : id == 702 ? 2 : id == 701 ? 3 : 0;
    if (state.mission == 9) rank = id == 901 ? 1 : id == 902 ? 2 : 0;
    if (rank <= state.checkpoint) return;
    auto players = Players();
    if (players.empty()) return;
    auto* closest = players.front();
    if (destinations.count(id)) {
        const Vector3 dest = destinations[id];
        for (auto* p : players)
            if (Commands->Get_Distance(Commands->Get_Position(p),dest) < Commands->Get_Distance(Commands->Get_Position(closest),dest)) closest = p;
    }
    // A position actually occupied by a player is safer than a cinematic marker.
    spawn = Commands->Get_Position(closest); facing = Commands->Get_Facing(closest);
    spawnValid = true; state.checkpoint = rank;
    DA::Host_Message("Coop checkpoint %d reached. Respawns and late joins share this position.",rank);
    Console_Output("[Campaign] checkpoint=%d objective=%d position=%.2f,%.2f,%.2f\n",rank,id,spawn.X,spawn.Y,spawn.Z);
}
static void Add(int id,int type,int status,int title,char* sound,int description) {
    if (!state.active()) { originalAdd(id,type,status,title,sound,description); return; }
    if (!state.add(id,type,status,title,description)) return;
    Console_Output("[Campaign] objective=%d status=%d title=%d\n",id,state.objectives[id].status,title);
    UpdateMarker(id);
    if (status != Hidden) for (auto* p : Players()) Show(p);
}
static void StatusChanged(int id,int status) {
    if (!state.active()) { originalStatus(id,status); return; }
    if (!state.set(id,status)) return;
    Console_Output("[Campaign] objective=%d status=%d\n",id,status);
    if (status == Accomplished) Checkpoint(id);
    UpdateMarker(id);
    for (auto* p : Players()) Show(p);
}
static void Blip(int id,const Vector3& pos) {
    if (!state.active()) { originalBlip(id,pos); return; }
    destinations[id] = pos; UpdateMarker(id);
}
static void BlipObject(int id,GameObject* obj) {
    if (!state.active()) { originalBlipObject(id,obj); return; }
    if (obj) Blip(id,Commands->Get_Position(obj));
}
static void Finish(bool success) {
    if (!state.active()) { originalFinish(success); return; }
    state.finish(success);
    Console_Output("[Campaign] completion requested success=%d result=%d\n",success,static_cast<int>(state.outcome));
}
static void ApplyKeys(SoldierGameObj* p) {
    if (!DASettingsManager::Get_Bool("CampaignSharedKeys",true)) return;
    const uint32_t current = static_cast<uint32_t>(p->Get_Key_Ring());
    for (int i = 0; i < 32; ++i) {
        const auto bit = uint32_t(1) << i;
        if ((current & bit) != (state.keys & bit)) originalKey(p,i,(state.keys & bit) != 0);
    }
    observedKeys[Commands->Get_ID(p)] = state.keys;
}
static void Key(GameObject* obj,int key,bool grant) {
    if (!CoopMap() || !DASettingsManager::Get_Bool("CampaignSharedKeys",true) || !Human(obj)) { originalKey(obj,key,grant); return; }
    state.grant(key,grant);
    if (key >= 0 && key < 32) {
        if (grant) revokedKeys &= ~(uint32_t(1) << key);
        else revokedKeys |= uint32_t(1) << key;
    }
    for (auto* p : Players()) ApplyKeys(p);
}
static void TickSharedKeys() {
    if (!CoopMap() || !DASettingsManager::Get_Bool("CampaignSharedKeys",true)) return;
    const auto players = Players();
    for (auto* p : players) {
        const uint32_t current = static_cast<uint32_t>(p->Get_Key_Ring());
        state.keys |= current & ~observedKeys[Commands->Get_ID(p)] & ~revokedKeys;
    }
    for (auto* p : players) ApplyKeys(p);
}
static void Join(GameObject* obj,int conversation,bool move,bool turn,bool face) {
    // A narrative must never freeze a human player's controls in multiplayer.
    originalJoin(obj,conversation,state.active() && Human(obj) ? true : move,turn,face);
}
static void Install() {
    originalFind = Commands->Find_Object; Commands->Find_Object = Find;
    originalId = Commands->Get_ID; Commands->Get_ID = Id;
    originalFinish = Commands->Mission_Complete; Commands->Mission_Complete = Finish;
    originalAdd = Commands->Add_Objective; Commands->Add_Objective = Add;
    originalStatus = Commands->Set_Objective_Status; Commands->Set_Objective_Status = StatusChanged;
    originalBlip = Commands->Set_Objective_Radar_Blip; Commands->Set_Objective_Radar_Blip = Blip;
    originalBlipObject = Commands->Set_Objective_Radar_Blip_Object; Commands->Set_Objective_Radar_Blip_Object = BlipObject;
    originalKey = Commands->Grant_Key; Commands->Grant_Key = Key;
    originalJoin = Commands->Join_Conversation; Commands->Join_Conversation = Join;
}
static void Uninstall() {
    Commands->Find_Object = originalFind; Commands->Get_ID = originalId;
    Commands->Mission_Complete = originalFinish; Commands->Add_Objective = originalAdd;
    Commands->Set_Objective_Status = originalStatus; Commands->Set_Objective_Radar_Blip = originalBlip;
    Commands->Set_Objective_Radar_Blip_Object = originalBlipObject;
    Commands->Grant_Key = originalKey; Commands->Join_Conversation = originalJoin;
}
static void Loaded() {
    if (!MapNumber()) { Begin(0); return; }
    // FDS dispatches observer Created after DA's LEVELLOADED event.
    if (state.mission != MapNumber()) return;
    The_Game()->Set_Time_Limit_Minutes(0);
    The_Game()->Set_Is_Friendly_Fire_Permitted(false);
    The_Game()->Set_Is_Team_Changing_Allowed(false);
    The_Game()->Set_Remix_Teams(false);
    if (The_Cnc_Game()) {
        The_Cnc_Game()->Set_Base_Destruction_Ends_Game(false);
        The_Cnc_Game()->Set_Beacon_Placement_Ends_Game(false);
    }
    auto& spawners = Spawners();
    for (int i = 0; i < spawners.Count(); ++i) {
        auto* s = spawners[i];
        if (!s->definition->IsPrimary) continue;
        spawn = s->transform.Get_Translation(); spawnValid = true;
        const auto& ids = s->definition->SpawnDefinitionIDList;
        if (ids.Count()) {
            auto* d = DefinitionMgrClass::Find_Definition(ids[0],true);
            if (d) character = d->Get_Name();
        }
        break;
    }
    ready = spawnValid;
    if (state.mission == 9) state.grant(10,true);
    if (state.mission == 6) { state.grant(9,true); state.grant(10,true); }
    Console_Output("[Campaign] M%02d ready=%d spawners=%d character=%s spawn=%.2f,%.2f,%.2f\n",state.mission,ready,spawners.Count(),character.c_str(),spawn.X,spawn.Y,spawn.Z);
}
static void RestoreCampaignPlayerArmor(SoldierGameObj* obj) {
    // CoopBeta's shared presets make the SP Havoc variants invulnerable.
    // M06/M07 use Commando_Stealth and M09 uses Commando_Desert. Restore Blamo
    // types to the normal Commando types; keep model, equipment and HP intact.
    const char* expected = state.mission == 9 ? "Commando_Desert" : "Commando_Stealth";
    if (!state.active() || !obj || _stricmp(Commands->Get_Preset_Name(obj),expected)) return;
    auto* defense = obj->Get_Defense_Object();
    const auto blamo = ArmorWarheadManager::Get_Armor_Type("Blamo");
    if (defense->Get_Skin() == blamo) {
        defense->Set_Skin(ArmorWarheadManager::Get_Armor_Type("SkinFlesh"));
        defense->Mark_Owner_Dirty();
    }
    if (defense->Get_Shield_Type() == blamo)
        defense->Set_Shield_Type(ArmorWarheadManager::Get_Armor_Type("ShieldKevlar"));
}
static bool PlacePlayer(GameObject* obj) {
    if (!state.active() || !Human(obj)) return true;
    if (!ready) return false;
    // A PT purchase reinitializes the same soldier. Keep the purchased class
    // and position for this life; a new respawn gets a new object ID.
    if (purchasedCharacters.count(Commands->Get_ID(obj))) {
        ApplyKeys(obj->As_SoldierGameObj());
        if (state.mission == 6) Attach_Script_Once(obj,"M06_Havoc_DLS","");
        if (state.mission == 7) Attach_Script_Once(obj,"M07_Havoc_DLS","");
        return true;
    }
    auto* movable = obj->As_PhysicalGameObj()->Peek_Physical_Object()->As_MoveablePhysClass();
    Vector3 location;
    if (!movable || !movable->Find_Teleport_Location(spawn,8.0f,&location)) return false;
    Commands->Set_Position(obj,location); Commands->Set_Facing(obj,facing);
    // Change_Character reinitializes this soldier in place; it does not promise
    // a fresh object or another MPF_Player Created callback.
    if (!character.empty() && _stricmp(Commands->Get_Preset_Name(obj),character.c_str())) Change_Character(obj,character.c_str());
    RestoreCampaignPlayerArmor(obj->As_SoldierGameObj());
    ApplyKeys(obj->As_SoldierGameObj());
    if (state.mission == 6) Attach_Script_Once(obj,"M06_Havoc_DLS","");
    if (state.mission == 7) Attach_Script_Once(obj,"M07_Havoc_DLS","");
    Show(obj);
    Console_Output("[Campaign] player=%d checkpoint=%d spawned=%.2f,%.2f,%.2f\n",Get_Player_ID(obj),state.checkpoint,location.X,location.Y,location.Z);
    return true;
}
static void TickSpawners(float elapsed) {
    auto& spawners = Spawners();
    for (int i = 0; i < spawners.Count(); ++i) {
        auto* s = spawners[i]; const auto* d = s->definition;
        // FDS skips these SP soldier spawners. Other spawners remain engine-owned.
        if (d->IsPrimary || !d->IsSoldierStartup || !s->enabled || s->currentObject.Get_Ptr()) continue;
        if (d->SpawnMax >= 0 && s->spawnCount >= d->SpawnMax) continue;
        s->spawnTimeRemaining -= elapsed;
        if (s->spawnTimeRemaining <= 0) Commands->Trigger_Spawner(s->id);
    }
}
static void Tick(float elapsed) {
    if (!state.active() || actorLoadFailed) return;
    if (!ready) Loaded();
    if (!ready) return;
    if (state.outcome != Outcome::Running) {
        if (!outcomeTime) {
            outcomeTime = GetTickCount();
            DA::Host_Message(state.outcome == Outcome::Success ? "Mission complete. Moving to the next mission in 8 seconds." : "Mission failed. Restarting this mission in 8 seconds.");
        }
        if (!transitionIssued && GetTickCount() - outcomeTime >= 8000) {
            transitionIssued = true;
            if (state.outcome == Outcome::Failure) Reload_Level();
            else { The_Game()->Set_Winner_ID(1); Console_Input("gameover"); }
        }
        return;
    }
    const auto players = Players();
    if (players.empty()) return;
    RestoreActors();
    if (!ready) return;
    TickSpawners(elapsed);
}
static bool Inside(GameObject* zone,GameObject* obj) {
    return zone && obj && zone->As_ScriptZoneGameObj() && zone->As_ScriptZoneGameObj()->Point_In_Zone(Commands->Get_Position(obj));
}
static bool HumanInside(GameObject* zone) {
    for (auto* p : Players()) if (Inside(zone,p)) return true;
    return false;
}
static void Send(GameObject* from,int target,int type,int value,float delay = 0) {
    if (auto* to = Commands->Find_Object(target)) Commands->Send_Custom_Event(from,to,type,value,delay);
}
}
