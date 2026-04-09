#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "namegen.h"
#include "testhelper.hpp"
#include "string_filters.hpp"

namespace ut = boost::ut;

// Tests for the unit auto-naming system (AutoNameSoloUnits + getPersonName).
// Design doc: docs/UNIT_NAMING_SYSTEM.md
// Trigger: solo player units with default name "Unit (N)" get a race-appropriate name.
ut::suite<"Unit Auto-Naming"> unit_naming_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // getPersonName: returns non-empty string for each enabled race
    // -----------------------------------------------------------------------
    "getPersonName returns non-empty for each enabled race"_test = [] {
        const std::vector<int> enabled_races = {
            I_MAN, I_ORC, I_HILLDWARF, I_HIGHELF, I_WOODELF, I_HOBBIT,
            I_LEADERS, I_DROWMAN, I_GNOME, I_ICEDWARF, I_UNDERDWARF,
            I_GOBLINMAN, I_LIZARDMAN, I_CENTAURMAN,
            I_FAIRY, I_TIEFLING
        };
        for (int race : enabled_races) {
            std::string name = getPersonName(race);
            expect(!name.empty()) << "Expected non-empty name for race item " << race;
        }
    };

    "getPersonName returns non-empty for unknown race (abstract fallback)"_test = [] {
        // Unknown races now fall back to getAbstractName() instead of returning ""
        expect(!getPersonName(-1).empty());
        expect(!getPersonName(9999).empty());
    };

    // Each enabled race should produce at least 3 distinct names (basic variety check)
    "getPersonName produces varied output for each race"_test = [] {
        const std::vector<int> races = {
            I_MAN, I_ORC, I_HILLDWARF, I_HIGHELF, I_WOODELF, I_HOBBIT,
            I_LEADERS, I_DROWMAN, I_GNOME, I_ICEDWARF, I_UNDERDWARF,
            I_GOBLINMAN, I_LIZARDMAN, I_CENTAURMAN,
            I_FAIRY, I_TIEFLING
        };
        for (int race : races) {
            std::unordered_set<std::string> names;
            for (int i = 0; i < 50; i++) names.insert(getPersonName(race));
            expect(names.size() >= 3_ul)
                << "Race " << race << " produced only " << names.size() << " distinct names in 50 tries";
        }
    };

    // -----------------------------------------------------------------------
    // AutoNameSoloUnits: renames exactly-one-person "Unit (N)" player units
    // -----------------------------------------------------------------------
    "AutoNameSoloUnits renames a solo leader unit with default name"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test Faction");
        Unit *u = helper.get_first_unit(faction);

        // Default state: 1 I_LEADERS, name is "Unit (N)"
        expect((u->name | filter::strip_number) == std::string("Unit"))
            << "Precondition: unit should have default name, got: " << u->name;

        helper.run_auto_name_solo_units();

        expect((u->name | filter::strip_number) != std::string("Unit"))
            << "Unit should be renamed after AutoNameSoloUnits, got: " << u->name;
        expect(!u->name.empty());
    };

    "AutoNameSoloUnits renames a solo non-leader race unit"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test Faction");
        Unit *u = helper.get_first_unit(faction);
        // Replace the default I_LEADERS with a single I_ORC
        u->items.SetNum(I_LEADERS, 0);
        u->items.SetNum(I_ORC, 1);

        helper.run_auto_name_solo_units();

        expect((u->name | filter::strip_number) != std::string("Unit"))
            << "Orc unit should be renamed, got: " << u->name;
    };

    "AutoNameSoloUnits does NOT rename a multi-person unit"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test Faction");
        Unit *u = helper.get_first_unit(faction);
        u->items.SetNum(I_LEADERS, 2);  // 2 people — should not trigger

        std::string original = u->name;
        helper.run_auto_name_solo_units();

        expect(u->name == original) << "Multi-person unit should not be renamed";
    };

    "AutoNameSoloUnits does NOT rename a unit with mixed races totalling 1"_test = [] {
        // Edge case: 1 I_MAN + 1 I_ORC = 2 people — must NOT rename
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test Faction");
        Unit *u = helper.get_first_unit(faction);
        u->items.SetNum(I_LEADERS, 0);
        u->items.SetNum(I_MAN, 1);
        u->items.SetNum(I_ORC, 1);  // total = 2

        std::string original = u->name;
        helper.run_auto_name_solo_units();

        expect(u->name == original) << "2-person mixed unit must not be renamed";
    };

    "AutoNameSoloUnits does NOT rename an already-renamed unit"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test Faction");
        Unit *u = helper.get_first_unit(faction);
        u->set_name("Thorin Oakenshield");

        std::string named = u->name;
        helper.run_auto_name_solo_units();

        expect(u->name == named) << "Already-named unit must not be renamed";
    };

    // -----------------------------------------------------------------------
    // Within-turn uniqueness: two units of same race get different names
    // -----------------------------------------------------------------------
    "AutoNameSoloUnits gives unique names to two solo units of same race"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test Faction");
        ARegion *region = helper.get_region(0, 0, 0);

        // Create two single-orc units side by side
        Unit *u1 = helper.get_first_unit(faction);
        u1->items.SetNum(I_LEADERS, 0);
        u1->items.SetNum(I_ORC, 1);

        Unit *u2 = helper.create_unit(faction, region);
        u2->items.SetNum(I_LEADERS, 0);
        u2->items.SetNum(I_ORC, 1);

        helper.run_auto_name_solo_units();

        std::string base1 = u1->name | filter::strip_number;
        std::string base2 = u2->name | filter::strip_number;

        // Both should be renamed (not "Unit") and have different names
        expect(base1 != std::string("Unit")) << "First orc was not renamed: " << u1->name;
        expect(base2 != std::string("Unit")) << "Second orc was not renamed: " << u2->name;
        expect(base1 != base2) << "Two units got the same name: " << base1;
    };
};
