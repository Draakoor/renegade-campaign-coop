// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2025 Electronic Arts Inc.
// Adapted from EA CnC_Renegade 3e00c3a1b97381bb28be89a35b856375e0629a08,
// Code/Scripts/Mission06.cpp, Mission07.cpp, Mission09.cpp.
// Coop changes 2026: deferred openings, interrupted-dialogue recovery,
// shared state, and multiplayer deaths. Save-game serialization removed.
// See COPYING.plugin and THIRD_PARTY.txt.
#pragma once
#define BLOCK_OFF 40078
#define BLOCK_ON 40077
#define CONVERSATION 40006
#define DEADEYE (Commands->Find_Object(100660))
#define ENDED 40049
#define FOLLOW 40022
#define GUNNER (Commands->Find_Object(100661))
#define HOTWIRE (Commands->Find_Object(100658))
#define IDS_Enc_ObjTitle_Hidden_M06_01 6207
#define IDS_Enc_ObjTitle_Hidden_M06_02 6208
#define IDS_Enc_ObjTitle_Hidden_M06_03 6209
#define IDS_Enc_ObjTitle_Hidden_M06_04 6210
#define IDS_Enc_ObjTitle_Hidden_M09_01 6213
#define IDS_Enc_ObjTitle_Primary_M06_01 6241
#define IDS_Enc_ObjTitle_Primary_M06_02 6242
#define IDS_Enc_ObjTitle_Primary_M06_03 6243
#define IDS_Enc_ObjTitle_Primary_M06_04 6244
#define IDS_Enc_ObjTitle_Primary_M07_01 6245
#define IDS_Enc_ObjTitle_Primary_M07_02 6246
#define IDS_Enc_ObjTitle_Primary_M07_03 6247
#define IDS_Enc_ObjTitle_Primary_M07_04 6248
#define IDS_Enc_ObjTitle_Primary_M07_05 7593
#define IDS_Enc_ObjTitle_Primary_M09_01 6253
#define IDS_Enc_ObjTitle_Primary_M09_02 6254
#define IDS_Enc_ObjTitle_Primary_M09_03 6255
#define IDS_Enc_ObjTitle_Primary_M09_04 7646
#define IDS_Enc_ObjTitle_Secondary_M07_01 6304
#define IDS_Enc_ObjTitle_Secondary_M07_02 6305
#define IDS_Enc_ObjTitle_Secondary_M07_03 6306
#define IDS_Enc_ObjTitle_Secondary_M07_04 6307
#define IDS_Enc_ObjTitle_Secondary_M07_05 6308
#define IDS_Enc_Obj_Hidden_M06_01 6019
#define IDS_Enc_Obj_Hidden_M06_02 6020
#define IDS_Enc_Obj_Hidden_M06_03 6021
#define IDS_Enc_Obj_Hidden_M06_04 6022
#define IDS_Enc_Obj_Hidden_M09_04 6025
#define IDS_Enc_Obj_Primary_M06_01 6062
#define IDS_Enc_Obj_Primary_M06_02 6063
#define IDS_Enc_Obj_Primary_M06_03 6064
#define IDS_Enc_Obj_Primary_M06_04 6065
#define IDS_Enc_Obj_Primary_M07_01 6066
#define IDS_Enc_Obj_Primary_M07_02 6067
#define IDS_Enc_Obj_Primary_M07_03 6068
#define IDS_Enc_Obj_Primary_M07_04 6069
#define IDS_Enc_Obj_Primary_M07_05 7615
#define IDS_Enc_Obj_Primary_M09_01 6074
#define IDS_Enc_Obj_Primary_M09_02 6075
#define IDS_Enc_Obj_Primary_M09_03 6076
#define IDS_Enc_Obj_Primary_M09_04 7616
#define IDS_Enc_Obj_Secondary_M07_01 6171
#define IDS_Enc_Obj_Secondary_M07_02 6172
#define IDS_Enc_Obj_Secondary_M07_03 6173
#define IDS_Enc_Obj_Secondary_M07_04 6174
#define IDS_Enc_Obj_Secondary_M07_05 6175
#define IDS_M07DSGN_DSGN0089I1DSGN_TXT 8254
#define IDS_M07DSGN_DSGN0091I1DSGN_TXT 8256
#define IDS_M09DSGN_DSGN0072I1DSGN_TXT 8262
#define IDS_M09DSGN_DSGN0073I1DSGN_TXT 8263
#define IDS_M11DSGN_DSGN1007I1DSGN_TXT 8246
#define IDS_POG_DEFEAT 7375
#define IDS_POG_DESTROY 7376
#define IDS_POG_DISABLE 7377
#define IDS_POG_ELIMINATE 7378
#define IDS_POG_ESCAPE 7379
#define IDS_POG_ESCORT 7380
#define IDS_POG_EVACUATE 7381
#define IDS_POG_HACK 7599
#define IDS_POG_PROTECT 7387
#define IDS_POG_RESCUE 7389
#define IDS_POG_RETRIEVE 7390
#define IDS_POG_SECURE 7597
#define INCREMENT 40013
#define INNATE_PRIORITY_ENEMY_SEEN 90
#define M06_DEAD_HAVOC 100600
#define M06_MENDOZA_ID 6001
#define M07_GO_ASSEMBLY 7032
#define M07_GO_EVAC_SITE 7000
#define M07_NUKE_BLAST 7001
#define M07_NUKE_IMPACT 17000
#define M09_DEAD_HAVOC 1001
#define MOBIUS_KILLED 40076
#define PATCH (Commands->Find_Object(100659))
#define SPAWN_LOC 40012
#define SYDNEY (Commands->Find_Object(100662))
#define WALK .3f
#define check 10000

class M06_Objective_Controller : public ScriptImpClass // 100018
{
	int mendoza_id = 0;
	bool accomplished_609 = false;

	enum {HAVOCS_SCRIPT, M06_INITIAL_APACHES};

