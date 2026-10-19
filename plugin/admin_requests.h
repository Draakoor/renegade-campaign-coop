// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <map>
#include <string>

// Panel approval is a one-use request bound to the current connection.
// The nickname is an allowlist hint, never an authentication credential.
class AdminRequests {
    struct Request { std::string code; uint32_t created; };
    std::map<int,Request> entries;
public:
    void disconnect(int id) { entries.erase(id); }
    void issue(int id,const std::string& code,uint32_t now) { entries[id] = {code,now}; }
    int consume(const std::string& code,uint32_t now) {
        if (code.size() != 32) return 0;
        for (auto it=entries.begin(); it!=entries.end(); ++it) if (it->second.code == code) {
            const int id = now-it->second.created <= 300000 ? it->first : 0;
            entries.erase(it); return id;
        }
        return 0;
    }
};
