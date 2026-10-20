// SPDX-License-Identifier: GPL-3.0-or-later
// MPF_Player compatibility derived from MPF Games CoopBeta 4.2.4 (2012-2016).
// Built against the unmodified Dragonade 1.11.1 / TT 4.8.4 headers.
#include <functional>
#include "General.h"
#include "engine.h"
#include "engine_da.h"
#include "da.h"
#include "da_game.h"
#include "da_settings.h"
#include "SoldierGameObj.h"
#include "GameObjManager.h"
#include "ScriptZoneGameObj.h"
#include "SimpleGameObj.h"
#include "SpawnerClass.h"
#include "SpawnerDefClass.h"
#include "mission_runtime.h"
#include "mission_controllers.h"
#include "mission_coop.h"
#include "coop_services.h"
#include "campaign_admin.h"
#include "cosmetics.h"
#include "coop_features.h"
#include "brenbot_bridge.h"
static CoopServices::Events* coopServices = nullptr;
static NovaraAdmin* campaignAdmin = nullptr;
static CoopFeatures::Events* coopFeatures = nullptr;
static BrenBot::Events* brenbot = nullptr;

static void AuditMap() {
    if (!DASettingsManager::Get_Bool("CampaignDiagnostics",false) || The_Game()->Get_Current_Players()!=0) return;
    FILE* out=fopen("map-audit.tsv","wb"); if (!out) return;
    for (auto* n=GameObjManager::GameObjList.Head(); n; n=n->Next()) {
        auto* obj=n->Data()->As_ScriptableGameObj(); if (!obj) continue;
        const auto pos=Commands->Get_Position(obj);
        fprintf(out,"object\t%d\t%s\t%.3f\t%.3f\t%.3f",Commands->Get_ID(obj),Commands->Get_Preset_Name(obj),pos.X,pos.Y,pos.Z);
        if (auto* physical=obj->As_PhysicalGameObj()) {
            fprintf(out,"\tmodel=%s\tfacing=%.3f",Get_Model(physical),Commands->Get_Facing(obj));
            if (auto* model=physical->Peek_Physical_Object()->Peek_Model()) {
                AABoxClass box; model->Get_Obj_Space_Bounding_Box(box);
                fprintf(out,"\tbounds=%.3f,%.3f,%.3f;%.3f,%.3f,%.3f",box.Center.X,box.Center.Y,box.Center.Z,box.Extent.X,box.Extent.Y,box.Extent.Z);
            }
        }
        if (auto* zone=obj->As_ScriptZoneGameObj()) {
            const auto& box=zone->Get_Bounding_Box();
            fprintf(out,"\tzone=%s\textent=%.3f,%.3f,%.3f",zone->Get_Definition().ModelName.Peek_Buffer(),box.Extent.X,box.Extent.Y,box.Extent.Z);
        }
        fputs("\n",out);
        const auto& observers=obj->Get_Observers();
        for (int i=0; i<observers.Count(); ++i) {
            const char* name=observers[i]->Get_Name(); char params[4096]={0};
            // DA observers share this list but are not ScriptImpClass instances.
            // Find_Script_On_Object performs an unchecked cast.
            if (ScriptRegistrar::GetScriptFactory(name))
                if (auto* script=Find_Script_On_Object(obj,name)) script->Get_Parameters_String(params,sizeof(params));
            fprintf(out,"script\t%d\t%s\t%s\n",Commands->Get_ID(obj),name,params);
        }
    }
    fclose(out); Console_Output("[Novara] Map audit written: %s\n",The_Game()->Get_Map_Name());
}

