#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "quests.h"
#include "quest_data.h"
#include "ruleset_config.h"
#include "object.h"
#include "unit.h"
#include "testhelper.hpp"

namespace ut = boost::ut;
using namespace std;

namespace {

// --- Helpers ----------------------------------------------------------------

Unit *find_mayor_in_hall(ARegion *r) {
    for (const auto o : r->objects) {
        if (o->type != O_TOWN_HALL || o->incomplete > 0) continue;
        for (const auto u : o->units)
            if (u->type == U_MAYOR && u->GetMen() > 0) return u;
    }
    return nullptr;
}

Object *find_town_hall(ARegion *r) {
    for (const auto o : r->objects)
        if (o->type == O_TOWN_HALL && o->incomplete <= 0) return o;
    return nullptr;
}

// Spawn a completed Town Hall and move a fresh mayor into it, invested with his
// seal of office - CreateMayor spawns him bare and AdjustCityMon hands the
// cornucopia over a turn later, which quest generation requires.
// Returns {hall, mayor}.
pair<Object*, Unit*> setup_hall_with_mayor(UnitTestHelper& helper, ARegion *r) {
    helper.create_building(r, nullptr, O_TOWN_HALL);
    Object *hall = find_town_hall(r);
    if (hall) hall->incomplete = 0;
    helper.spawn_mayor(r, hall);
    Unit *mayor = find_mayor_in_hall(r);
    if (mayor) mayor->items.SetNum(I_CORNUCOPIA, 1);
    return {hall, mayor};
}

// Count active LOCAL quests issued by mayor_num.
int count_mayor_quests(int mayor_num) {
    int n = 0;
    for (const auto& q : quests)
        if (q->scope == Quest::SCOPE_LOCAL && q->issuer_unit == mayor_num) n++;
    return n;
}

// Hand-built LOCAL quest issued by mayor_num from city r, with a unique number.
// A road quest needs no monster target, so completion is driven directly.
shared_ptr<Quest> make_issued_road_quest(int num, int mayor_num, ARegion *r, int road_type) {
    auto q = make_shared<Quest>();
    q->num           = num;
    q->type          = -1;
    q->scope         = Quest::SCOPE_LOCAL;
    q->subtype       = Quest::LOCAL_BUILD_ROAD;
    q->tokens        = 1;
    q->issuer_unit   = mayor_num;
    q->issuer_region = r->num;
    q->created_turn  = 1;
    q->expires_turn  = 99;
    q->target        = -1;
    q->regionnum     = r->num;
    q->building      = road_type;
    q->regionname    = "Testburg";
    quests.push_back(q);
    return q;
}

} // anonymous namespace

// ---------------------------------------------------------------------------

