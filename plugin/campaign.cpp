// SPDX-License-Identifier: GPL-2.0-or-later
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

// This mode deliberately does not call DALevelCleaner::Clean_Level(): the
// campaign needs its single-player spawners, buildings and script observers.
class WellischCampaign : public DAGameModeClass, public DAEventClass {
public:
    void Init() override {
        Register_Event(DAEvent::TEAMCHANGEREQUEST, INT_MAX);
        Register_Event(DAEvent::PLAYERLOADED);
        Register_Object_Event(DAObjectEvent::CREATED, DAObjectEvent::PLAYER);
        Console_Output("[Wellisch] Campaign mode active; mission scripts preserved.\n");
    }

    bool Team_Change_Request_Event(cPlayer*) override { return false; }

    void Object_Created_Event(GameObject* obj) override {
        // Keep this legacy script name because CoopBeta maps refer to it.
        Attach_Script_Once(obj, "MPF_Player", "");
    }

    void Player_Loaded_Event(cPlayer* player) override {
        DA::Private_Host_Message(player, "Wellisch campaign development server. Mission validation is pending.");
    }
};
Register_Game_Mode(WellischCampaign, "Campaign", "Renegade Campaign Coop", 0);

class MPF_Player : public ScriptImpClass {
    void Created(GameObject* obj) override {
        const int team = DASettingsManager::Get_Int("ForceTeam", 1);
        if (Get_Object_Type(obj) != team) Change_Team(obj, team);
    }
};
ScriptRegistrant<MPF_Player> playerRegistrant("MPF_Player", "");

class MPF_Player_Cant_Use_Goto : public ScriptImpClass {};
ScriptRegistrant<MPF_Player_Cant_Use_Goto> noGotoRegistrant("MPF_Player_Cant_Use_Goto", "");

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
        const char* map = The_Game()->Get_Map_Name();
        FILE* state = fopen("active-map.tmp", "wb");
        if (state) { fputs(map, state); fclose(state);
            MoveFileExA("active-map.tmp", "active-map.txt", MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH); }
    }
    void Think() override {
        // DA timers scale with gameplay time; the Tutorial pauses that clock.
        const DWORD now = GetTickCount();
        if (now - lastPoll < 250) return;
        lastPoll = now;
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
            Console_Input(line);
        }
        fclose(in);
    }
};
static PanelConsole* panelConsole = 0;
extern "C" __declspec(dllexport) void Plugin_Init() {
    panelConsole = new PanelConsole;
    Console_Output("[Wellisch] da_campaign loaded (TT 4.8.4 / DA 1.11.1).\n");
}
extern "C" __declspec(dllexport) void Plugin_Shutdown() {
    delete panelConsole;
    panelConsole = 0;
}