	void Created(GameObject * obj)
	{
        Campaign::Begin(6);
		// Background Music
		Commands->Start_Timer(obj, this, 1.0f, 90006);
    }
    void Start_Opening(GameObject* obj) {
        Commands->Set_Background_Music("06-onyourfeet.mp3");
		// Scale their sight and hearing back
	//	Commands->Scale_AI_Awareness(.5f, .5f );

		Commands->Start_Timer (obj, this, 1.0f, HAVOCS_SCRIPT);
		Commands->Enable_Hibernation(obj, false);
		mendoza_id = 0;
        Commands->Start_Timer(obj, this, 35.0f, 90060);
		accomplished_609 = false;
		
		// EVA - Give me a position on the scientists.\n
		const char *conv_name = ("M06_CON059");
		int conv_id = Commands->Create_Conversation (conv_name, 100.0f, 200.0f, false);
		Commands->Join_Conversation((Commands->Get_A_Star(Commands->Get_Position(Owner()))), conv_id, false, true, true);
		Commands->Join_Conversation(NULL, conv_id, false, true, true);
		Commands->Start_Conversation (conv_id, 300601);
		Commands->Monitor_Conversation (obj, conv_id);	

		Add_An_Objective(605);
		Add_An_Objective(607);
		Add_An_Objective(608);
//		Add_An_Objective(609);
//		Add_An_Objective(610);

		Commands->Start_Timer (obj, this, 6.0f, M06_INITIAL_APACHES);
	}

	void Add_An_Objective(int id)
	{
		GameObject *object;
						
		switch (id)
		{
		// Hack War Room Computer
		case 601: 
			{
				Commands->Add_Objective(601, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Primary_M06_02, NULL, IDS_Enc_Obj_Primary_M06_02);
				object = Commands->Find_Object(106952);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(601, object);
					Commands->Set_Objective_HUD_Info_Position(601, 90.0f, "POG_M06_1_03.tga", IDS_POG_HACK, Commands->Get_Position (object));
				}
			}
			break;
		// Rescue Scientists
		case 603: 
			{
				Commands->Add_Objective(603, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Primary_M06_01, NULL, IDS_Enc_Obj_Primary_M06_01);
				object = Commands->Find_Object(101010);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(603, object);
					Commands->Set_Objective_HUD_Info_Position(603, 90.0f, "POG_M06_1_04.tga", IDS_POG_RESCUE, Commands->Get_Position (object));
				}
			}
			break;
		// Eliminate Mendoza
		case 604: 
			{
				Commands->Add_Objective(604, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Primary_M06_04, NULL, IDS_Enc_Obj_Primary_M06_04);
			//	object = Commands->Find_Object(mendoza_id);
				object = Commands->Find_Object(108275);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(604, object);
					Commands->Set_Objective_HUD_Info_Position(604, 90.0f, "POG_M06_1_01.tga", IDS_POG_ELIMINATE, Commands->Get_Position (object));
				}
			}
			break;
		// Rescue Resistance
		case 605: 
			{
				Commands->Add_Objective(605, OBJECTIVE_TYPE_TERTIARY, OBJECTIVE_STATUS_HIDDEN, IDS_Enc_ObjTitle_Hidden_M06_02, NULL, IDS_Enc_Obj_Hidden_M06_02);
				object = Commands->Find_Object(101016);
				
			}
			break;
		// Release GDI prisoner
		case 607: 
			{
				Commands->Add_Objective(607, OBJECTIVE_TYPE_TERTIARY, OBJECTIVE_STATUS_HIDDEN, IDS_Enc_ObjTitle_Hidden_M06_03, NULL, IDS_Enc_Obj_Hidden_M06_03);
			}
			break;
		// Plunder Raveshaw's hidden cache
		case 608: 
			{
				Commands->Add_Objective(608, OBJECTIVE_TYPE_TERTIARY, OBJECTIVE_STATUS_HIDDEN, IDS_Enc_ObjTitle_Hidden_M06_04, NULL, IDS_Enc_Obj_Hidden_M06_04);
			}
			break;
		// Deactivate alarm system
		case 609: 
			{
				Commands->Add_Objective(609, OBJECTIVE_TYPE_SECONDARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Hidden_M06_01, NULL, IDS_Enc_Obj_Hidden_M06_01);
				Vector3 star_pos = Commands->Get_Position((Commands->Get_A_Star(Commands->Get_Position(Owner()))));
				Vector3 greenhouse_alarm = Commands->Get_Position(Commands->Find_Object(101548));
				Vector3 shower_alarm = Commands->Get_Position(Commands->Find_Object(101549));
				float star_greenhouse = Commands->Get_Distance(star_pos, greenhouse_alarm);
				float star_shower = Commands->Get_Distance(star_pos, shower_alarm);
				if(star_greenhouse == (WWMath::Min(star_greenhouse, star_shower)))
				{
					object = Commands->Find_Object(101548);
					if(object)
					{
						Commands->Set_Objective_Radar_Blip_Object(609, object);
						Commands->Set_Objective_HUD_Info_Position(609, 90.0f, "POG_M08_2_02.tga", IDS_POG_DISABLE, Commands->Get_Position (object));
					}
				}
				else
				{
					object = Commands->Find_Object(101549);
					if(object)
					{
						Commands->Set_Objective_Radar_Blip_Object(609, object);
						Commands->Set_Objective_HUD_Info_Position(609, 90.0f, "POG_M08_2_02.tga", IDS_POG_DISABLE, Commands->Get_Position (object));
					}
				}


				
			}
			break;
		// Alarm State: Stand Down
		case 610: 
			{
				Commands->Add_Objective(610, OBJECTIVE_TYPE_SECONDARY, OBJECTIVE_STATUS_HIDDEN, 1000, NULL, 0);
			}
			break;
		// Escape Chateau with Dr. Sydney Mobius
		case 611: 
			{
				Commands->Set_HUD_Help_Text ( IDS_M11DSGN_DSGN1007I1DSGN_TXT, Vector3(.196f,.882f,.196f) );
				Commands->Add_Objective(611, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Primary_M06_03, NULL, IDS_Enc_Obj_Primary_M06_03);
				object = Commands->Find_Object(101527);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(611, object);
					Commands->Set_Objective_HUD_Info_Position(611, 90.0f, "POG_M06_1_05.tga", IDS_POG_ESCORT, Commands->Get_Position (object));
				}
			}
			break;
		}
	}

	void Custom(GameObject * obj, int type, int param, GameObject * sender)
	{
		if(type == M06_MENDOZA_ID)
		{
			mendoza_id = param;
			
		}
		if(type == 666 && param == 666)
		{
			
			Commands->Start_Timer (obj, this, 2.0f, M06_DEAD_HAVOC);
		}
		if(type > 611)
		{
			return;
		}
		switch (param)
		{
			case 1: 
				{
					if(type == 609 && accomplished_609)
					{
						return;
					}
					Commands->Clear_Radar_Marker (type);
					Commands->Set_Objective_Status(type, OBJECTIVE_STATUS_ACCOMPLISHED);
					if(type == 604)
					{
						Commands->Mission_Complete ( true );
					}
					if(type == 609)
					{
						accomplished_609 = true;
					}
						

				}
				break;
			case 2:
				{
					Commands->Clear_Radar_Marker (type);
					Commands->Set_Objective_Status(type, OBJECTIVE_STATUS_FAILED);
					if(type == 611)
					{
						Commands->Mission_Complete ( false );
					}
				}
				break;
			case 3: 
					Add_An_Objective(type);
				break;
			case 4: 
					Commands->Set_Objective_Status(type, OBJECTIVE_STATUS_PENDING);
				break;
			case 5: 
					Commands->Set_Objective_Status(type, OBJECTIVE_STATUS_HIDDEN);
				break;
		}
		
	}

	void Action_Complete(GameObject * obj, int action_id, ActionCompleteReason reason)
	{
		if(action_id == 300601 && (reason == ACTION_COMPLETE_CONVERSATION_ENDED || reason == ACTION_COMPLETE_CONVERSATION_INTERRUPTED))
		{
			Commands->Send_Custom_Event(obj, Commands->Find_Object(100018), 601, 3, 0.0f);
		}
	}
	
	void Timer_Expired(GameObject * obj, int timer_id )
	{
		
		if (timer_id == 90006) {
            if (!Campaign::HasPlayers()) Commands->Start_Timer(obj, this, 1.0f, 90006);
            else Start_Opening(obj);
        }
        if (timer_id == 90060) Add_An_Objective(601);
        if(timer_id == HAVOCS_SCRIPT)
		{
	//		Commands->Attach_Script((Commands->Get_A_Star(Commands->Get_Position(Owner()))), "M06_Havoc_DLS", "");
			
		}
		if(timer_id == M06_DEAD_HAVOC)
		{
		
	//		Commands->Attach_Script((Commands->Get_A_Star(Commands->Get_Position(Owner()))), "M06_Havoc_DLS", "");
			
		}

		if(timer_id == M06_INITIAL_APACHES)
		{
		
			// Initial Apaches
			GameObject *controller1 = Commands->Create_Object("Invisible_Object", Vector3(0,0,0));
			Commands->Attach_Script(controller1, "Test_Cinematic", "X6A_Apache_01.txt");
				
			GameObject *controller2 = Commands->Create_Object("Invisible_Object", Vector3(0,0,0));
			Commands->Attach_Script(controller2, "Test_Cinematic", "X6A_Apache_02.txt");
			
		}
	}

};
ScriptRegistrant<M06_Objective_Controller> reg_M06_Objective_Controller("M06_Objective_Controller", "");

