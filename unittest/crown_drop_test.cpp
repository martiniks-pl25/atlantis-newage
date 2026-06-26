#include "external/boost/ut.hpp"
#include "battle.h"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "items.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// ---------------------------------------------------------------------------
// Crown drop from the pirate Admiral (I_PIRATE_KING).
//
// Killing the Admiral grants exactly 1 I_CROWN to the victors, wired at kill
// time in Army::Lose() — like pirate compasses/whistles, and unlike the
// dungeon-boss RMAP it is NOT gated on LEVEL_DUNGEON (a pirate hideout is the
// only place an Admiral spawns). The Crown is the Trident "Three Crowns"
// victory token. See docs/TRIDENT_VICTORY_MECHANIC_DESIGN.md.
// ---------------------------------------------------------------------------

// Sum a given item across all units of a faction in a region (spoils landing).
static int faction_item_in_region(ARegion *r, Faction *f, int item)
{
    int total = 0;
    for (auto *obj : r->objects)
        for (auto *u : obj->units)
            if (u->faction == f) total += u->items.GetNum(item);
    return total;
}

// Overwhelming, well-armed force so it reliably wins and fully wipes the Admiral.
static Unit *create_strong_attacker(UnitTestHelper &helper, ARegion *r)
{
    Faction *player = helper.create_faction("Player");
    Unit *u = helper.create_unit(player, r);
    u->items.SetNum(I_LEADERS, 500);
    u->items.SetNum(I_MSWORD, 500);
    helper.set_skill_level(u, S_COMBAT, 5);
    return u;
}

ut::suite<"CrownDrop"> crown_drop_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // The Crown item carries the flags the victory mechanic relies on.
    // -----------------------------------------------------------------------
    "Crown item has the expected flags"_test = [] {
        expect((ItemDefs[I_CROWN].type & IT_ALWAYS_SPOIL) != 0)
            << "Crown must always drop as battle spoils";
        expect((ItemDefs[I_CROWN].flags & ItemType::NOSTEALTH) != 0)
            << "Crown bearer must not be stealthy (gazette transparency)";
        expect((ItemDefs[I_CROWN].flags & ItemType::NOMARKET) != 0)
            << "Crown must not be sellable on a market";
        expect((ItemDefs[I_CROWN].flags & ItemType::CANTGIVE) == 0)
            << "Crown must be givable (transport to the capital)";
        expect((ItemDefs[I_CROWN].flags & ItemType::DISABLED) == 0)
            << "Crown must be enabled";
    };

    // -----------------------------------------------------------------------
    // Killing the Admiral → victors receive exactly 1 Crown.
    // -----------------------------------------------------------------------
    "killing a pirate Admiral drops exactly 1 Crown"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);  // no city, no guards

        Unit *admiral  = helper.create_monster(r, I_PIRATE_KING, 1);
        Unit *attacker = create_strong_attacker(helper, r);

        int result = helper.run_battle(r, attacker, admiral);
        expect(result == BATTLE_WON) << "attacker must win";

        expect(faction_item_in_region(r, attacker->faction, I_CROWN) == 1_i)
            << "a killed Admiral must drop exactly 1 Crown";
    };

    // -----------------------------------------------------------------------
    // Ordinary mob → no Crown (only the Admiral drops it).
    // -----------------------------------------------------------------------
    "ordinary mob drops no Crown"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);

        Unit *mob      = helper.create_monster(r, I_KOBOLD, 20);
        Unit *attacker = create_strong_attacker(helper, r);

        int result = helper.run_battle(r, attacker, mob);
        expect(result == BATTLE_WON) << "attacker must win";

        expect(faction_item_in_region(r, attacker->faction, I_CROWN) == 0_i)
            << "ordinary mobs must not drop a Crown";
    };
};
