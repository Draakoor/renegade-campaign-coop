// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <vector>
#include <cctype>
namespace TextRules {
inline std::string Lower(std::string value) {
    for (auto& c : value) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
inline bool Number(const std::string& text,int& value,int maximum=1000000) {
    if (text.empty() || text.size()>7) return false;
    int n=0;
    for (char c : text) { if (c<'0' || c>'9') return false; n=n*10+c-'0'; if (n>maximum) return false; }
    if (!n) return false;
    value=n; return true;
}
inline bool Word(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c=='_'; }
inline bool Phrase(const std::string& message,const std::string& key) {
    if (key.empty()) return false;
    for (size_t pos=message.find(key); pos!=std::string::npos; pos=message.find(key,pos+1))
        if ((!pos || !Word(message[pos-1])) && (pos+key.size()==message.size() || !Word(message[pos+key.size()]))) return true;
    return false;
}
inline std::string MapName(std::string name) {
    name=Lower(name);
    if (name=="tutorial" || name=="training" || name=="m00") name="m00_tutorial";
    if (name.size()>4 && name.substr(name.size()-4)==".mix") name.resize(name.size()-4);
    return name;
}
// Rotate the existing cycle so the requested map occupies the next slot.
// Order and membership are unchanged, including the map following the target.
inline std::vector<std::string> Rotate(const std::vector<std::string>& maps,int current,int target) {
    if (maps.empty() || current<0 || target<0 || current>=static_cast<int>(maps.size()) || target>=static_cast<int>(maps.size())) return {};
    std::vector<std::string> result(maps.size());
    const int count=static_cast<int>(maps.size()),next=(current+1)%count;
    for (int i=0; i<count; ++i) result[(next+i)%count]=maps[(target+i)%count];
    return result;
}
}