class M07_Objective_Controller : public ScriptImpClass // 100657
{
	bool accomplished_705;
    bool opening_done = false;
	bool accomplished_701;
	
	enum {HAVOCS_SCRIPT, M07_DEAD_HAVOC, INITIAL_CONV, PROTECT_TEAM};

	void Created(GameObject * obj)
	{
        Campaign::Begin(7);
		// Background Music
		Commands->Set_Background_Music ("07-Got A Present For Ya.mp3");

		Commands->Start_Timer (obj, this, 0.5f, HAVOCS_SCRIPT);
		Commands->Start_Timer (obj, this, 1.0f, INITIAL_CONV);
		Add_An_Objective(709);
		accomplished_705 = false;
		accomplished_701 = false;
	}

	void Add_An_Objective(int id)
	{
		GameObject *object;
						
		switch (id)
		{
		// Evacuate Sydney
		case 701: 
			{
				Commands->Add_Objective(701, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Primary_M07_01, NULL, IDS_Enc_Obj_Primary_M07_01);
				object = Commands->Find_Object(100664);
				if(object)
				{
					Commands->Set_HUD_Help_Text ( IDS_M11DSGN_DSGN1007I1DSGN_TXT, Vector3(.196f,.882f,.196f) );
					Commands->Set_Objective_Radar_Blip_Object(701, object);
					Commands->Set_Objective_HUD_Info_Position(701, 90.0f, "POG_M07_1_04.tga", IDS_POG_EVACUATE, Commands->Get_Position (object));
				}
			}
			break;
		// Secure SAM Sites
		case 702: 
			{
				Commands->Add_Objective(702, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Primary_M07_03, NULL, IDS_Enc_Obj_Primary_M07_03);
				object = Commands->Find_Object(100673);
				if(object)
				{
					Commands->Set_HUD_Help_Text ( IDS_M07DSGN_DSGN0089I1DSGN_TXT, Vector3(.196f,.882f,.196f) );
					Commands->Set_Objective_Radar_Blip_Object(702, object);
					Commands->Set_Objective_HUD_Info_Position(702, 90.0f, "POG_M07_1_01.tga", IDS_POG_SECURE, Commands->Get_Position (object));
				}
			}
			break;
		// Destroy SSM Launchers
		case 703: 
			{
				Commands->Add_Objective(703, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Primary_M07_04, NULL, IDS_Enc_Obj_Primary_M07_04);
				object = Commands->Find_Object(100798);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(703, object);
					Commands->Set_Objective_HUD_Info_Position(703, 90.0f, "POG_M07_1_02.tga", IDS_POG_DESTROY, Commands->Get_Position (object));
				}
			}
			break;
		// Destroy Radar Installations
		case 704: 
			{
				Commands->Add_Objective(704, OBJECTIVE_TYPE_SECONDARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Secondary_M07_01, NULL, IDS_Enc_Obj_Secondary_M07_01);
				object = Commands->Find_Object(100721);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(704, object);
					Commands->Set_Objective_HUD_Info_Position(704, 90.0f, "POG_M07_2_07.tga", IDS_POG_DESTROY, Commands->Get_Position (object));
				}
			}
			break;
		// Rescue Resistance
		case 705: 
			{
				Commands->Add_Objective(705, OBJECTIVE_TYPE_SECONDARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Secondary_M07_02, NULL, IDS_Enc_Obj_Secondary_M07_02);
				object = Commands->Find_Object(115842);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(705, object);
					Commands->Set_Objective_HUD_Info_Position(705, 90.0f, "POG_M07_2_04.tga", IDS_POG_RESCUE, Commands->Get_Position (object));
				}
			}
			break;
		// Destroy Napalm Stockpile
		case 706: 
			{
				Commands->Add_Objective(706, OBJECTIVE_TYPE_SECONDARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Secondary_M07_03, NULL, IDS_Enc_Obj_Secondary_M07_03);
				object = Commands->Find_Object(100781);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(706, object);
					Commands->Set_Objective_HUD_Info_Position(706, 90.0f, "POG_M07_2_06.tga", IDS_POG_DESTROY, Commands->Get_Position (object));
				}
			}
			break;
		// Destroy Stealth Tank
		case 707: 
			{
				Commands->Add_Objective(707, OBJECTIVE_TYPE_SECONDARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Secondary_M07_04, NULL, IDS_Enc_Obj_Secondary_M07_04);
				object = Commands->Find_Object(100801);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(707, object);
					Commands->Set_Objective_HUD_Info_Position(707, 90.0f, "POG_M07_2_03.tga", IDS_POG_DESTROY, Commands->Get_Position (object));
				}
			}
			break;
		// Defeat Obelisk
		case 708: 
			{
				Commands->Add_Objective(708, OBJECTIVE_TYPE_SECONDARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Secondary_M07_05, NULL, IDS_Enc_Obj_Secondary_M07_05);
				object = Commands->Find_Object(168926);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(708, object);
					Commands->Set_Objective_HUD_Info_Position(708, 90.0f, "POG_M07_2_05.tga", IDS_POG_DEFEAT, Commands->Get_Position (object));
				}
			}
			break;
		// Protect Team Members
		case 709: 
			{
				Commands->Add_Objective(709, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Primary_M07_05, NULL, IDS_Enc_Obj_Primary_M07_05);
				object = Commands->Find_Object(100717);
				if(object)
				{
					Commands->Set_HUD_Help_Text ( IDS_M07DSGN_DSGN0091I1DSGN_TXT, Vector3(.196f,.882f,.196f) );
					Commands->Set_Objective_Radar_Blip_Object(709, object);
					Commands->Set_Objective_HUD_Info_Position(709, 90.0f, "POG_M07_1_05.tga", IDS_POG_PROTECT, Commands->Get_Position (object));
				}
			}
			break;
		// Escape Nuclear Strike
		case 710: 
			{
				Commands->Add_Objective(710, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Primary_M07_02, NULL, IDS_Enc_Obj_Primary_M07_02);
				object = Commands->Find_Object(100717);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(710, object);
					Commands->Set_Objective_HUD_Info_Position(710, 90.0f, "POG_M07_1_03.tga", IDS_POG_ESCAPE, Commands->Get_Position (object));
				}
			}
			break;
		}
	}

	void Remove_Pog(int id)
	{
		switch (id)
		{
		// Evacuate Dr. Sydney Mobius
		case 701: 
			{
				Commands->Set_Objective_HUD_Info (701, -1, "POG_M07_1_04.tga", IDS_POG_PROTECT);
			}
			break;
		}
	}

	void Custom(GameObject * obj, int type, int param, GameObject * sender)
	{
		switch (param)
		{
			case 1: 
				{
					if(type == 705 && accomplished_705)
					{
						return;
					}
					if(type == 705)
					{
						accomplished_705 = true;
					}
					if(type == 701)
					{
						accomplished_701 = true;
					}
					Commands->Clear_Radar_Marker (type);
					Commands->Set_Objective_Status(type, OBJECTIVE_STATUS_ACCOMPLISHED);
					Remove_Pog(type);
					if(type == 703)
					{
						Commands->Mission_Complete ( true );
					}
					
				}
				break;
			case 2:
				{
					if(type == 702 && accomplished_701)
					{
						return;
					}
					Commands->Clear_Radar_Marker (type);
					Commands->Set_Objective_Status(type, OBJECTIVE_STATUS_FAILED);
					Remove_Pog(type);
					if(type == 701)
					{
						Commands->Mission_Complete ( false );
					}
					if(type == 702)
					{
						Commands->Mission_Complete ( false );
					}
					if(type == 709)
					{
						Commands->Mission_Complete ( false );
					}
					if(type == 710)
					{
						Commands->Mission_Complete ( false );
					}
				}
				break;
			case 3: 
					Add_An_Objective(type);
				break;
			case 4: 
					Commands->Set_Objective_Status(type, OBJECTIVE_STATUS_PENDING);
				break;
			case 5: 
					Commands->Set_Objective_Status(type, OBJECTIVE_STATUS_HIDDEN);
				break;
		}
	}

		
	void Timer_Expired(GameObject * obj, int timer_id )
	{
		if(timer_id == INITIAL_CONV)
		{
            if (!Campaign::HasPlayers() || !GUNNER || !SYDNEY) { Commands->Start_Timer(obj, this, 1.0f, INITIAL_CONV); return; }
            Commands->Start_Timer(obj, this, 50.0f, 90070);
			// How are Dr. Mobius and Sydney?\n
			const char *conv_name = ("M07_CON001");
			int conv_id = Commands->Create_Conversation (conv_name, INNATE_PRIORITY_ENEMY_SEEN - 5, 200.0f, false);
			Commands->Join_Conversation(NULL, conv_id, false, true, true);
			Commands->Join_Conversation((Commands->Get_A_Star(Commands->Get_Position(Owner()))), conv_id, false, true, true);
			Commands->Join_Conversation(NULL, conv_id, false, true, true);
			Commands->Join_Conversation(GUNNER, conv_id, true, true, true);
			Commands->Start_Conversation (conv_id, 300701);
			Commands->Monitor_Conversation (obj, conv_id);	
		}
		if (timer_id == 90070) Action_Complete(obj, 300701, ACTION_COMPLETE_CONVERSATION_ENDED);
        if(timer_id == PROTECT_TEAM)
		{
			Add_An_Objective(710);
		}
	}

	void Action_Complete(GameObject * obj, int action_id, ActionCompleteReason reason)
	{
		ActionParamsStruct params;

		if (!opening_done && action_id == 300701 && (reason == ACTION_COMPLETE_CONVERSATION_ENDED || reason == ACTION_COMPLETE_CONVERSATION_INTERRUPTED))
		{
			opening_done = true;
            Add_An_Objective(701);

			Commands->Start_Timer (obj, this, 2.0f, PROTECT_TEAM);
			
			// Start nuclear strike countdown
			Commands->Send_Custom_Event(obj, Commands->Find_Object(100663), M07_GO_EVAC_SITE, 1, 0.0f);
			// Customs to move team to evac site
			// Gunner
			Commands->Send_Custom_Event(obj, GUNNER, M07_GO_ASSEMBLY, 1, 0.0f);
			// Deadeye
			Commands->Send_Custom_Event(obj, DEADEYE, M07_GO_ASSEMBLY, 1, 0.0f);
			// Sydney
			Commands->Send_Custom_Event(obj, SYDNEY, M07_GO_ASSEMBLY, 1, 0.0f);
			// Hotwire
			Commands->Send_Custom_Event(obj, HOTWIRE, M07_GO_ASSEMBLY, 1, 2.0f);
			// Patch
			Commands->Send_Custom_Event(obj, PATCH, M07_GO_ASSEMBLY, 1, 2.0f);
		}
	}
};
ScriptRegistrant<M07_Objective_Controller> reg_M07_Objective_Controller("M07_Objective_Controller", "");

