// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "da_player.h"
#include "admin_requests.h"
#include "coop_rules.h"
#include "admin_tools.h"
#include <random>

class NovaraAdmin : public DAEventClass {
    AdminRequests requests;
    std::map<int,DWORD> lastSpawn;
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
        Register_Chat_Command((DAECC)&NovaraAdmin::Credits,"!givecredits|!credits",0,DAAccessLevel::ADMINISTRATOR);
        Register_Chat_Command((DAECC)&NovaraAdmin::Vehicles,"!vehicles",0,DAAccessLevel::ADMINISTRATOR);
        Register_Chat_Command((DAECC)&NovaraAdmin::Spawn,"!spawnvehicle|!vehicle",0,DAAccessLevel::ADMINISTRATOR);
        Register_Chat_Command((DAECC)&NovaraAdmin::Map,"!map|!changemap",0,DAAccessLevel::ADMINISTRATOR);
        Register_Chat_Command((DAECC)&NovaraAdmin::Weapons,"!weapons",0,DAAccessLevel::ADMINISTRATOR);
        Register_Chat_Command((DAECC)&NovaraAdmin::GiveWeapon,"!giveweapon",0,DAAccessLevel::ADMINISTRATOR);
        Register_Chat_Command((DAECC)&NovaraAdmin::GiveAmmo,"!giveammo",0,DAAccessLevel::ADMINISTRATOR);
    }
    void Player_Join_Event(cPlayer* player) override { requests.disconnect(player->Get_Id()); lastSpawn.erase(player->Get_Id()); }
    void Player_Leave_Event(cPlayer* player) override { requests.disconnect(player->Get_Id()); lastSpawn.erase(player->Get_Id()); }
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
        DA::Page_Player(player,"Admin: !givecredits <player/ID|me|all> <amount>, !vehicles [filter] [page], !spawnvehicle <ID|preset>, !map <name>.");
        DA::Page_Player(player,"Weapons: !weapons [filter] [page], !giveweapon <player/ID|me|all> <ID|preset>, !giveammo <player/ID|me|all>.");
        DA::Page_Player(player,"Also: !nextmap, !restartmap, !kick <player ID>. Reconnect: !admin, then approve in the panel."); return false;
    }
    std::vector<cPlayer*> WeaponTargets(cPlayer* actor,const StringClass& name) {
        std::vector<cPlayer*> targets;
        const auto who=TextRules::Lower(name.Peek_Buffer());
        if (who=="all") { for (int i=1;i<=127;++i) if (auto* p=Find_Player(i)) if (p->Is_Active()) targets.push_back(p); }
        else if (who=="me") targets.push_back(actor);
        else if (auto* p=Match_Player(actor,name,true,false)) if (p->Is_Active()) targets.push_back(p);
        return targets;
    }
    bool Weapons(cPlayer* player,const DATokenClass& text,TextMessageEnum) {
        int page=1; std::string filter;
        if (text.Size()>2) { DA::Page_Player(player,"Use !weapons [filter] [page]."); return false; }
        if (text.Size() && !TextRules::Number(text[1].Peek_Buffer(),page)) filter=TextRules::Lower(text[1].Peek_Buffer());
        if (text.Size()==2 && !TextRules::Number(text[2].Peek_Buffer(),page)) { DA::Page_Player(player,"Page must be a positive number."); return false; }
        std::vector<unsigned> matches;
        for (unsigned i=0;i<AdminWeapons::catalog.size();++i)
            if (TextRules::Lower(AdminWeapons::catalog[i]->Get_Name()).find(filter)!=std::string::npos) matches.push_back(i);
        const int pages=(std::max)(1,(static_cast<int>(matches.size())+5)/6); page=(std::min)(page,pages);
        DA::Page_Player(player,"Weapons: %u matches, page %d/%d. !giveweapon <player|me|all> <ID|preset>.",static_cast<unsigned>(matches.size()),page,pages);
        for (unsigned n=(page-1)*6;n<matches.size() && n<static_cast<unsigned>(page*6);++n)
            DA::Page_Player(player,"%u: %s",matches[n]+1,AdminWeapons::catalog[matches[n]]->Get_Name());
        return false;
    }
    bool GiveWeapon(cPlayer* player,const DATokenClass& text,TextMessageEnum) {
        if (text.Size()!=2) { DA::Page_Player(player,"Use !giveweapon <player/ID|me|all> <ID|exact preset>. Browse with !weapons."); return false; }
        const WeaponDefinitionClass* weapon=nullptr; int index=0;
        if (TextRules::Number(text[2].Peek_Buffer(),index,static_cast<int>(AdminWeapons::catalog.size()))) weapon=AdminWeapons::catalog[index-1];
        else for (const auto* w : AdminWeapons::catalog) if (!_stricmp(w->Get_Name(),text[2].Peek_Buffer())) weapon=w;
        if (!weapon) { DA::Page_Player(player,"Unknown portable weapon. Use !weapons [filter] for exact IDs/presets."); return false; }
        unsigned count=0,skipped=0;
        for (auto* p : WeaponTargets(player,text[1])) {
            auto* soldier=p->Get_GameObj();
            if (!soldier || Commands->Get_Health(soldier)<=0 || !AdminWeapons::Give(soldier,weapon)) { ++skipped; continue; }
            ++count;
            DA::Page_Player(p,"Administrator gave you %s with full ammunition.",weapon->Get_Name());
            Console_Output("[Novara] Admin player=%d gave weapon=%s recipient=%d\n",player->Get_Id(),weapon->Get_Name(),p->Get_Id());
        }
        DA::Page_Player(player,"Weapon granted to %u player(s); %u without a living soldier skipped.",count,skipped); return false;
    }
    bool GiveAmmo(cPlayer* player,const DATokenClass& text,TextMessageEnum) {
        if (text.Size()!=1) { DA::Page_Player(player,"Use !giveammo <player/ID|me|all>. Refills carried weapons without healing."); return false; }
        unsigned count=0,skipped=0;
        for (auto* p : WeaponTargets(player,text[1])) {
            auto* soldier=p->Get_GameObj();
            if (!soldier || Commands->Get_Health(soldier)<=0) { ++skipped; continue; }
            const unsigned weapons=AdminWeapons::Refill(soldier); ++count;
            DA::Page_Player(p,"Administrator refilled ammunition for %u carried weapon(s).",weapons);
            Console_Output("[Novara] Admin player=%d refilled weapons=%u recipient=%d\n",player->Get_Id(),weapons,p->Get_Id());
        }
        DA::Page_Player(player,"Ammunition refilled for %u player(s); %u without a living soldier skipped.",count,skipped); return false;
    }
    bool Credits(cPlayer* player,const DATokenClass& text,TextMessageEnum) {
        int amount=0;
        if (text.Size()!=2 || !CoopRules::Credits(text[2].Peek_Buffer(),amount)) {
            DA::Page_Player(player,"Use !givecredits <player/ID|me|all> <whole credits, 1-1000000>."); return false;
        }
        std::vector<cPlayer*> targets;
        const auto who=TextRules::Lower(text[1].Peek_Buffer());
        if (who=="all") { for (int id=1; id<=127; ++id) { auto* p=Find_Player(id); if (p && p->Is_Active()) targets.push_back(p); } }
        else if (who=="me") targets.push_back(player);
        else if (auto* p=Match_Player(player,text[1],true,false)) if (p->Is_Active()) targets.push_back(p);
        if (targets.empty()) return false;
        for (auto* p : targets) {
            const auto balance=Get_Money(p->Get_Id());
            if (!std::isfinite(balance) || balance<0 || balance>16000000-amount) {
                DA::Page_Player(player,"Credit grant refused: an account would exceed the safe balance limit."); return false;
            }
        }
        for (auto* p : targets) {
            Set_Money(p->Get_Id(),Get_Money(p->Get_Id())+amount);
            Console_Output("[Novara] Admin player=%d granted credits=%d recipient=%d\n",player->Get_Id(),amount,p->Get_Id());
            DA::Page_Player(p,"Administrator granted you %d credits.",amount);
        }
        DA::Page_Player(player,"Granted %d credits to %u player(s).",amount,static_cast<unsigned>(targets.size())); return false;
    }
    bool Vehicles(cPlayer* player,const DATokenClass& text,TextMessageEnum) {
        int page=1; std::string filter;
        if (text.Size()>2) { DA::Page_Player(player,"Use !vehicles [filter] [page]."); return false; }
        if (text.Size() && !TextRules::Number(text[1].Peek_Buffer(),page)) filter=TextRules::Lower(text[1].Peek_Buffer());
        if (text.Size()==2 && !TextRules::Number(text[2].Peek_Buffer(),page)) { DA::Page_Player(player,"Page must be a positive number."); return false; }
        std::vector<unsigned> matches;
        for (unsigned i=0; i<AdminTools::vehicles.size(); ++i)
            if (TextRules::Lower(AdminTools::vehicles[i].preset).find(filter)!=std::string::npos) matches.push_back(i);
        const int pages=(std::max)(1,(static_cast<int>(matches.size())+5)/6); page=(std::min)(page,pages);
        DA::Page_Player(player,"Driveable vehicles: %u matches, page %d/%d. !vehicles [filter] [page]; !spawnvehicle <ID|preset>.",static_cast<unsigned>(matches.size()),page,pages);
        for (unsigned n=(page-1)*6; n<matches.size() && n<static_cast<unsigned>(page*6); ++n) {
            const auto i=matches[n]; const auto& v=AdminTools::vehicles[i];
            DA::Page_Player(player,"%u: %s (%d seats%s)",i+1,v.preset.c_str(),v.seats,v.flying ? ", aircraft" : "");
        }
        return false;
    }
    bool Spawn(cPlayer* player,const DATokenClass& text,TextMessageEnum) {
        if (text.Size()!=1) { DA::Page_Player(player,"Use !spawnvehicle <ID|exact preset>. Browse with !vehicles [filter]."); return false; }
        auto* actor=player->Get_GameObj();
        if (!actor || Commands->Get_Health(actor)<=0 || Get_Vehicle(actor) || !actor->Is_Control_Enabled()) {
            DA::Page_Player(player,"Stand alive on foot in an open area before spawning a vehicle."); return false;
        }
        const AdminTools::Vehicle* selected=nullptr; int index=0;
        if (TextRules::Number(text[1].Peek_Buffer(),index,static_cast<int>(AdminTools::vehicles.size()))) selected=&AdminTools::vehicles[index-1];
        else for (const auto& v : AdminTools::vehicles) if (!_stricmp(v.preset.c_str(),text[1].Peek_Buffer())) selected=&v;
        if (!selected) { DA::Page_Player(player,"Unknown driveable vehicle. Use !vehicles to find its ID or exact preset."); return false; }
        for (auto it=AdminTools::spawned.begin(); it!=AdminTools::spawned.end();) {
            if (!Commands->Find_Object(*it)) it=AdminTools::spawned.erase(it); else ++it;
        }
        const int cap=(std::max)(1,(std::min)(100,DASettingsManager::Get_Int("CampaignAdminVehicleLimit",20)));
        if (AdminTools::spawned.size()>=static_cast<unsigned>(cap) ||
            (lastSpawn.count(player->Get_Id()) && GetTickCount()-lastSpawn[player->Get_Id()]<5000)) {
            DA::Page_Player(player,"Vehicle spawn limit reached or five-second cooldown active (limit %d per map).",cap); return false;
        }
        const float facing=Commands->Get_Facing(actor),radians=facing*3.14159265f/180;
        Vector3 position=Commands->Get_Position(actor)+Vector3(cosf(radians)*10,sinf(radians)*10,selected->flying ? 3.0f : 1.0f);
        auto* vehicle=AdminTools::Create(*selected,position,facing);
        if (!vehicle) { DA::Page_Player(player,"No safe vehicle spawn nearby. Move to a larger open area."); return false; }
        AdminTools::spawned.insert(Commands->Get_ID(vehicle)); lastSpawn[player->Get_Id()]=GetTickCount();
        Console_Output("[Novara] Admin player=%d spawned vehicle=%s object=%d\n",player->Get_Id(),selected->preset.c_str(),Commands->Get_ID(vehicle));
        DA::Page_Player(player,"Spawned %s for the coop team.",selected->preset.c_str()); return false;
    }
    bool Map(cPlayer* player,const DATokenClass& text,TextMessageEnum) {
        if (text.Size()!=1) {
            DA::Page_Player(player,"Use !map <name>, e.g. !map M01 or !map M06. Available rotation:");
            for (int i=0; i<Get_Map_Count(); ++i) DA::Page_Player(player,"%s",Get_Map(i));
            return false;
        }
        if (AdminTools::ChangeMap(text[1].Peek_Buffer())) {
            Console_Output("[Novara] Admin player=%d requested map=%s\n",player->Get_Id(),text[1].Peek_Buffer());
        } else DA::Page_Player(player,"Map unavailable. Use !map to list the installed rotation.");
        return false;
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
