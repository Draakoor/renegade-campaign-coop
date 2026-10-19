// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "da_player.h"
#include "admin_requests.h"
#include <random>

class NovaraAdmin : public DAEventClass {
    AdminRequests requests;
    bool Allowed(cPlayer* player) {
        StringClass name; DASettingsManager::Get_String(name,"CampaignAdminName","");
        StringClass current(player->Get_Name());
        return !name.Is_Empty() && !_stricmp(name.Peek_Buffer(),current.Peek_Buffer());
    }
public:
    NovaraAdmin() {
        Register_Event(DAEvent::PLAYERJOIN);
        Register_Event(DAEvent::PLAYERLEAVE);
        Register_Chat_Command((DAECC)&NovaraAdmin::RequestAccess,"!admin",0);
        Register_Chat_Command((DAECC)&NovaraAdmin::Help,"!adminhelp",0,DAAccessLevel::ADMINISTRATOR);
        Register_Chat_Command((DAECC)&NovaraAdmin::Next,"!nextmap",0,DAAccessLevel::ADMINISTRATOR);
        Register_Chat_Command((DAECC)&NovaraAdmin::Restart,"!restartmap",0,DAAccessLevel::ADMINISTRATOR);
        Register_Chat_Command((DAECC)&NovaraAdmin::Kick,"!kick",1,DAAccessLevel::ADMINISTRATOR);
    }
    void Player_Join_Event(cPlayer* player) override { requests.disconnect(player->Get_Id()); }
    void Player_Leave_Event(cPlayer* player) override { requests.disconnect(player->Get_Id()); }
    bool RequestAccess(cPlayer* player,const DATokenClass&,TextMessageEnum) {
        if (!Allowed(player)) { DA::Page_Player(player,"This name is not configured for panel-approved administration."); return false; }
        if (player->Get_DA_Player()->Get_Access_Level() == DAAccessLevel::ADMINISTRATOR) return Help(player,DATokenClass(),TEXT_MESSAGE_PRIVATE);
        try {
            std::random_device random;
            char code[33]; for (int i=0; i<4; ++i) sprintf(code+i*8,"%08x",static_cast<unsigned>(random()));
            requests.issue(player->Get_Id(),code,GetTickCount());
            DA::Page_Player(player,"Confirm in your authenticated panel console within 5 minutes: campaign_admin %s",code);
        } catch (...) { DA::Page_Player(player,"Could not generate an admin request. Try again later."); }
        return false;
    }
    bool Approve(const char* code) {
        const int id=requests.consume(code,GetTickCount());
        auto* player=id ? Find_Player(id) : nullptr;
        if (!player || !player->Is_Active() || !Allowed(player)) return false;
        player->Get_DA_Player()->Set_Access_Level(DAAccessLevel::ADMINISTRATOR);
        Console_Output("[Novara] Admin approved for player %d for this connection.\n",player->Get_Id());
        DA::Page_Player(player,"Novara admin access granted for this connection. Commands: !adminhelp");
        return true;
    }
    bool Help(cPlayer* player,const DATokenClass&,TextMessageEnum) {
        DA::Page_Player(player,"Admin: !nextmap, !restartmap, !kick <player ID>. Reconnect: !admin, then approve in the panel."); return false;
    }
    bool Next(cPlayer*,const DATokenClass&,TextMessageEnum) {
        Console_Output("[Novara] Admin requested next map.\n"); Console_Input("gameover"); return false;
    }
    bool Restart(cPlayer*,const DATokenClass&,TextMessageEnum) {
        Console_Output("[Novara] Admin requested mission restart.\n"); Reload_Level(); return false;
    }
    bool Kick(cPlayer* player,const DATokenClass& text,TextMessageEnum) {
        int id=0; const char* arg=text[1].Peek_Buffer(); char* end=nullptr;
        const long parsed=strtol(arg,&end,10);
        if (!*arg || *end || parsed < 1 || parsed > 127 || !(id=static_cast<int>(parsed)) || !Find_Player(id)) {
            DA::Page_Player(player,"Use !kick <connected player ID>."); return false;
        }
        char cmd[80]; sprintf(cmd,"kick %d Removed by server administrator",id); Console_Input(cmd); return false;
    }
};
