// SPDX-License-Identifier: GPL-3.0-or-later
// Coop replacements for the corresponding EA mission triggers. Event IDs,
// assets and waypoints follow the original sources cited in mission_controllers.h.
#pragma once

class CoopWarRoom : public ScriptImpClass {
    bool used = false, completed = false;
    void Created(GameObject* obj) override { Commands->Enable_HUD_Pokable_Indicator(obj,true); }
    void Complete(GameObject* obj) {
        if (completed) return;
        completed = true;
        Campaign::Send(obj,100018,601,1);
        Campaign::Send(obj,100018,603,3,1.0f);
    }
    void Poked(GameObject* obj,GameObject* player) override {
        if (used || !Campaign::Human(player)) return;
        used = true;
        Commands->Enable_HUD_Pokable_Indicator(obj,false);
        Commands->Create_Object("Level_03_Keycard",Vector3(35.263f,16.432f,13.382f));
        const int conv = Commands->Create_Conversation("M06_CON001",100,200,false);
        Commands->Join_Conversation(nullptr,conv,true,true,true);
        Commands->Join_Conversation(player,conv,true,true,true);
        Commands->Start_Conversation(conv,300603);
        Commands->Monitor_Conversation(obj,conv);
        Commands->Start_Timer(obj,this,30.0f,1);
    }
    void Action_Complete(GameObject* obj,int id,ActionCompleteReason reason) override {
        if (id == 300603 && (reason == ACTION_COMPLETE_CONVERSATION_ENDED || reason == ACTION_COMPLETE_CONVERSATION_INTERRUPTED)) Complete(obj);
    }
    void Timer_Expired(GameObject* obj,int) override { Complete(obj); }
};
ScriptRegistrant<CoopWarRoom> regWarRoom("M06_WarRoom_Computer","");

class CoopM09Havoc : public ScriptImpClass {
    // Keys belong to Campaign::state and are reapplied on every respawn.
    // A player death must not send the single-player death event to the mission.
};
ScriptRegistrant<CoopM09Havoc> regM09Havoc("M09_Havoc_Script","");

class CoopMobiusSuit : public ScriptImpClass {
    bool completed = false;
    void Created(GameObject* obj) override { Commands->Start_Timer(obj,this,.5f,1); }
    void Timer_Expired(GameObject* obj,int) override {
        if (completed) return;
        auto* mobius = Commands->Find_Object(2000010);
        if (mobius && Commands->Get_Health(mobius) > 0 && Campaign::HumanInside(obj) &&
            Commands->Get_Distance(Commands->Get_Position(mobius),Commands->Get_Position(obj)) < 12.0f &&
            Campaign::state.objectives.count(901)) {
            completed = true;
            // Apply the original suit rewards without the single-player camera
            // sequence, which moves one STAR out of the level and locks input.
            Commands->Set_Model(mobius,"c_ag_gdi_pmob");
            Commands->Give_PowerUp(mobius,"POW_LaserChaingun_AI",false);
            Commands->Select_Weapon(mobius,"Weapon_LaserChaingun_Ai");
            Set_Max_Health(mobius,300.0f);
            Commands->Set_Health(mobius,300.0f);
            Attach_Script_Once(mobius,"M09_Damage_Modifier",".05");
            Campaign::Send(obj,2000071,901,1);
            Campaign::Send(obj,2000071,902,3);
            DA::Host_Message("Mobius has his power suit. Escort him to the surface.");
        } else Commands->Start_Timer(obj,this,.5f,1);
    }
};
ScriptRegistrant<CoopMobiusSuit> regMobiusSuit("M09_Mobius_Suit_Objective","");

class CoopMobiusSurface : public ScriptImpClass {
    bool completed = false;
    void Created(GameObject* obj) override { Commands->Start_Timer(obj,this,.5f,1); }
    void Timer_Expired(GameObject* obj,int) override {
        if (completed) return;
        auto* mobius = Commands->Find_Object(2000010);
        if (Campaign::state.done(901) && mobius && Commands->Get_Health(mobius) > 0 && Campaign::HumanInside(obj) &&
            (Campaign::Inside(obj,mobius) || Commands->Get_Distance(Commands->Get_Position(mobius),Commands->Get_Position(obj)) < 12.0f)) {
            completed = true;
            Campaign::Send(obj,2000071,902,1);
            Campaign::Send(obj,2000071,903,3);
            Campaign::Send(obj,2000955,40050,0);
            for (int id : {2000614,2000954}) if (auto* block = Commands->Find_Object(id)) Commands->Destroy_Object(block);
        } else Commands->Start_Timer(obj,this,.5f,1);
    }
};
ScriptRegistrant<CoopMobiusSurface> regMobiusSurface("M09_Surface_Objective","");

class CoopMobiusEvac : public ScriptImpClass {
    bool completed = false;
    int occupantsTicks = 0;
    void Created(GameObject* obj) override { Commands->Start_Timer(obj,this,.5f,1); }
    void Entered(GameObject* obj,GameObject* player) override {
        if (Campaign::Human(player) && Campaign::state.done(902)) Campaign::Send(obj,2000010,40022,2000969);
    }
    void Timer_Expired(GameObject* obj,int) override {
        if (completed) return;
        auto* mobius = Commands->Find_Object(2000010);
        const bool present = Campaign::state.done(901) && Campaign::state.done(902) && mobius &&
            Commands->Get_Health(mobius) > 0 && Campaign::Inside(obj,mobius) && Campaign::HumanInside(obj);
        // Recompute occupancy, including death/disconnect, rather than keeping
        // one STAR flag which another player's exit could incorrectly clear.
        occupantsTicks = present ? occupantsTicks + 1 : 0;
        if (occupantsTicks >= 2) {
            completed = true;
            Campaign::StatusChanged(903,Campaign::Accomplished);
            Campaign::StatusChanged(900,Campaign::Accomplished);
            Campaign::Finish(true);
        } else Commands->Start_Timer(obj,this,.5f,1);
    }
};
ScriptRegistrant<CoopMobiusEvac> regMobiusEvac("M09_Evac_Point_Objective","");
