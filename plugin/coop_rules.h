// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace CoopRules {
inline bool Credits(const std::string& text,int& amount) {
    if (text.empty() || text.size() > 7) return false;
    int value=0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        value=value*10+c-'0';
    }
    if (value < 1 || value > 1000000) return false;
    amount=value; return true;
}
struct Account { int id; float balance; };
struct Transfer { float donorBalance=0; int total=0,each=0; std::vector<Account> recipients; };
inline bool PlanTransfer(Account donor,const std::vector<Account>& recipients,int requested,Transfer& out) {
    if (requested < 1 || requested > 1000000 || recipients.empty() || recipients.size() > 127 ||
        !std::isfinite(donor.balance) || donor.balance < requested || donor.balance > 16000000) return false;
    Transfer plan;
    plan.each=requested/static_cast<int>(recipients.size());
    if (!plan.each) return false;
    plan.total=plan.each*static_cast<int>(recipients.size());
    plan.donorBalance=donor.balance-plan.total;
    std::set<int> ids{donor.id};
    for (auto a : recipients) {
        if (a.id < 1 || !ids.insert(a.id).second || !std::isfinite(a.balance) ||
            a.balance < 0 || a.balance > 16000000-plan.each) return false;
        a.balance+=plan.each; plan.recipients.push_back(a);
    }
    out=plan; return true;
}
inline bool Elapsed(uint32_t now,uint32_t since,uint32_t duration) { return now-since >= duration; }
}