class M07_Havoc_DLS : public ScriptImpClass
{
	bool nuke_blast;

	enum {HAVOC_ALARM_SOUND};

	void Created(GameObject * obj)
	{
		nuke_blast = true;
		Commands->Give_PowerUp(obj, "POW_Chaingun_Player", false);
		Commands->Give_PowerUp(obj, "POW_SniperRifle_Player", false);
		Commands->Give_PowerUp(obj, "POW_RocketLauncher_Player", false);
		Commands->Give_PowerUp(obj, "POW_MineRemote_Player", false);
	}

	void Custom( GameObject * obj, int type, int param, GameObject * sender ) 
	{
		if ( type == M07_NUKE_BLAST ) 
		{
			nuke_blast = (param == 1) ? true : false;
		}
	}

	void Sound_Heard(GameObject * obj, const CombatSound &sound)
	{
		if (sound.sound == M07_NUKE_IMPACT)
		{
			if(nuke_blast)
			{
				Commands->Apply_Damage( obj, 10000.0f, "DEATH", nullptr);
			}
		} 
	}

	void Enemy_Seen(GameObject * obj, GameObject *enemy )
	{
		if(enemy == Commands->Find_Object(100801))
		{
			// You've got to be kidding.\n
			const char *conv_name = ("M07_CON015");
			int conv_id = Commands->Create_Conversation (conv_name, 100.0f, 200.0f, false);
			Commands->Join_Conversation((Commands->Get_A_Star(Commands->Get_Position(Owner()))), conv_id, false, true, true);
			Commands->Start_Conversation (conv_id, 300707);
			Commands->Monitor_Conversation (obj, conv_id);
		}
	}