ut::suite<"QuestGeneration"> quest_generation_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Test 1: No monsters in domain → no kill quests generated
    // (infrastructure quests like road/tower may still be created)
    // -----------------------------------------------------------------------
    "no candidates → no kill quests generated"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        auto [hall, mayor] = setup_hall_with_mayor(helper, r);
        expect(mayor != nullptr) << "mayor must exist";

        helper.run_generate_quests_for_mayor(r, mayor);

        int kill_quests = 0;
        for (const auto& q : quests) {
            if (q->scope == Quest::SCOPE_LOCAL && q->issuer_unit == mayor->num &&
                (q->subtype == Quest::LOCAL_HUNT || q->subtype == Quest::LOCAL_LAIR_CLEAR))
                kill_quests++;
        }
        expect(kill_quests == 0) << "no kill quests without monsters";
    };

    // -----------------------------------------------------------------------
    // Test 2: Wandering monster in domain → LOCAL_HUNT quest created
    // -----------------------------------------------------------------------
    "wandering monster → LOCAL_HUNT quest"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        auto [hall, mayor] = setup_hall_with_mayor(helper, r);
        expect(mayor != nullptr) << "mayor must exist";

        // Spawn a wolf (I_WOLF, tokens=1 in hunt_targets).
        Unit *wolf = helper.create_monster(r, I_WOLF, 5);

        helper.run_generate_quests_for_mayor(r, mayor);

        expect(count_mayor_quests(mayor->num) >= 1)
            << "should create at least 1 quest";

        // A LOCAL_HUNT quest must have been issued by this mayor (wolf or any
        // other domain monster — new_hunt_this_run limits to 1 per run, so the
        // wolf may not win the slot if other monsters exist in the test map).
        bool found_hunt = false;
        bool wolf_quest_valid = true;
        for (const auto& q : quests) {
            if (q->scope != Quest::SCOPE_LOCAL || q->subtype != Quest::LOCAL_HUNT) continue;
            if (q->issuer_unit != mayor->num) continue;
            found_hunt = true;
            if (q->target == wolf->num) {
                // Validate wolf quest fields.
                if (q->tokens <= 0) wolf_quest_valid = false;
                if (q->issuer_region != r->num) wolf_quest_valid = false;
                if (q->expires_turn != helper.turn_number() + ruleset_config().quests.generation.ttl)
                    wolf_quest_valid = false;
            }
        }
        expect(found_hunt) << "must find at least one LOCAL_HUNT from this mayor";
        expect(wolf_quest_valid) << "wolf quest fields must be valid if wolf was chosen";

        // Cleanup.
        r->Kill(wolf);
    };

    // -----------------------------------------------------------------------
    // Test 3: Mayor budget cap — max quests.generation.per_mayor quests per mayor
    // -----------------------------------------------------------------------
    "mayor budget capped at quests.generation.per_mayor"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        auto [hall, mayor] = setup_hall_with_mayor(helper, r);
        expect(mayor != nullptr) << "mayor must exist";

        // Spawn many monsters.
        for (int i = 0; i < 10; i++) helper.create_monster(r, I_WOLF, 3);

        helper.run_generate_quests_for_mayor(r, mayor);

        expect(count_mayor_quests(mayor->num) <= 3)
            << "must not exceed 3 quests";
    };

    // -----------------------------------------------------------------------
    // Test 4: No duplicate quests for same target unit
    // -----------------------------------------------------------------------
    "second generation pass does not duplicate quests"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        auto [hall, mayor] = setup_hall_with_mayor(helper, r);
        expect(mayor != nullptr) << "mayor must exist";

        helper.create_monster(r, I_WOLF, 3);

        helper.run_generate_quests_for_mayor(r, mayor);
        int after_first = count_mayor_quests(mayor->num);

        helper.run_generate_quests_for_mayor(r, mayor);
        int after_second = count_mayor_quests(mayor->num);

        expect(after_second == after_first)
            << "second pass must not add more quests if budget is full";
    };

    // -----------------------------------------------------------------------
    // Test 5: TTL expiry removes quests
    // -----------------------------------------------------------------------
    "expired quests are removed by ExpireLocalQuests"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        auto [hall, mayor] = setup_hall_with_mayor(helper, r);
        expect(mayor != nullptr) << "mayor must exist";

        helper.create_monster(r, I_DRAGON, 1);
        helper.run_generate_quests_for_mayor(r, mayor);
        expect(count_mayor_quests(mayor->num) >= 1) << "quest created";

        // Artificially set expires_turn to current turn.
        int cur = helper.turn_number();
        for (const auto& q : quests)
            if (q->scope == Quest::SCOPE_LOCAL)
                const_cast<Quest&>(*q).expires_turn = cur;

        helper.run_expire_local_quests();
        expect(count_mayor_quests(mayor->num) == 0)
            << "all expired quests must be removed";
    };

    // -----------------------------------------------------------------------
    // Test 6: Awareness — faction unit in region gains quest awareness
    // -----------------------------------------------------------------------
    "unit in issuer region gains quest awareness"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        auto [hall, mayor] = setup_hall_with_mayor(helper, r);
        expect(mayor != nullptr) << "mayor must exist";

        helper.create_monster(r, I_WOLF, 3);
        helper.run_generate_quests_for_mayor(r, mayor);
        expect(count_mayor_quests(mayor->num) >= 1) << "quest created";

        Faction *fac = helper.create_faction("Observer");
        Unit *obs = helper.create_unit(fac, r);

        helper.run_update_quest_awareness();

        bool has_awareness = false;
        for (const auto& q : quests) {
            if (q->scope == Quest::SCOPE_LOCAL &&
                fac->known_local_quests.count(q->num)) {
                has_awareness = true;
                break;
            }
        }
        expect(has_awareness) << "faction in region must gain quest awareness";
        (void)obs;
    };

    // -----------------------------------------------------------------------
    // Test 7: Awareness not gained for faction outside region
    // -----------------------------------------------------------------------
    "faction not in region does not gain awareness"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r     = helper.get_region(0, 0, 0);
        ARegion *other = helper.get_region(0, 2, 0);
        auto [hall, mayor] = setup_hall_with_mayor(helper, r);
        expect(mayor != nullptr) << "mayor must exist";

        helper.create_monster(r, I_WOLF, 3);
        helper.run_generate_quests_for_mayor(r, mayor);

        Faction *fac = helper.create_faction("Far Away");
        // SetupFaction puts a starting unit in regions.front() == r — kill it
        // so fac has NO unit in the quest's issuer region.
        Unit *start = helper.get_first_unit(fac);
        if (start) start->SetMen(I_LEADERS, 0);
        helper.create_unit(fac, other); // only unit: in a different region

        helper.run_update_quest_awareness();

        bool has_awareness = false;
        for (const auto& q : quests)
            if (fac->known_local_quests.count(q->num)) { has_awareness = true; break; }

        expect(!has_awareness) << "faction outside region must NOT gain awareness";
    };

    // -----------------------------------------------------------------------
    // Test 8: Mayor death erases LOCAL quests and cleans awareness
    // -----------------------------------------------------------------------
    "mayor death erases quests and purges awareness"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        auto [hall, mayor] = setup_hall_with_mayor(helper, r);
        expect(mayor != nullptr) << "mayor must exist";

        helper.create_monster(r, I_WOLF, 3);
        helper.run_generate_quests_for_mayor(r, mayor);
        expect(count_mayor_quests(mayor->num) >= 1) << "quests created";

        // Give a faction awareness.
        Faction *fac = helper.create_faction("Witness");
        helper.create_unit(fac, r);
        helper.run_update_quest_awareness();

        // Kill the mayor the way a battle does: his men drop to zero and
        // DeleteEmptyUnits takes the unit out of the region long before the
        // end-of-turn sweep runs.
        int mayor_num = mayor->num;
        mayor->SetMen(I_LEADERS, 0);
        helper.run_delete_empty_units();
        helper.run_expire_local_quests();

        expect(count_mayor_quests(mayor_num) == 0)
            << "all mayor quests must be erased on death";

        // Awareness must be cleaned from faction.
        bool still_aware = false;
        for (int qnum : fac->known_local_quests) {
            // Check if this quest still exists globally.
            for (const auto& q : quests)
                if (q->num == qnum) { still_aware = true; break; }
        }
        expect(!still_aware)
            << "faction awareness must be purged for erased quests";
    };

    // -----------------------------------------------------------------------
    // Test 8b: a quest completed in the turn its mayor died is still paid.
    //
    // The sweep runs at the end of the turn, after every player action: the
    // builder's completion (and the debt it records) happens first, and the
    // debt is untouched by the sweep. Quests nobody completed are erased.
    // -----------------------------------------------------------------------
    "quest completed in the turn the mayor died keeps its debt"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        auto [hall, mayor] = setup_hall_with_mayor(helper, r);
        expect(mayor != nullptr) << "mayor must exist";
        const int mayor_num = mayor->num;

        make_issued_road_quest(7001, mayor_num, r, O_ROADSE);   // will be completed
        make_issued_road_quest(7002, mayor_num, r, O_ROADNE);   // will not

        Faction *fac = helper.create_faction("Builders");
        Unit *builder = helper.create_unit(fac, r);

        // Combat phase: the mayor dies and his unit leaves the region.
        mayor->SetMen(I_LEADERS, 0);
        helper.run_delete_empty_units();

        // Month phase: the builder finishes the road after the mayor is dead.
        string rewards;
        int done = quests.check_road_quest(r, O_ROADSE, builder, &rewards, helper.get_regions());
        expect(done == 1) << "a dead mayor must not stop the quest from completing";
        expect(fac->quest_debts.count(r->num) > 0) << "debt must be recorded";
        expect(fac->quest_debts.at(r->num) == 1) << "debt owes the quest's token";

        // End of turn: the sweep removes what is left, and only that.
        helper.run_expire_local_quests();
        expect(count_mayor_quests(mayor_num) == 0)
            << "the unfinished quest of the dead mayor must be erased";
        expect(fac->quest_debts.count(r->num) > 0 && fac->quest_debts.at(r->num) == 1)
            << "the sweep must not touch faction debts";
        expect(builder->items.GetNum(I_BOUNTY) == 1) << "the builder keeps the token";
    };

    // -----------------------------------------------------------------------
    // Test 8c: the sweep drops only the dead mayor's quests.
    //
    // A living mayor outside the Town Hall (he stands in the dummy object) is a
    // mayor all the same; a replacement is a new unit and inherits nothing.
    // -----------------------------------------------------------------------
    "sweep spares a living mayor and drops his dead predecessor's quests"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        auto [hall, old_mayor] = setup_hall_with_mayor(helper, r);
        expect(old_mayor != nullptr) << "mayor must exist";
        const int old_num = old_mayor->num;

        // Replacement mayor in the dummy object (no empty hall to move into).
        old_mayor->SetMen(I_LEADERS, 0);
        helper.run_delete_empty_units();
        helper.spawn_mayor(r);
        Unit *new_mayor = nullptr;
        for (const auto o : r->objects)
            for (const auto u : o->units)
                if (u->type == U_MAYOR && u->GetMen() > 0) new_mayor = u;
        expect(new_mayor != nullptr) << "replacement mayor must exist";
        if (!new_mayor) return;
        expect(new_mayor->num != old_num) << "replacement is a new unit";

        make_issued_road_quest(7101, old_num, r, O_ROADSE);
        make_issued_road_quest(7102, new_mayor->num, r, O_ROADNE);

        helper.run_expire_local_quests();

        expect(count_mayor_quests(old_num) == 0)
            << "the predecessor's quest must be erased";
        expect(count_mayor_quests(new_mayor->num) == 1)
            << "the living mayor's quest must stay, wherever he stands";
    };

    // -----------------------------------------------------------------------
    // Test 9: ComputeMayorDomain includes city region
    // -----------------------------------------------------------------------
    "ComputeMayorDomain always includes city region"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        auto domain = helper.compute_mayor_domain(r);

        expect(domain.count(r->num) > 0u)
            << "domain must include the city's own region";
        expect(!domain.empty()) << "domain must not be empty";
    };

    // -----------------------------------------------------------------------
    // Test 10: expires_turn set correctly for regular LOCAL quest
    // -----------------------------------------------------------------------
    "regular LOCAL quest expires_turn = created_turn + TTL"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        auto [hall, mayor] = setup_hall_with_mayor(helper, r);
        expect(mayor != nullptr) << "mayor must exist";

        helper.create_monster(r, I_WOLF, 3);
        int turn_before = helper.turn_number();
        helper.run_generate_quests_for_mayor(r, mayor);

        for (const auto& q : quests) {
            if (q->scope != Quest::SCOPE_LOCAL) continue;
            expect(q->expires_turn == turn_before + ruleset_config().quests.generation.ttl)
                << "expires_turn must equal created_turn + quests.generation.ttl";
        }
    };

    // -----------------------------------------------------------------------
    // Test 11: only an invested mayor speaks for the town.
    //
    // CreateMayor seats him bare and AdjustCityMon hands over the cornucopia a
    // turn later (covered by the Mayor Town Hall suite), so a mayor who has just
    // replaced a killed predecessor holds no authority for one turn.
    // -----------------------------------------------------------------------
    "a mayor without his seal of office issues no quests"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 0, 0);
        helper.create_building(r, nullptr, O_TOWN_HALL);
        Object *hall = find_town_hall(r);
        if (hall) hall->incomplete = 0;
        helper.spawn_mayor(r, hall);

        Unit *mayor = find_mayor_in_hall(r);
        expect(mayor != nullptr) << "mayor must exist";
        if (!mayor) return;
        expect(mayor->items.GetNum(I_CORNUCOPIA) == 0_i) << "a fresh mayor is seated bare";

        // A candidate is available, so silence can only come from the gate.
        helper.create_monster(r, I_WOLF, 5);

        helper.run_generate_quests_for_mayor(r, mayor);
        expect(count_mayor_quests(mayor->num) == 0_i)
            << "an uninvested mayor must issue nothing";

        // Investiture opens the office.
        mayor->items.SetNum(I_CORNUCOPIA, 1);
        helper.run_generate_quests_for_mayor(r, mayor);
        expect(count_mayor_quests(mayor->num) > 0_i)
            << "once invested, the mayor issues quests again";
    };
};
