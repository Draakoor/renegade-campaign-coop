// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "PhysDefClass.h"
#include "AudibleSoundDefinitionClass.h"
#include "TranslateDBClass.h"
#include <cctype>
#include "text_rules.h"

namespace Cosmetics {
struct Skin { std::string model,label; std::vector<std::string> presets; };
static std::vector<Skin> catalog;
static std::string Lower(std::string value) {
    for (auto& c : value) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
static std::string Basename(std::string value) {
    const auto slash=value.find_last_of("/\\");
    return slash==std::string::npos ? value : value.substr(slash+1);
}
static std::string Model(const SoldierGameObjDef* def) {
    auto* physics=DefinitionMgrClass::Find_Definition(def->Get_Phys_Def_ID(),true);
    if (!physics) return {};
    auto name=Lower(Basename(static_cast<PhysDefClass*>(physics)->Get_Model_Name().Peek_Buffer()));
    if (name.size()>4 && name.substr(name.size()-4)==".w3d") name.resize(name.size()-4);
    return name;
}
static bool Available(const std::string& filename) {
    auto* file=Get_Data_File(filename.c_str());
    const bool available=file && file->Is_Available();
    if (file) Close_Data_File(file);
    return available;
}
static std::string Resolve(std::string name) {
    name=Lower(name); int index=0;
    if (TextRules::Number(name,index,static_cast<int>(catalog.size()))) return catalog[index-1].model;
    const std::map<std::string,std::string> aliases={
        {"havoc","c_ag_havoc"},{"soldier","c_ag_gdi_mg"},{"engineer","c_ag_gdi_en"},
        {"hotwire","c_ag_gdi_hotw"},{"gunner","c_ag_gdi_gun"},{"patch","c_ag_gdi_ptch"},
        {"deadeye","c_ag_gdi_dead"},{"mobius","c_ag_gdi_mobi"},{"sydney","c_ag_gdi_psyd"},
        {"sakura","c_ag_nod_saku"},{"kane","c_ag_nod_kane"},{"mendoza","c_ag_nod_mdz"},
        {"raveshaw","c_ag_nod_rav"},{"technician","c_ag_nod_tc"},{"stealth","c_ag_nod_stlth"}};
    auto alias=aliases.find(name); if (alias!=aliases.end()) name=alias->second;
    std::string found;
    for (const auto& skin : catalog) {
        if (skin.model==name) return skin.model;
        for (const auto& preset : skin.presets) if (Lower(preset)==name) return skin.model;
        if (Lower(skin.label)==name) { if (!found.empty()) return {}; found=skin.model; }
    }
    return found;
}
static void List(cPlayer* player,const DATokenClass& text) {
    int page=1; std::string filter;
    if (text.Size()>2) { DA::Page_Player(player,"Use !skins [search] [page], for example !skins nod 2."); return; }
    if (text.Size() && !TextRules::Number(text[1].Peek_Buffer(),page)) filter=Lower(text[1].Peek_Buffer());
    if (text.Size()==2 && !TextRules::Number(text[2].Peek_Buffer(),page)) { DA::Page_Player(player,"Page must be a positive number."); return; }
    std::vector<unsigned> matches;
    for (unsigned i=0; i<catalog.size(); ++i) {
        std::string words=catalog[i].model+" "+Lower(catalog[i].label);
        for (const auto& preset : catalog[i].presets) words+=" "+Lower(preset);
        if (words.find(filter)!=std::string::npos) matches.push_back(i);
    }
    const int pages=(std::max)(1,(static_cast<int>(matches.size())+5)/6); page=(std::min)(page,pages);
    DA::Page_Player(player,"Skins: %u matches, page %d/%d. !skins [search] [page]; !skin <ID|alias|model>; !skin default.",static_cast<unsigned>(matches.size()),page,pages);
    for (unsigned n=(page-1)*6; n<matches.size() && n<static_cast<unsigned>(page*6); ++n) {
        const auto i=matches[n]; const auto& skin=catalog[i];
        DA::Page_Player(player,"%u: %s [%s]",i+1,skin.label.c_str(),skin.model.c_str());
    }
}
static void Build() {
    catalog.clear(); std::map<std::string,Skin> models;
    for (auto* def=DefinitionMgrClass::Get_First(CID_Soldier); def; def=DefinitionMgrClass::Get_Next(def,CID_Soldier)) {
        auto name=Model(static_cast<SoldierGameObjDef*>(def));
        if (name.empty() || name=="null" || name=="none" || !Available(name+".w3d")) continue;
        auto& entry=models[name]; entry.model=name; entry.presets.push_back(def->Get_Name());
        if (entry.label.empty() || entry.label=="No String") {
            const char* translated=Get_Translated_Definition_Name(def->Get_Name());
            entry.label=translated; delete[] translated;
            if (entry.label.empty()) entry.label=def->Get_Name();
        }
    }
    for (auto& pair : models) catalog.push_back(pair.second);
    Console_Output("[Novara] Cosmetic catalog: %u character models.\n",static_cast<unsigned>(catalog.size()));
}
static void Dump() {
    if (!DASettingsManager::Get_Bool("CampaignDiagnostics",false) || The_Game()->Get_Current_Players()!=0) return;
    FILE* out=fopen("cosmetic-catalog.tsv","wb"); if (!out) return;
    for (const auto& skin : catalog) {
        fprintf(out,"skin\t%s\t%s",skin.model.c_str(),skin.label.c_str());
        for (const auto& preset : skin.presets) fprintf(out,"\t%s",preset.c_str());
        fputs("\n",out);
    }
    for (auto* def=DefinitionMgrClass::Get_First(0x5000); def; def=DefinitionMgrClass::Get_Next(def,0x5000)) {
        const auto* sound=static_cast<AudibleSoundDefinitionClass*>(def);
        fprintf(out,"sound\t%s\t%s\t%s\n",def->Get_Name(),sound->Get_Filename().Peek_Buffer(),sound->Get_Display_Text().Peek_Buffer());
    }
    for (int i=1000; i<1000+TranslateDBClass::Get_Object_Count(); ++i) {
        auto* entry=TranslateDBClass::Find_Object(i);
        if (!entry || !entry->Get_Sound_ID()) continue;
        auto* def=DefinitionMgrClass::Find_Definition(entry->Get_Sound_ID(),true);
        if (def && def->Get_Class_ID()==0x5000)
            fprintf(out,"voice\t%s\t%s\n",def->Get_Name(),entry->Get_English_String().Peek_Buffer());
    }
    fclose(out); Console_Output("[Novara] Wrote cosmetic-catalog.tsv (models and sound definitions).\n");
}
}