	void Action_Complete(GameObject * obj, int action_id, ActionCompleteReason reason)
	{
		if (action_id == 300702 && reason == ACTION_COMPLETE_CONVERSATION_ENDED)
		{
			// Defeat Stealth Tank.
			Commands->Send_Custom_Event(obj, Commands->Find_Object(100657), 707, 3, 0.0f);
			// Change soundtrack to attack
			Commands->Set_Background_Music ("Raveshaw_Act on Instinct");
		}
	}

	void Killed (GameObject * obj, GameObject * killer)
	{
		// Coop: individual deaths respawn through the multiplayer engine.
	}

};
ScriptRegistrant<M07_Havoc_DLS> reg_M07_Havoc_DLS("M07_Havoc_DLS", "");

class M09_Objective_Controller : public ScriptImpClass // Object Controller id: 2000071
{
	
	enum {HAVOCS_SCRIPT};
	bool objective [3];
	int spawn_loc;
	int block1, block2, block3, block4;

	void Created(GameObject * obj)
	{
        Campaign::Begin(9);
		Commands->Set_Num_Tertiary_Objectives ( 1 );

		Commands->Start_Timer (obj, this, 1.0f, HAVOCS_SCRIPT);
		Commands->Enable_Hibernation(obj, false);
		
		spawn_loc = 0;

		for (int x = 0; x <= 2; x++)
		{
			objective [x] = false;
		}

		block1 = 0;
		block2 = 0;
		block3 = 0;
		block4 = 0;
					
	}

	void Remove_Pog(int id)
	{
		switch (id)
		{
		// New Primary Mission Objective:  Retrieve the Mobile Power Suit for Dr. Mobius
		case 901: 
			{
				Commands->Set_Objective_HUD_Info(id, -1, "POG_M09_1_01.tga", IDS_POG_RETRIEVE);
			}
			break;
		// New Primary Mission Objective: Escort Dr. Mobius to the Surface
		case 902: 
			{
				Commands->Set_Objective_HUD_Info(id, -1, "POG_M09_1_02.tga", IDS_POG_ESCORT);
			}
			break;
		// New Primary Mission Objective: Escort Dr. Mobius to the Rendezvous Point
		case 903: 
			{
				Commands->Set_Objective_HUD_Info(id, -1, "POG_M09_1_03.tga", IDS_POG_ESCORT);
			}
			break;	
		}
	}

