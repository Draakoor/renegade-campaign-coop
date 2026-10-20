// SPDX-License-Identifier: GPL-3.0-or-later
#include "../plugin/mission_state.h"
#include "../plugin/admin_requests.h"
#include "../plugin/coop_rules.h"
#include "../plugin/text_rules.h"
#include <limits>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>
using namespace Campaign;

static void permutations(int mission,std::vector<int> goals) {
    do {
        State s(mission);
        s.finish(true); // An early boss kill must not bypass the escort/hack.
        assert(s.outcome == Outcome::Running);
        for (size_t i=0; i<goals.size(); ++i) {
            s.set(goals[i],Accomplished);
            s.add(goals[i],1,Pending,123,124); // Delayed duplicate events.
            assert(s.done(goals[i]));
            assert(s.outcome == (i+1==goals.size() ? Outcome::Success : Outcome::Running));
        }
        s.finish(true);
        assert(s.outcome == Outcome::Success);
    } while (std::next_permutation(goals.begin(),goals.end()));
}
int main() {
    int credits=0;
    for (const char* bad : {"", "0", "-1", "+10", "1.5", "NaN", "inf", " 10", "10 ", "1e6", "1000001", "99999999"})
        assert(!CoopRules::Credits(bad,credits));
    assert(CoopRules::Credits("1000000",credits) && credits==1000000);
    assert(CoopRules::Credits("001",credits) && credits==1);
    CoopRules::Transfer transfer;
    assert(CoopRules::PlanTransfer({1,1000.5f},{{2,10.25f},{3,20.f},{4,30.f}},100,transfer));
    assert(transfer.each==33 && transfer.total==99 && transfer.donorBalance==901.5f);
    double sum=transfer.donorBalance;
    for (auto a : transfer.recipients) sum+=a.balance;
    assert(sum==1060.75); // No credits minted; division remainder stays with donor.
    const float nan=std::numeric_limits<float>::quiet_NaN();
    assert(!CoopRules::PlanTransfer({1,100},{},10,transfer));
    assert(!CoopRules::PlanTransfer({1,100},{{1,100}},10,transfer));
    assert(!CoopRules::PlanTransfer({1,100},{{2,100},{2,100}},10,transfer));
    assert(!CoopRules::PlanTransfer({1,9},{{2,0}},10,transfer));
    assert(!CoopRules::PlanTransfer({1,nan},{{2,0}},10,transfer));
    assert(!CoopRules::PlanTransfer({1,100},{{2,nan}},10,transfer));
    assert(!CoopRules::PlanTransfer({1,100},{{2,16000000}},10,transfer));
    assert(!CoopRules::PlanTransfer({1,100},{{2,0},{3,0}},1,transfer));
    assert(transfer.total==99); // Rejections do not partially overwrite the plan.
    const std::vector<std::string> maps={"M01","M02","M03","M04","M05","M06","M07","M08","M09","M10","M11"};
    for (int current=0; current<11; ++current) for (int wanted=0; wanted<11; ++wanted) {
        const auto rotated=TextRules::Rotate(maps,current,wanted);
        for (int step=0; step<11; ++step)
            assert(rotated[(current+1+step)%11]==maps[(wanted+step)%11]);
    }
    assert(TextRules::Rotate(maps,-1,1).empty());
    assert(TextRules::Rotate(maps,0,11).empty());
    assert(TextRules::MapName("M06.mix")=="m06");
    assert(CoopRules::Elapsed(20,0xfffffff0u,30) && !CoopRules::Elapsed(20,0xfffffff0u,40));
    AdminRequests admin;
    const std::string token(32,'a'),other(32,'b');
    admin.issue(1,token,100);
    assert(admin.consume(other,200) == 0);
    assert(admin.consume(token,200) == 1);
    assert(admin.consume(token,201) == 0); // No replay.
    admin.issue(1,token,100);
    admin.disconnect(1); // A replacement connection using the same ID/name.
    admin.issue(1,other,110);
    assert(admin.consume(token,120) == 0);
    assert(admin.consume(other,120) == 1);
    admin.issue(1,token,100);
    assert(admin.consume(token,300101) == 0);
    admin.issue(1,token,0xfffffff0u);
    assert(admin.consume(token,20) == 1); // Uptime counter rollover.
    permutations(6,{601,603,604,611});
    permutations(7,{701,702,703,709,710});
    permutations(9,{901,902,903});
    State s(9);
    s.add(901,1,Pending,1,2);
    s.set(901,Accomplished);
    s.finish(false); s.finish(true); s.set(902,Accomplished); s.set(903,Accomplished);
    assert(s.outcome == Outcome::Failure); // A late success cannot erase escort death.
    s.grant(3,true); s.grant(10,true); s.grant(10,false);
    assert(s.keys == (1u<<3));
    s.grant(-1,true); s.grant(32,true);
    assert(s.keys == (1u<<3));
    s = State(9); // Reload discards objectives and checkpoints.
    assert(s.keys == 0 && s.checkpoint == 0 && s.objectives.empty() && s.outcome == Outcome::Running);
    s.set(900,Failed); s.add(900,1,Pending,1,2);
    assert(s.objectives[900].status == Failed);
    assert(!State(8).active() && State(6).active());
    std::cout << "150 completion orderings, failure latch, duplicate events, key revocation and map reset passed.\n";
    std::cout << "Credit validation/conservation, rejected transfers, all 121 map changes and cooldown rollover passed.\n";
}
