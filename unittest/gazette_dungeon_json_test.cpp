#include "external/boost/ut.hpp"
#include "external/nlohmann/json.hpp"
#include "game.h"
#include "gamedata.h"
#include "dungeon.h"
#include "aregion.h"
#include "object.h"
#include "testhelper.hpp"

#include <filesystem>
#include <fstream>
#include <list>
#include <sstream>
#include <string>

namespace ut = boost::ut;
using namespace std;

// ---------------------------------------------------------------------------
// Gazette JSON — dungeon events, dungeon register, and per-type settlement
// breakdown. Times JSON gains three additive keys:
//   - events[].highlight       (dungeon events: the type and surface region)
//   - "dungeons"               (every non-pirate dungeon still alive)
//   - settlement_stats["uncontrolled_by_type"] / ["contested_by_type"]
// See docs/plans/2026-09-18-gazette-dungeon-registry.md.
// ---------------------------------------------------------------------------

namespace {
    const char *dungeon_subtype(DungeonEventType t) {
        switch (t) {
            case DungeonEventType::SPAWN:       return "spawn";
            case DungeonEventType::BOSS_KILLED: return "boss_killed";
            case DungeonEventType::COLLAPSING:  return "collapsing";
            case DungeonEventType::DECAYING:    return "decaying";
        }
        return "";
    }

    DungeonInstance make_registry_dungeon(DungeonType type, DungeonSlotState state,
                                          int surface_num) {
        DungeonInstance d;
        d.id = 999;
        d.type = type;
        d.state = state;
        d.surface_region_num = surface_num;
        return d;
    }
}