	void Add_An_Objective(int id)
	{
		GameObject *object;
						
		switch (id)
		{
		// New Primary Mission Objective:  Protect Mobius

		case 900: 
			{
				Commands->Add_Objective(900, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Primary_M09_04, NULL, IDS_Enc_Obj_Primary_M09_04);
				
				object = Commands->Find_Object(2000010);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(900, object);
					Commands->Set_Objective_HUD_Info_Position( 900, 90, "POG_M09_1_02.tga", IDS_POG_PROTECT, Commands->Get_Position (object));
				}
			}
			break;

		// New Primary Mission Objective:  Retrieve the Mobile Power Suit for Dr. Mobius

		case 901: 
			{
				Commands->Add_Objective(901, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Primary_M09_01, NULL, IDS_Enc_Obj_Primary_M09_01);
				
				object = Commands->Find_Object(2002239);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(901, object);
					Commands->Set_Objective_HUD_Info_Position( 901, 99, "POG_M09_1_01.tga", IDS_POG_RETRIEVE, Commands->Get_Position (object));
				}
			}
			break;
		// New Primary Mission Objective: Escort Dr. Mobius to the Surface
		case 902: 
			{
				Commands->Add_Objective(902, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Primary_M09_02, NULL, IDS_Enc_Obj_Primary_M09_02);

				object = Commands->Find_Object(2000955);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(902, object);
					Commands->Set_Objective_HUD_Info_Position( 902, 98, "POG_M08_1_03.tga", IDS_POG_ESCORT, Commands->Get_Position (object));
				}
			}
			break;
		// New Primary Mission Objective: Escort Dr. Mobius to the Rendezvous Point
		case 903: 
			{
				Commands->Add_Objective(903, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_Enc_ObjTitle_Primary_M09_03, NULL, IDS_Enc_Obj_Primary_M09_03);

				object = Commands->Find_Object(2000969);
				if(object)
				{
					Commands->Set_Objective_Radar_Blip_Object(903, object);
					Commands->Set_Objective_HUD_Info_Position( 903, 97, "POG_M09_1_03.tga", IDS_POG_ESCORT, Commands->Get_Position (object));
				}
			}
			break;
		// New Hidden Mission Objective: Eliminate the NOD Resistance at Checkpoint A
		
		case 904: 
			{
				Commands->Add_Objective(904, OBJECTIVE_TYPE_TERTIARY, OBJECTIVE_STATUS_HIDDEN, IDS_Enc_ObjTitle_Hidden_M09_01, NULL, IDS_Enc_Obj_Hidden_M09_04);			}
			break;/*
		// New Hidden Mission Objective: Comandeer the NOD Stealth Tank.
		case 905: 
			{
				Commands->Add_Objective(905, OBJECTIVE_TYPE_TERTIARY, OBJECTIVE_STATUS_HIDDEN, IDS_M09_T02, NULL, 0);
			}
			break;
		// New Hidden Mission Objective: Comandeer the NOD Stealth Tank.
		case 906: 
			{
				Commands->Add_Objective(906, OBJECTIVE_TYPE_PRIMARY, OBJECTIVE_STATUS_PENDING, IDS_M09_P05, NULL, 0);
			}
			break;*/
		case 1000:
			{
				break;
			}
		}
	}

	void Custom(GameObject * obj, int type, int param, GameObject * sender)
	{
		if (type == BLOCK_ON)
		{
			if ((block1 != 0) && (block2 != 0) && (block3 != 0) && (block4 != 0)) {
				Commands->Start_Timer (obj, this, 3.0f, 333);

				GameObject *rub1 = Commands->Create_Object ("M08_Rubble_Stub", Vector3 (-108.066f, 483.917f, -191.022f));
				block1 = Commands->Get_ID (rub1);
				Commands->Set_Is_Rendered( rub1, false );
				GameObject *rub2 = Commands->Create_Object ("M08_Rubble_Stub", Vector3 (-25.433f, 513.013f, -150.613f));
				block2 = Commands->Get_ID (rub2);
				Commands->Set_Is_Rendered( rub2, false );
				GameObject *rub3 = Commands->Create_Object ("M08_Rubble_Stub", Vector3 (-47.948f, 535.216f, -103.284f));
				block3 = Commands->Get_ID (rub3);
				Commands->Set_Is_Rendered( rub3, false );
				Commands->Set_Facing (rub3, -90.0);
				GameObject *rub4 = Commands->Create_Object ("M08_Rubble_Stub", Vector3 (39.231f, 515.105f, -78.137f));
				block4 = Commands->Get_ID (rub4);
				Commands->Set_Is_Rendered( rub4, false );
			}
		}

		if (type == BLOCK_OFF)
		{
			Commands->Destroy_Object (Commands->Find_Object (block1));
			Commands->Destroy_Object (Commands->Find_Object (block2));
			Commands->Destroy_Object (Commands->Find_Object (block3));
			Commands->Destroy_Object (Commands->Find_Object (block4));
			block1 = block2 = block3 = block4 = 0;
		}


		if (type == MOBIUS_KILLED)
		{
			Commands->Set_HUD_Help_Text ( IDS_M09DSGN_DSGN0073I1DSGN_TXT, Vector3(.196f,.882f,.196f) );

			Commands->Start_Timer (obj, this, 4.0f, 40);
		}

		if (type == check && param == check)
		{
			Commands->Send_Custom_Event (obj, sender, 901, objective [0], 0.0f);
			Commands->Send_Custom_Event (obj, sender, 902, objective [1], 0.0f);
			Commands->Send_Custom_Event (obj, sender, 903, objective [2], 0.0f);
		}

		if (type == 901 && param == 1)
		{
			objective [0] = true;
			Remove_Pog(901);
		}

		if (type == 902 && param == 1)
		{
			objective [1] = true;
			Remove_Pog(902);
		}

		if (type == 903 && param == 1)
		{
			objective [2] = true;
			Remove_Pog(903);
		}

		switch (param)
		{
			case 1: 
				{
					//Commands->Clear_Radar_Marker (type);
					Commands->Set_Objective_Status(type, OBJECTIVE_STATUS_ACCOMPLISHED);
				}
				break;
			case 2:
				{
					//Commands->Clear_Radar_Marker (type);
					Commands->Set_Objective_Status(type, OBJECTIVE_STATUS_FAILED);
				}
				break;
			case 3: 
					Add_An_Objective(type);
				break;
			case 4: 
					Commands->Set_Objective_Status(type, OBJECTIVE_STATUS_PENDING);
				break;
			case 5: 
					Commands->Set_Objective_Status(type, OBJECTIVE_STATUS_HIDDEN);
				break;
		}
		
		if(type == 666 && param == 666)
		{
			
			Commands->Start_Timer (obj, this, 0.10f, M09_DEAD_HAVOC);
		}

		if (type == SPAWN_LOC && param == INCREMENT)
		{
			spawn_loc++;
		}
	}

		
	void Timer_Expired(GameObject * obj, int timer_id )
	{
		if (timer_id == 333)
		{
			Commands->Destroy_Object (Commands->Find_Object (block1));
			Commands->Destroy_Object (Commands->Find_Object (block2));
			Commands->Destroy_Object (Commands->Find_Object (block3));
			Commands->Destroy_Object (Commands->Find_Object (block4));
			block1 = block2 = block3 = block4 = 0;
		}

		if (timer_id == 40)
		{
			Commands->Mission_Complete(false);
		}	
	}
};
ScriptRegistrant<M09_Objective_Controller> reg_M09_Objective_Controller("M09_Objective_Controller", "");

