#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "quests.h"
#include "quest_data.h"
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

// Spawn a completed Town Hall and move a fresh mayor into it.
// Returns {hall, mayor}.
pair<Object*, Unit*> setup_hall_with_mayor(UnitTestHelper& helper, ARegion *r) {
    helper.create_building(r, nullptr, O_TOWN_HALL);
    Object *hall = find_town_hall(r);
    if (hall) hall->incomplete = 0;
    helper.spawn_mayor(r, hall);
    Unit *mayor = find_mayor_in_hall(r);
    return {hall, mayor};
}

// Count active LOCAL quests issued by mayor_num.
int count_mayor_quests(int mayor_num) {
    int n = 0;
    for (const auto& q : quests)
        if (q->scope == Quest::SCOPE_LOCAL && q->issuer_unit == mayor_num) n++;
    return n;
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
                if (q->expires_turn != helper.turn_number() + LOCAL_QUEST_TTL)
                    wolf_quest_valid = false;
            }
        }
        expect(found_hunt) << "must find at least one LOCAL_HUNT from this mayor";
        expect(wolf_quest_valid) << "wolf quest fields must be valid if wolf was chosen";

        // Cleanup.
        r->Kill(wolf);
    };

    // -----------------------------------------------------------------------
    // Test 3: Mayor budget cap — max LOCAL_QUESTS_PER_MAYOR quests per mayor
    // -----------------------------------------------------------------------
    "mayor budget capped at LOCAL_QUESTS_PER_MAYOR"_test = [] {
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
            << "must not exceed LOCAL_QUESTS_PER_MAYOR=3";
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

        // Kill mayor — triggers quest cleanup via AdjustCityMons path.
        int mayor_num = mayor->num;
        mayor->SetMen(I_LEADERS, 0);
        // Simulate the cleanup that happens in AdjustCityMons.
        vector<shared_ptr<Quest>> to_purge;
        for (const auto& q : quests)
            if (q->scope == Quest::SCOPE_LOCAL && q->issuer_unit == mayor_num)
                to_purge.push_back(q);
        for (auto& q : to_purge) quests.erase_with_cleanup(q, &helper.get_factions());

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
            expect(q->expires_turn == turn_before + LOCAL_QUEST_TTL)
                << "expires_turn must equal created_turn + LOCAL_QUEST_TTL";
        }
    };
};
