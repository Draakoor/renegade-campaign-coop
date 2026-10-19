// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <map>
#include <cstdint>

namespace Campaign {
enum Status { Pending = 0, Accomplished = 1, Failed = 2, Hidden = 3 };
enum class Outcome { Running, Success, Failure };
struct Objective {
    int type = 1, status = Hidden, title = 0, description = 0;
    bool announced = false;
};

// No engine pointers or persistent checkpoint saves. A new level gets a new State.
struct State {
    int mission = 0;
    std::map<int, Objective> objectives;
    uint32_t keys = 0;
    int checkpoint = 0;
    Outcome outcome = Outcome::Running;
    bool successRequested = false;

    explicit State(int number = 0) : mission(number) {}
    bool active() const { return mission == 6 || mission == 7 || mission == 9; }
    bool done(int id) const {
        auto it = objectives.find(id);
        return it != objectives.end() && it->second.status == Accomplished;
    }
    bool completedGoals() const {
        if (mission == 6) return done(601) && done(603) && done(604) && done(611);
        if (mission == 7) return done(701) && done(702) && done(703) && done(709) && done(710);
        if (mission == 9) return done(901) && done(902) && done(903);
        return false;
    }
    bool add(int id, int type, int status, int title, int description) {
        auto& obj = objectives[id];
        if (obj.announced) return false;
        // A delayed Add must not undo an accomplishment delivered earlier.
        if (obj.status != Accomplished && obj.status != Failed) obj.status = status;
        obj.type = type; obj.title = title; obj.description = description; obj.announced = true;
        return true;
    }
    bool set(int id, int status) {
        auto& obj = objectives[id];
        if (obj.status == status || obj.status == Accomplished || obj.status == Failed) return false;
        obj.status = status;
        evaluate();
        return true;
    }
    void evaluate() {
        if (outcome == Outcome::Running && successRequested && completedGoals()) outcome = Outcome::Success;
    }
    void finish(bool success) {
        if (outcome != Outcome::Running) return;
        if (!success) outcome = Outcome::Failure;
        else { successRequested = true; evaluate(); }
    }
    void grant(int key, bool value) {
        if (key < 0 || key >= 32) return;
        const uint32_t bit = uint32_t(1) << key;
        if (value) keys |= bit; else keys &= ~bit;
    }
};
}