static void DumpMissionObjects() {
    FILE* out = fopen("mission-objects.tsv", "wb");
    if (!out) return;
    fputs("id\tpreset\tx\ty\tz\thealth\tteam\tscripts\n", out);
    unsigned count = 0;
    for (auto* node = GameObjManager::GameObjList.Head(); node; node = node->Next()) {
        auto* obj = node->Data()->As_ScriptableGameObj();
        if (!obj) continue;
        const Vector3 p = Commands->Get_Position(obj);
        fprintf(out, "%d\t%s\t%.3f\t%.3f\t%.3f\t%.1f\t%d\t", Commands->Get_ID(obj),
                Commands->Get_Preset_Name(obj), p.X, p.Y, p.Z,
                Commands->Get_Health(obj), Commands->Get_Player_Type(obj));
        const auto& observers = obj->Get_Observers();
        for (int i = 0; i < observers.Count(); ++i) {
            fprintf(out, "%s%s", i ? ";" : "", observers[i]->Get_Name());
        }
        fputs("\n", out);
        if (Campaign::Human(obj)) {
            auto* defense = obj->As_SoldierGameObj()->Get_Defense_Object();
            Console_Output("[Campaign] player=%d skin=%s armor=%s hp=%.1f shield=%.1f can_die=%d\n",
                Get_Player_ID(obj), ArmorWarheadManager::Get_Armor_Name(defense->Get_Skin()),
                ArmorWarheadManager::Get_Armor_Name(defense->Get_Shield_Type()),
                defense->Get_Health(), defense->Get_Shield_Strength(), defense->Get_Can_Object_Die());
        }
        ++count;
    }
    fclose(out);
    // TT 4.8.4 FDS: identical address to Dragonade engine_da.cpp SpawnerList.
    // The installer pins and hashes the executable; never use for a client.
    const auto& spawners = *reinterpret_cast<DynamicVectorClass<SpawnerClass*>*>(0x008564A8);
    out = fopen("mission-spawners.tsv", "wb");
    if (out) {
        fputs("id\tpreset\tx\ty\tz\tprimary\tstartup\tenabled\tcount\tscripts\n", out);
        for (int i = 0; i < spawners.Count(); ++i) {
            const auto* s = spawners[i];
            const auto* d = s->definition;
            const Vector3 p = s->transform.Get_Translation();
            fprintf(out, "%d\t%s\t%.3f\t%.3f\t%.3f\t%d\t%d\t%d\t%d\t", s->id,
                    d->Get_Name(), p.X, p.Y, p.Z, d->IsPrimary, d->IsSoldierStartup, s->enabled, s->spawnCount);
            for (int j = 0; j < s->scriptNames.Count(); ++j)
                fprintf(out, "%s%s", j ? ";" : "", s->scriptNames[j].Peek_Buffer());
            fputs("\n", out);
        }
        fclose(out);
    }
    Console_Output("[Campaign] Wrote %u mission objects to mission-objects.tsv.\n", count);
    Console_Output("[Campaign] state mission=%d ready=%d checkpoint=%d keys=%u outcome=%d players=%u\n",Campaign::state.mission,Campaign::ready,Campaign::state.checkpoint,Campaign::state.keys,static_cast<int>(Campaign::state.outcome),static_cast<unsigned>(Campaign::Players().size()));
    for (const auto& e : Campaign::state.objectives) Console_Output("[Campaign] state objective=%d status=%d\n",e.first,e.second.status);
}

// This mode deliberately does not call DALevelCleaner::Clean_Level(): the
// campaign needs its single-player spawners, buildings and script observers.
class NovaraCampaign : public DAGameModeClass, public DAEventClass {
public:
    void Init() override {
        Register_Event(DAEvent::TEAMCHANGEREQUEST, INT_MAX);
        Register_Event(DAEvent::PLAYERLOADED);
        Register_Object_Event(DAObjectEvent::CREATED, DAObjectEvent::PLAYER);
        Console_Output("[Novara] Campaign mode active; mission scripts preserved.\n");
    }

    bool Team_Change_Request_Event(cPlayer*) override { return false; }

    void Object_Created_Event(GameObject* obj) override {
        // Keep this legacy script name because CoopBeta maps refer to it.
        Attach_Script_Once(obj, "MPF_Player", "");
    }

    void Player_Loaded_Event(cPlayer* player) override {
        DA::Private_Host_Message(player, "Novara campaign coop. Public test; mission validation is pending. Commands: !help. Supply terminals: !buy.");
    }
};
Register_Game_Mode(NovaraCampaign, "Campaign", "Renegade Campaign Coop", 0);

