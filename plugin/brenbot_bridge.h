// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sstream>
namespace BrenBot {
static std::map<int,unsigned> sessions;
static unsigned serial=0;
static DWORD snapshotAt=0;
static long offset=0;
static std::string Quote(const wchar_t* text) {
    std::string out="\"";
    for (unsigned i=0; text && text[i] && i<1024; ++i) {
        unsigned c=static_cast<unsigned short>(text[i]);
        if (c>=32 && c<127 && c!='"' && c!='\\') out+=static_cast<char>(c);
        else { char escaped[8]; sprintf(escaped,"\\u%04x",c); out+=escaped; }
    }
    return out+"\"";
}
static void Write(const std::string& line) {
    FILE* out=fopen("brenbot-events.jsonl","ab");
    if (out) { fputs(line.c_str(),out); fputc('\n',out); fclose(out); }
}
static std::string Player(cPlayer* p) {
    const int id=p->Get_Id(); if (!sessions.count(id)) sessions[id]=++serial;
    std::ostringstream out;
    out << "\"id\":" << id << ",\"session\":" << sessions[id] << ",\"name\":" << Quote(p->Get_Name())
        << ",\"team\":" << p->Get_Player_Type();
    return out.str();
}
static void Snapshot() {
    std::string row="{\"event\":\"players\",\"players\":["; bool first=true;
    for (int id=1; id<=127; ++id) if (auto* p=Find_Player(id)) if (p->Is_Active()) {
        if (!first) row+=","; first=false; row+="{"+Player(p)+"}";
    }
    Write(row+"]}");
}
static void Loaded() {
    Write("{\"event\":\"map\",\"map\":"+Quote(WideStringClass(The_Game()->Get_Map_Name()))+"}");
    Snapshot();
}
static void Tick() {
    const DWORD now=GetTickCount();
    if (now-snapshotAt>=5000) { snapshotAt=now; Snapshot(); }
    FILE* input=fopen("brenbot.in","rb"); if (!input) return;
    fseek(input,offset,SEEK_SET); char line[1024];
    while (fgets(line,sizeof(line),input)) {
        char* end=strchr(line,'\n'); if (!end) break;
        offset=ftell(input); *end=0;
        std::istringstream in(line); std::string operation; unsigned session; int id;
        if (!(in>>operation>>session>>id)) continue;
        auto* p=Find_Player(id);
        if (!p || !p->Is_Active() || !sessions.count(id) || sessions[id]!=session) continue;
        std::string value; std::getline(in,value); if (!value.empty() && value.front()==' ') value.erase(0,1);
        if (value.find_first_of("\r\n")!=std::string::npos) continue;
        if (operation=="page") DA::Page_Player(p,"%s",value.substr(0,240).c_str());
        else if (operation=="sound") {
            bool valid=value.size()>4 && value.size()<128 && value.substr(value.size()-4)==".wav" && value.find("..") == std::string::npos;
            for (unsigned char c : value) valid &= std::isalnum(c) || c=='_' || c=='-' || c=='.';
            if (valid) Create_2D_WAV_Sound_Player_By_ID(id,value.c_str());
        }
    }
    fclose(input);
}
class Events : public DAEventClass {
public:
    Events() {
        for (const auto* name : {"brenbot.in","brenbot-events.jsonl"}) { FILE* f=fopen(name,"wb"); if (f) fclose(f); }
        Register_Event(DAEvent::CHAT,INT_MAX);
        Register_Event(DAEvent::PLAYERJOIN); Register_Event(DAEvent::PLAYERLEAVE);
    }
    void Player_Join_Event(cPlayer* p) override { sessions[p->Get_Id()]=++serial; Write("{\"event\":\"join\","+Player(p)+"}"); }
    void Player_Leave_Event(cPlayer* p) override { Write("{\"event\":\"leave\","+Player(p)+"}"); sessions.erase(p->Get_Id()); }
    bool Chat_Event(cPlayer* p,TextMessageEnum type,const wchar_t* message,int) override {
        if (p && p->Is_Active() && Campaign::CoopMap() && type!=TEXT_MESSAGE_PRIVATE)
            Write("{\"event\":\"chat\","+Player(p)+",\"scope\":\""+(type==TEXT_MESSAGE_TEAM ? "team" : "public")+"\",\"message\":"+Quote(message)+"}");
        return true;
    }
};
}