class M09_Mobius_Initial_Conversation : public ScriptImpClass
{
	float max_health;
    bool released = false;

	void Created (GameObject *obj)
	{
		Commands->Set_HUD_Help_Text ( IDS_M09DSGN_DSGN0072I1DSGN_TXT, Vector3(.196f,.882f,.196f) );

		Commands->Start_Timer (obj, this, 5.0f, 20);

		Commands->Send_Custom_Event (obj, Commands->Find_Object(2000071), 900, 3, 0.0f);

		Commands->Innate_Disable ( obj );
		max_health = Commands->Get_Health (obj);
	}

	void Timer_Expired(GameObject * obj, int timer_id )
	{
		if (timer_id == 21) { Action_Complete(obj, 903, ACTION_COMPLETE_CONVERSATION_INTERRUPTED); return; }
        if (timer_id == 20)
		{
            if (!Campaign::HasPlayers()) { Commands->Start_Timer(obj, this, 1.0f, 20); return; }
            Commands->Start_Timer(obj, this, 60.0f, 21);	
			const char *conv_name = ("IDS_M09_D07");
			int conv_id = Commands->Create_Conversation (conv_name, 99, 200, false);
			Commands->Join_Conversation(obj, conv_id, false, false, true);
			Commands->Join_Conversation((Commands->Get_A_Star(Commands->Get_Position(Owner()))), conv_id, false, false, true);
			Commands->Start_Conversation (conv_id, 900);
			Commands->Monitor_Conversation (obj, conv_id);
		}
	}

	/*void Damaged (GameObject *obj, GameObject *damager, float amount)
	{
		Commands->Set_Health (obj, max_health);
	}*/
	
	void Killed(GameObject * obj, GameObject *killer )
	{
		Commands->Send_Custom_Event (obj, Commands->Find_Object(2000071), MOBIUS_KILLED, 0, 0.0f);		
	}
	
	void Action_Complete(GameObject * obj, int action_id, ActionCompleteReason reason)
	{
		if(action_id == 900 && (reason == ACTION_COMPLETE_CONVERSATION_ENDED || reason == ACTION_COMPLETE_CONVERSATION_INTERRUPTED))
		{
			const char *conv_name = ("IDS_M09_P01");
			int conv_id = Commands->Create_Conversation (conv_name, 99, 200, false);
			Commands->Join_Conversation(obj, conv_id, false, false, true);
			Commands->Join_Conversation((Commands->Get_A_Star(Commands->Get_Position(Owner()))), conv_id, false, false, true);
			Commands->Start_Conversation (conv_id, 903);
			Commands->Monitor_Conversation (obj, conv_id);
		}

		if(action_id == 901 && (reason == ACTION_COMPLETE_CONVERSATION_ENDED || reason == ACTION_COMPLETE_CONVERSATION_INTERRUPTED))
		{
			ActionParamsStruct params;

			params.Set_Basic( this, 99, 101 );
			params.Set_Movement( Vector3(0,0,0), WALK, 0.5f);
			params.WaypathID = 2000275;
			
			Commands->Action_Goto (obj, params);
		}

		if (action_id == 101)
		{
			
			/*ActionParamsStruct params;
			params.Set_Basic( this, 99, 150 );
			params.Set_Face_Location( Commands->Get_Position ((Commands->Get_A_Star(Commands->Get_Position(Owner())))), 10.0f);
			Commands->Action_Face_Location ( obj, params );
			*/
			const char *conv_name = ("IDS_M09_D07");
			int conv_id = Commands->Create_Conversation (conv_name, 99, 200, false);
			Commands->Join_Conversation(obj, conv_id, false, false, true);
			Commands->Join_Conversation((Commands->Get_A_Star(Commands->Get_Position(Owner()))), conv_id, false, false, true);
			Commands->Start_Conversation (conv_id, 902);
			Commands->Monitor_Conversation (obj, conv_id);
		}

		if(action_id == 902 && (reason == ACTION_COMPLETE_CONVERSATION_ENDED || reason == ACTION_COMPLETE_CONVERSATION_INTERRUPTED))
		{
			const char *conv_name = ("IDS_M09_P01");
			int conv_id = Commands->Create_Conversation (conv_name, 99, 200, false);
			Commands->Join_Conversation(obj, conv_id, false, false, true);
			Commands->Join_Conversation((Commands->Get_A_Star(Commands->Get_Position(Owner()))), conv_id, false, false, true);
			Commands->Start_Conversation (conv_id, 903);
			Commands->Monitor_Conversation (obj, conv_id);
		}

		if(!released && action_id == 903 && (reason == ACTION_COMPLETE_CONVERSATION_ENDED || reason == ACTION_COMPLETE_CONVERSATION_INTERRUPTED))
		{
			Commands->Send_Custom_Event (obj, Commands->Find_Object(2000071), 901, 3, 0.0f);
			released = true;
            Commands->Innate_Enable(obj);
			Commands->Attach_Script(Commands->Find_Object (2000010), "M09_Mobius_Follow", "");
			Commands->Send_Custom_Event (obj, Commands->Find_Object(2000279), CONVERSATION, ENDED, 0.0f);
			Commands->Send_Custom_Event( obj, Commands->Find_Object (2000010), FOLLOW, 2001012, 1.0f );

			Commands->Give_PowerUp(obj, "POW_Pistol_AI", false);
			Commands->Select_Weapon (obj, "Weapon_Pistol_Ai" );
		}
	}
};
ScriptRegistrant<M09_Mobius_Initial_Conversation> reg_M09_Mobius_Initial_Conversation("M09_Mobius_Initial_Conversation", "");

