// unittest/battle_json_test.cpp
// Tests for the structured battle fields in report.N.json (Battle::build_json_report).
// Spec: docs/report-format/BATTLE_REPORT_DESIGN.md, schema: docs/report-format/report.schema.json.
//
// These cover the reporting contract only. The battle itself - who fights, who dies, what
// drops - is deliberately not asserted here; the structured fields must describe the fight,
// never change it.

#include "external/boost/ut.hpp"

#include "battle.h"
#include "game.h"
#include "gamedefs.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "unit.h"
#include "quests.h"
#include "quest_data.h"
#include "items.h"

#include <optional>
#include "testhelper.hpp"

namespace ut = boost::ut;
using namespace std;

namespace {

// Overwhelming force, so the fight resolves without the target surviving on a lucky roll.
Unit *create_attacker(UnitTestHelper &helper, ARegion *r, const string& name)
{
    Faction *player = helper.create_faction(name);
    Unit *u = helper.create_unit(player, r);
    u->items.SetNum(I_LEADERS, 200);
    u->items.SetNum(I_MSWORD, 200);
    helper.set_skill_level(u, S_COMBAT, 5);
    return u;
}

// The battle whose opening line names `u`, or null. RunMovementOrders sweeps the whole map,
// so a test must pick its own fight out of whatever else the world did this turn.
Battle *battle_of(UnitTestHelper &helper, Unit *u)
{
    const string tag = "(" + to_string(u->num) + ")";
    for (auto b : helper.game_object().battles) {
        if (!b->text.empty() && b->text[0].find(tag) != string::npos) return b;
    }
    return nullptr;
}

// unittest/rules.cpp sets BATTLE_FACTION_INFO = 0 while neworigins sets 1, so a test states
// the value it needs and puts it back afterwards.
struct ScopedFactionInfo {
    int saved;
    explicit ScopedFactionInfo(int value) : saved(Globals->BATTLE_FACTION_INFO) {
        Globals->BATTLE_FACTION_INFO = value;
    }
    ~ScopedFactionInfo() { Globals->BATTLE_FACTION_INFO = saved; }
};

// Whether the roster entry for `u` on the attacking side carries a faction.
// Returns nullopt when the unit is missing from the roster entirely.
optional<bool> attacker_has_faction(const json& j, int unit_num)
{
    for (const auto& u : j["attackers"]) {
        if (u["number"] == unit_num) return u.contains("faction");
    }
    return nullopt;
}

// A minimal LOCAL_HUNT quest on `target_num`, pushed to the global list.
shared_ptr<Quest> make_hunt_quest(int target_num, int issuer_region, int tokens)
{
    auto q = make_shared<Quest>();
    q->num           = 999;
    q->type          = -1;
    q->scope         = Quest::SCOPE_LOCAL;
    q->subtype       = Quest::LOCAL_HUNT;
    q->tokens        = tokens;
    q->issuer_unit   = 0;
    q->issuer_region = issuer_region;
    q->created_turn  = 1;
    q->expires_turn  = 13;
    q->target        = target_num;
    q->regionname    = "Testburg";
    quests.push_back(q);
    return q;
}

} // namespace

