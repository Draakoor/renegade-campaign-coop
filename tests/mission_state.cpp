// SPDX-License-Identifier: GPL-3.0-or-later
#include "../plugin/mission_state.h"
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
}