class MPF_Player : public ScriptImpClass {
    void Created(GameObject* obj) override {
        const int team = DASettingsManager::Get_Int("ForceTeam", 1);
        if (Get_Object_Type(obj) != team) { Change_Team(obj, team); return; }
        Commands->Start_Timer(obj,this,.25f,1);
    }
    void Timer_Expired(GameObject* obj,int) override {
        if (!Campaign::PlacePlayer(obj)) Commands->Start_Timer(obj,this,1.0f,1);
        else CoopFeatures::PlaceLegacyRespawn(obj);
    }
};
ScriptRegistrant<MPF_Player> playerRegistrant("MPF_Player", "");

class MPF_Player_Cant_Use_Goto : public ScriptImpClass {};
ScriptRegistrant<MPF_Player_Cant_Use_Goto> noGotoRegistrant("MPF_Player_Cant_Use_Goto", "");

static void TestPlayerArmor() {
    if (!DASettingsManager::Get_Bool("CampaignDiagnostics",false) ||
        !Campaign::state.active() || !Campaign::ready || Campaign::HasPlayers()) {
        Console_Output("[Campaign] Armor probe requires diagnostics, a ready campaign mission and no human players.\n");
        return;
    }
    // Synthetic actor on the isolated test server; never damages a player.
    auto* obj = Commands->Create_Object(Campaign::character.c_str(),Vector3(10000,10000,10000));
    if (!obj || !obj->As_SoldierGameObj()) return;
    const int id = Commands->Get_ID(obj);
    auto* defense = obj->As_SoldierGameObj()->Get_Defense_Object();
    const float before = defense->Get_Health() + defense->Get_Shield_Strength();
    Commands->Apply_Damage(obj,50,"Steel",nullptr);
    const float legacy = defense->Get_Health() + defense->Get_Shield_Strength();
    Campaign::RestoreCampaignPlayerArmor(obj->As_SoldierGameObj());
    const bool preserved = legacy == defense->Get_Health() + defense->Get_Shield_Strength();
    Console_Output("[Campaign] DIAGNOSTIC armor before=%.1f legacy_after_hit=%.1f preserved=%d skin=%s armor=%s\n",
        before,legacy,preserved,ArmorWarheadManager::Get_Armor_Name(defense->Get_Skin()),
        ArmorWarheadManager::Get_Armor_Name(defense->Get_Shield_Type()));
    Commands->Apply_Damage(obj,50,"Steel",nullptr);
    const float fixed = defense->Get_Health() + defense->Get_Shield_Strength();
    Commands->Apply_Damage(obj,5000,"Steel",nullptr);
    auto* remaining = Commands->Find_Object(id);
    const bool dead = !remaining || Commands->Get_Health(remaining) <= 0;
    Console_Output("[Campaign] DIAGNOSTIC armor fixed_after_hit=%.1f lethal=%d result=%s; synthetic actor, no human playtest.\n",
        fixed,dead,(legacy == before && preserved && fixed < legacy && dead) ? "PASS" : "FAIL");
    if (remaining) Commands->Destroy_Object(remaining);
}