ut::suite<"BattleJson"> battle_json_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // phase: a fight started by entering an object belongs to the movement
    // phase that was running, not to the pre-movement block.
    //
    // RunMovementOrders opens each phase body with an Enter sweep, and
    // DoMoveEnter can call RunBattle from it. While the phase was claimed just
    // before DoMovementAttacks at the end of the body, those battles were
    // stamped with the previous phase - or with "before_movement" on phase 0.
    // -----------------------------------------------------------------------
    "an enter battle is stamped with the movement phase it ran in"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        expect(r != nullptr) << "test bootstrap needs a region";
        if (!r) return;
        expect(r->IsSafeRegion() == 0_i) << "battles must be allowed here";
        helper.clear_npc_units_in_region(r);

        // The holder refuses entry: default attitude is below FRIENDLY, which is all
        // Object::ForbiddenBy asks for.
        Faction *holder_fac = helper.create_faction("Holder");
        Unit *holder = helper.create_unit(holder_fac, r);
        holder->items.SetNum(I_LEADERS, 1);
        helper.create_building(r, holder, O_FORT);
        Object *fort = holder->object;
        expect(fort != nullptr && fort->type == O_FORT) << "holder must own the fort";
        if (!fort) return;

        Unit *mover = create_attacker(helper, r, "Mover");

        // ADVANCE <object number> is the only way in: plain MOVE is refused outright
        // (DoMoveEnter checks o->advancing before it will fight).
        stringstream ss;
        ss << "#atlantis " << mover->faction->num << " \"mypassword\"\n";
        ss << "unit " << mover->num << "\n";
        ss << "advance " << fort->num << "\n";
        helper.parse_orders(mover->faction->num, ss, nullptr);

        helper.move_units();

        Battle *b = battle_of(helper, mover);
        expect(b != nullptr) << "entering a forbidden object must start a battle";
        if (!b) return;

        json j;
        b->build_json_report(j, mover->faction);
        expect(j["phase"]["timing"] == "movement")
            << "an enter battle happens during movement, not before it";
        expect(j["phase"]["index"] == 0)
            << "it ran in the first movement phase";
        expect(j["trigger"] == "auto_attack")
            << "entering a forbidden object is not an ATTACK order";
    };

    // -----------------------------------------------------------------------
    // quest.tokens is the quest's own grant. `spoils` is a shared pot that also
    // carries looted tokens and the grants of any earlier target, so reading
    // I_BOUNTY out of it over-reports.
    //
    // The injection into `spoils` must stay exactly as it was: that is the
    // reward economy, not reporting.
    // -----------------------------------------------------------------------
    "check_kill_target reports its own grant, not the bounty pot"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        Faction *mf = helper.get_faction(helper.get_monfaction());
        Unit *monster = helper.create_unit(mf, r);
        monster->type = U_WMON;
        monster->items.SetNum(I_WOLF, 3);

        make_hunt_quest(monster->num, r->num, 2);

        ItemList spoils;
        spoils.SetNum(I_BOUNTY, 7);   // the pot is not empty when the target dies

        string rewards;
        int tokens = -1;
        int result = quests.check_kill_target(monster, spoils, &rewards,
                                              nullptr, nullptr, nullptr, nullptr,
                                              nullptr, &tokens);

        expect(result == 1) << "the quest must match";
        expect(tokens == 2) << "out_tokens is this quest's grant, not the pot";
        expect(spoils.GetNum(I_BOUNTY) == 9)
            << "the grant must still be injected into spoils: 7 already there + 2 granted";
    };

    // -----------------------------------------------------------------------
    // The same value, end to end through a real battle.
    // -----------------------------------------------------------------------
    "a battle's quest block carries the grant"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        helper.clear_npc_units_in_region(r);

        Faction *mf = helper.get_faction(helper.get_monfaction());
        Unit *monster = helper.create_unit(mf, r);
        monster->type = U_WMON;
        monster->items.SetNum(I_WOLF, 2);

        make_hunt_quest(monster->num, r->num, 3);

        Unit *hunter = create_attacker(helper, r, "Hunters");
        expect(helper.run_battle(r, hunter, monster) == BATTLE_WON) << "the hunter must win";

        Battle *b = battle_of(helper, hunter);
        expect(b != nullptr) << "the battle must be recorded";
        if (!b) return;

        json j;
        b->build_json_report(j, hunter->faction);
        expect(j.contains("quest")) << "killing a bounty target must emit the quest block";
        if (!j.contains("quest")) return;
        expect(j["quest"]["tokens"] == 3) << "tokens is the quest's grant";
        expect(j["quest"]["num"] == 999) << "and it describes that same quest";
    };

    // -----------------------------------------------------------------------
    // A roster entry names a faction only when the prose does. The same Battle
    // goes to every faction that saw the fight, so a structured field that
    // ignores the disclosure gate hands out what the text deliberately hides -
    // an assassin's colours above all.
    //
    // Two gates, both exercised here: the ruleset-level BATTLE_FACTION_INFO and,
    // under it, the stealth-versus-observation check in Unit::get_name.
    // -----------------------------------------------------------------------

    // A fight where the attacker out-stealths every defender.
    "a unit the defenders cannot identify keeps its faction hidden"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ScopedFactionInfo faction_info(1);   // after setup, so nothing resets it

        ARegion *r = helper.get_region(0, 2, 0);
        helper.clear_npc_units_in_region(r);

        Faction *df = helper.create_faction("Blind");
        Unit *target = helper.create_unit(df, r);
        target->items.SetNum(I_LEADERS, 1);   // no observation skill: attribute stays 0

        Unit *sneak = create_attacker(helper, r, "Sneaks");
        helper.set_skill_level(sneak, S_STEALTH, 3);

        expect(helper.run_battle(r, sneak, target) == BATTLE_WON) << "the attacker must win";
        Battle *b = battle_of(helper, sneak);
        expect(b != nullptr) << "the battle must be recorded";
        if (!b) return;

        json j;
        b->build_json_report(j, df);
        auto has = attacker_has_faction(j, sneak->num);
        expect(has.has_value()) << "the attacker must appear in the roster";
        if (has) expect(*has == false)
            << "observation 0 does not beat stealth 3, so the faction stays hidden";
    };

    // The mirror image: the defenders see through the attacker.
    "an identified unit carries its faction"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ScopedFactionInfo faction_info(1);   // after setup, so nothing resets it

        ARegion *r = helper.get_region(0, 2, 0);
        helper.clear_npc_units_in_region(r);

        Faction *df = helper.create_faction("Watchers");
        Unit *target = helper.create_unit(df, r);
        target->items.SetNum(I_LEADERS, 1);
        helper.set_skill_level(target, S_OBSERVATION, 3);

        Unit *open = create_attacker(helper, r, "Openly");   // no stealth skill

        expect(helper.run_battle(r, open, target) == BATTLE_WON) << "the attacker must win";
        Battle *b = battle_of(helper, open);
        expect(b != nullptr) << "the battle must be recorded";
        if (!b) return;

        json j;
        b->build_json_report(j, df);
        auto has = attacker_has_faction(j, open->num);
        expect(has.has_value()) << "the attacker must appear in the roster";
        if (has) expect(*has == true)
            << "observation 3 beats stealth 0, so the faction is named";

        if (has && *has) {
            for (const auto& u : j["attackers"])
                if (u["number"] == open->num)
                    expect(u["faction"]["number"] == open->faction->num)
                        << "and it must be the right faction";
        }
    };

    // The ruleset gate sits above the stealth gate: with faction info off, the prose
    // prints a bare unit name and the structured field must follow.
    "no faction is reported when the ruleset hides battle faction info"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        ScopedFactionInfo faction_info(0);   // after setup, so nothing resets it

        ARegion *r = helper.get_region(0, 2, 0);
        helper.clear_npc_units_in_region(r);

        Faction *df = helper.create_faction("Watchers");
        Unit *target = helper.create_unit(df, r);
        target->items.SetNum(I_LEADERS, 1);
        helper.set_skill_level(target, S_OBSERVATION, 3);   // would identify it, if allowed

        Unit *open = create_attacker(helper, r, "Openly");

        expect(helper.run_battle(r, open, target) == BATTLE_WON) << "the attacker must win";
        Battle *b = battle_of(helper, open);
        expect(b != nullptr) << "the battle must be recorded";
        if (!b) return;

        json j;
        b->build_json_report(j, df);
        auto has = attacker_has_faction(j, open->num);
        expect(has.has_value()) << "the attacker must appear in the roster";
        if (has) expect(*has == false)
            << "BATTLE_FACTION_INFO = 0 hides the faction whatever the observation";
    };
};