#undef BLOCK_OFF
#undef BLOCK_ON
#undef CONVERSATION
#undef DEADEYE
#undef ENDED
#undef FOLLOW
#undef GUNNER
#undef HOTWIRE
#undef IDS_Enc_ObjTitle_Hidden_M06_01
#undef IDS_Enc_ObjTitle_Hidden_M06_02
#undef IDS_Enc_ObjTitle_Hidden_M06_03
#undef IDS_Enc_ObjTitle_Hidden_M06_04
#undef IDS_Enc_ObjTitle_Hidden_M09_01
#undef IDS_Enc_ObjTitle_Primary_M06_01
#undef IDS_Enc_ObjTitle_Primary_M06_02
#undef IDS_Enc_ObjTitle_Primary_M06_03
#undef IDS_Enc_ObjTitle_Primary_M06_04
#undef IDS_Enc_ObjTitle_Primary_M07_01
#undef IDS_Enc_ObjTitle_Primary_M07_02
#undef IDS_Enc_ObjTitle_Primary_M07_03
#undef IDS_Enc_ObjTitle_Primary_M07_04
#undef IDS_Enc_ObjTitle_Primary_M07_05
#undef IDS_Enc_ObjTitle_Primary_M09_01
#undef IDS_Enc_ObjTitle_Primary_M09_02
#undef IDS_Enc_ObjTitle_Primary_M09_03
#undef IDS_Enc_ObjTitle_Primary_M09_04
#undef IDS_Enc_ObjTitle_Secondary_M07_01
#undef IDS_Enc_ObjTitle_Secondary_M07_02
#undef IDS_Enc_ObjTitle_Secondary_M07_03
#undef IDS_Enc_ObjTitle_Secondary_M07_04
#undef IDS_Enc_ObjTitle_Secondary_M07_05
#undef IDS_Enc_Obj_Hidden_M06_01
#undef IDS_Enc_Obj_Hidden_M06_02
#undef IDS_Enc_Obj_Hidden_M06_03
#undef IDS_Enc_Obj_Hidden_M06_04
#undef IDS_Enc_Obj_Hidden_M09_04
#undef IDS_Enc_Obj_Primary_M06_01
#undef IDS_Enc_Obj_Primary_M06_02
#undef IDS_Enc_Obj_Primary_M06_03
#undef IDS_Enc_Obj_Primary_M06_04
#undef IDS_Enc_Obj_Primary_M07_01
#undef IDS_Enc_Obj_Primary_M07_02
#undef IDS_Enc_Obj_Primary_M07_03
#undef IDS_Enc_Obj_Primary_M07_04
#undef IDS_Enc_Obj_Primary_M07_05
#undef IDS_Enc_Obj_Primary_M09_01
#undef IDS_Enc_Obj_Primary_M09_02
#undef IDS_Enc_Obj_Primary_M09_03
#undef IDS_Enc_Obj_Primary_M09_04
#undef IDS_Enc_Obj_Secondary_M07_01
#undef IDS_Enc_Obj_Secondary_M07_02
#undef IDS_Enc_Obj_Secondary_M07_03
#undef IDS_Enc_Obj_Secondary_M07_04
#undef IDS_Enc_Obj_Secondary_M07_05
#undef IDS_M07DSGN_DSGN0089I1DSGN_TXT
#undef IDS_M07DSGN_DSGN0091I1DSGN_TXT
#undef IDS_M09DSGN_DSGN0072I1DSGN_TXT
#undef IDS_M09DSGN_DSGN0073I1DSGN_TXT
#undef IDS_M11DSGN_DSGN1007I1DSGN_TXT
#undef IDS_POG_DEFEAT
#undef IDS_POG_DESTROY
#undef IDS_POG_DISABLE
#undef IDS_POG_ELIMINATE
#undef IDS_POG_ESCAPE
#undef IDS_POG_ESCORT
#undef IDS_POG_EVACUATE
#undef IDS_POG_HACK
#undef IDS_POG_PROTECT
#undef IDS_POG_RESCUE
#undef IDS_POG_RETRIEVE
#undef IDS_POG_SECURE
#undef INCREMENT
#undef INNATE_PRIORITY_ENEMY_SEEN
#undef M06_DEAD_HAVOC
#undef M06_MENDOZA_ID
#undef M07_GO_ASSEMBLY
#undef M07_GO_EVAC_SITE
#undef M07_NUKE_BLAST
#undef M07_NUKE_IMPACT
#undef M09_DEAD_HAVOC
#undef MOBIUS_KILLED
#undef PATCH
#undef SPAWN_LOC
#undef SYDNEY
#undef WALK
#undef check