// Panel console transport stays inside the persistent volume. This avoids
// Wine console-input incompatibilities and a network-accessible admin socket.
class PanelConsole : public DAEventClass {
    long inputOffset;
    DWORD lastPoll;
public:
    PanelConsole() : inputOffset(0), lastPoll(0) {
        FILE* in = fopen("console.in", "wb");
        if (in) fclose(in);
        Register_Event(DAEvent::CONSOLEOUTPUT);
        Register_Event(DAEvent::LEVELLOADED);
        Register_Event(DAEvent::THINK);
    }
    void Console_Output_Event(const char* output) override {
        FILE* log = fopen("panel-console.log", "ab");
        if (log) { fputs(output, log); fclose(log); }
    }
    void Level_Loaded_Event() override {
        Campaign::Loaded();
        CoopServices::Loaded();
        CoopFeatures::Loaded();
        Cosmetics::Build();
        AdminTools::Loaded();
        BrenBot::Loaded();
        const char* map = The_Game()->Get_Map_Name();
        FILE* state = fopen("active-map.tmp", "wb");
        if (state) { fputs(map, state); fclose(state);
            MoveFileExA("active-map.tmp", "active-map.txt", MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH); }
    }
    void Think() override {
        // DA timers scale with gameplay time; the Tutorial pauses that clock.
        const DWORD now = GetTickCount();
        if (now - lastPoll < 250) return;
        const float elapsed = lastPoll ? (now-lastPoll)/1000.0f : .25f;
        lastPoll = now;
        Campaign::Tick((std::min)(elapsed,1.0f));
        CoopServices::Tick();
        CoopFeatures::Tick();
        AdminTools::Tick();
        BrenBot::Tick();
        FILE* in = fopen("console.in", "rb");
        if (!in) return;
        fseek(in, inputOffset, SEEK_SET);
        char line[1024];
        while (fgets(line, sizeof(line), in)) {
            // Wait for a complete command when a writer is mid-append.
            char* end = strchr(line, '\n');
            if (!end) break;
            inputOffset = ftell(in);
            *end = '\0';
            if (end > line && end[-1] == '\r') end[-1] = '\0';
            if (!_stricmp(line, "campaign_dump")) DumpMissionObjects();
            else if (!_stricmp(line,"campaign_supplies")) CoopServices::Dump();
            else if (!_stricmp(line,"campaign_features")) CoopFeatures::Dump();
            else if (!_stricmp(line,"campaign_test_features")) CoopFeatures::Probe();
            else if (!_stricmp(line,"campaign_catalog")) Cosmetics::Dump();
            else if (!_stricmp(line,"campaign_audit_map")) AuditMap();
            else if (!_stricmp(line,"campaign_test_vehicles")) AdminTools::Probe();
            else if (!_strnicmp(line,"campaign_test_map ",18) && DASettingsManager::Get_Bool("CampaignDiagnostics",false) && The_Game()->Get_Current_Players()==0) {
                Console_Output("[Novara] DIAGNOSTIC map request accepted=%d\n",AdminTools::ChangeMap(line+18));
            }
            else if (!_stricmp(line,"campaign_test_supplies")) CoopServices::Probe();
            else if (!_strnicmp(line,"campaign_admin ",15)) {
                if (!campaignAdmin->Approve(line+15)) Console_Output("[Novara] Admin request not found or expired. Use !admin in-game first.\n");
            }
            else if (!_stricmp(line,"campaign_test_player_armor")) TestPlayerArmor();
            else if (!_stricmp(line,"campaign_probe_spawners")) {
                if (DASettingsManager::Get_Bool("CampaignDiagnostics",false) && Campaign::ready) {
                    Campaign::RestoreActors();
                    Campaign::TickSpawners(1.0f);
                    Console_Output("[Campaign] DIAGNOSTIC spawner probe; no human playtest.\n");
                } else Console_Output("[Campaign] Diagnostic spawner probe disabled.\n");
            }
            else if (!_stricmp(line,"campaign_test_mobius_death")) {
                if (DASettingsManager::Get_Bool("CampaignDiagnostics",false) && Campaign::state.mission == 9) {
                    if (auto* mobius = Commands->Find_Object(2000010)) Commands->Apply_Damage(mobius,1000000.0f,"Death",nullptr);
                    Console_Output("[Campaign] DIAGNOSTIC Mobius death; no human playtest.\n");
                }
            }
            else Console_Input(line);
        }
        fclose(in);
    }
};
static PanelConsole* panelConsole = 0;
extern "C" __declspec(dllexport) void Plugin_Init() {
    Campaign::Install();
    coopServices = new CoopServices::Events;
    campaignAdmin = new NovaraAdmin;
    coopFeatures = new CoopFeatures::Events;
    brenbot = new BrenBot::Events;
    panelConsole = new PanelConsole;
    Console_Output("[Novara] da_campaign loaded (TT 4.8.4 / DA 1.11.1).\n");
}
extern "C" __declspec(dllexport) void Plugin_Shutdown() {
    Campaign::Uninstall();
    delete brenbot; brenbot = nullptr;
    delete coopFeatures; coopFeatures = nullptr;
    delete campaignAdmin; campaignAdmin = nullptr;
    delete coopServices; coopServices = nullptr;
    delete panelConsole;
    panelConsole = 0;
}