ut::suite<"GazetteDungeonJson"> gazette_dungeon_json_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Every dungeon phase produces an event whose subtype is the phase name,
    // whose highlight is exactly {type, region}, and whose text contains both
    // phrases (so the portal can find them and set them in bold).
    // -----------------------------------------------------------------------
    "dungeon events carry subtype and highlight for every phase"_test = [] {
        const char *type = "Skeleton Ruins";
        const char *region = "Greywood";
        DungeonEventType phases[] = {
            DungeonEventType::SPAWN, DungeonEventType::BOSS_KILLED,
            DungeonEventType::COLLAPSING, DungeonEventType::DECAYING
        };
        for (auto phase : phases) {
            for (int i = 0; i < 40; i++) {
                DungeonFact f;
                f.event_type = phase;
                f.dungeon_type_name = type;
                f.region_name = region;
                list<Event> events;
                f.GetEvents(events);

                expect(events.size() == 1_ul);
                const Event &e = events.front();
                expect(e.subtype == dungeon_subtype(phase));
                expect(e.highlight.size() == 2_ul);
                expect(e.highlight[0] == type);
                expect(e.highlight[1] == region);
                expect(e.text.find(type) != string::npos);
                expect(e.text.find(region) != string::npos);
            }
        }
    };

    // -----------------------------------------------------------------------
    // WriteJSON emits "highlight" for dungeon events and leaves it off other
    // event categories.
    // -----------------------------------------------------------------------
    "WriteJSON emits highlight for dungeon events and omits it elsewhere"_test = [] {
        Events events;
        auto *df = new DungeonFact();
        df->event_type = DungeonEventType::SPAWN;
        df->dungeon_type_name = "Demon Pit";
        df->region_name = "Ashenvale";
        events.AddFact(df);
        auto *pf = new PirateSightingFact();
        pf->ship_name = "Black Galleon";
        pf->captain_name = "Captain Cormorant";
        pf->terrain_name = "ocean";
        pf->region_name = "The Deep";
        events.AddFact(pf);

        string out = events.WriteJSON("World", "January", 1, {}, {}, {});
        json j = json::parse(out);

        const json *dungeon_ev = nullptr;
        const json *pirate_ev = nullptr;
        for (const auto &e : j["events"]) {
            string cat = e.value("category", string());
            if (cat == "dungeon") dungeon_ev = &e;
            if (cat == "pirate_sighting") pirate_ev = &e;
        }

        expect(dungeon_ev != nullptr);
        expect(pirate_ev != nullptr);
        if (dungeon_ev) {
            expect(dungeon_ev->contains("highlight"));
            expect((*dungeon_ev)["highlight"].size() == 2_ul);
            expect((*dungeon_ev)["highlight"][0] == "Demon Pit");
            expect((*dungeon_ev)["highlight"][1] == "Ashenvale");
        }
        if (pirate_ev) {
            expect(!pirate_ev->contains("highlight"));
        }
    };

    // -----------------------------------------------------------------------
    // The "dungeons" block lists ACTIVE ("open") before DYING ("closing"),
    // and skips COLLAPSING slots and pirate hideouts.
    // -----------------------------------------------------------------------
    "dungeons block lists open before closing and skips collapsed and hideouts"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r0 = helper.get_region(0, 0, 0);
        ARegion *r1 = helper.get_region(0, 2, 0);
        ARegion *r2 = helper.get_region(1, 1, 0);
        ARegion *r3 = helper.get_region(1, 3, 0);

        helper.inject_dungeon(make_registry_dungeon(DungeonType::DUNGEON_DRAGON_LAIR,
                                                    DungeonSlotState::ACTIVE, r0->num));
        helper.inject_dungeon(make_registry_dungeon(DungeonType::DUNGEON_SKELETON_RUINS,
                                                    DungeonSlotState::DYING, r1->num));
        helper.inject_dungeon(make_registry_dungeon(DungeonType::DUNGEON_KOBOLD_WARRENS,
                                                    DungeonSlotState::COLLAPSING, r2->num));
        helper.inject_dungeon(make_registry_dungeon(DungeonType::DUNGEON_PIRATE_HIDEOUT,
                                                    DungeonSlotState::ACTIVE, r3->num));

        json dungeons = helper.build_dungeons_json();

        expect(dungeons.is_array());
        expect(dungeons.size() == 2_ul) << "collapsed and pirate-hideout slots are skipped";

        expect(dungeons[0].value("type", string()) == "Dragon Lair");
        expect(dungeons[0].value("region", string()) == r0->name);
        expect(dungeons[0].value("state", string()) == "open");
        expect(dungeons[1].value("type", string()) == "Skeleton Ruins");
        expect(dungeons[1].value("region", string()) == r1->name);
        expect(dungeons[1].value("state", string()) == "closing");
    };

    // -----------------------------------------------------------------------
    // With no live dungeons the block is still written, as an empty array.
    // -----------------------------------------------------------------------
    "dungeons block is an empty array when nothing is alive"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        json dungeons = helper.build_dungeons_json();

        expect(dungeons.is_array());
        expect(dungeons.empty());
    };

    // -----------------------------------------------------------------------
    // settlement_stats gains per-type uncontrolled/contested breakdowns that
    // partition the world exactly: total == owned + uncontrolled + contested.
    // -----------------------------------------------------------------------
    "settlement_stats breaks down uncontrolled and contested by type"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *city    = helper.get_region(0, 0, 0);
        ARegion *town    = helper.get_region(0, 2, 0);
        ARegion *village = helper.get_region(1, 1, 0);

        // Wipe NPC guards/monsters so only the factions below guard anything.
        helper.clear_npc_units_in_region(city);
        helper.clear_npc_units_in_region(town);
        helper.clear_npc_units_in_region(village);

        // Owned settlement: the starting city, guarded by one player faction.
        Faction *solo = helper.create_faction("Solo Keepers");
        Unit *solo_guard = helper.get_first_unit(solo);
        solo_guard->guard = GUARD_GUARD;

        // Contested settlement: a town guarded by two player factions.
        town->add_town(TOWN_TOWN);
        Faction *red = helper.create_faction("Red Banner");
        Unit *red_guard = helper.create_unit(red, town);
        red_guard->guard = GUARD_GUARD;
        Faction *blue = helper.create_faction("Blue Banner");
        Unit *blue_guard = helper.create_unit(blue, town);
        blue_guard->guard = GUARD_GUARD;

        // Unguarded settlement: a village with no guarders at all.
        village->add_town(TOWN_VILLAGE);

        // A distinctive turn so this test only ever touches its own files.
        helper.game_object().year = 777;
        helper.game_object().month = 0;
        int turn = helper.turn_number();
        string prefix = "times." + to_string(turn) + ".";

        // Remove any stale files from a previous crashed run.
        for (auto const &entry : filesystem::directory_iterator(".")) {
            string name = entry.path().filename().string();
            if (name.rfind(prefix, 0) == 0) filesystem::remove(entry.path());
        }

        helper.game_object().WriteWorldEvents();

        // Locate the JSON newspaper written for this distinctive turn.
        string json_path;
        for (auto const &entry : filesystem::directory_iterator(".")) {
            string name = entry.path().filename().string();
            if (name.rfind(prefix, 0) == 0 && name.size() > 5 &&
                name.substr(name.size() - 5) == ".json") {
                json_path = entry.path().string();
                break;
            }
        }
        expect(!json_path.empty()) << "WriteWorldEvents must write a times JSON file";
        if (json_path.empty()) return;

        json j;
        {
            ifstream f(json_path);
            stringstream ss;
            ss << f.rdbuf();
            j = json::parse(ss.str());
        }

        // Remove the files written by this test.
        for (auto const &entry : filesystem::directory_iterator(".")) {
            string name = entry.path().filename().string();
            if (name.rfind(prefix, 0) == 0) filesystem::remove(entry.path());
        }

        const json &stats = j["settlement_stats"];
        expect(stats.value("total", 0) == 3) << "three settlements in total";
        expect(stats.value("uncontrolled", 0) == 1) << "one settlement is uncontrolled";
        expect(stats.value("contested", 0) == 1) << "one settlement is contested";

        const json &u = stats["uncontrolled_by_type"];
        const json &c = stats["contested_by_type"];
        const json &t = stats["total_by_type"];

        // The specific scenario: village unguarded, town contested, city owned.
        expect(u.value("villages", 0) == 1);
        expect(u.value("towns", 0) == 0);
        expect(u.value("cities", 0) == 0);
        expect(c.value("villages", 0) == 0);
        expect(c.value("towns", 0) == 1);
        expect(c.value("cities", 0) == 0);

        // Invariant: per-type sums equal the aggregate counts.
        int u_sum = u.value("villages", 0) + u.value("towns", 0) + u.value("cities", 0);
        int c_sum = c.value("villages", 0) + c.value("towns", 0) + c.value("cities", 0);
        expect(u_sum == stats.value("uncontrolled", 0));
        expect(c_sum == stats.value("contested", 0));

        // Invariant: total_by_type == owned + uncontrolled + contested per type.
        int owned_v = 0, owned_t = 0, owned_c = 0;
        for (const auto &o : stats["all_owners"]) {
            owned_v += o.value("villages", 0);
            owned_t += o.value("towns", 0);
            owned_c += o.value("cities", 0);
        }
        expect(t.value("villages", 0) == owned_v + u.value("villages", 0) + c.value("villages", 0));
        expect(t.value("towns", 0)    == owned_t + u.value("towns", 0)    + c.value("towns", 0));
        expect(t.value("cities", 0)   == owned_c + u.value("cities", 0)   + c.value("cities", 0));
        expect(owned_v == 0 && owned_t == 0 && owned_c == 1)
            << "the city is the only owned settlement";
    };
};
