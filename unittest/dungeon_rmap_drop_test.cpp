#include "external/boost/ut.hpp"
#include "battle.h"
#include "game.h"
#include "gamedata.h"
#include "dungeon.h"
#include "aregion.h"
#include "object.h"
#include "items.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// ---------------------------------------------------------------------------
// Resource-map drop from dungeon bosses.
//
// A non-pirate dungeon boss (race == boss_kill_item: ETTI/LICH/DEVIL/DRAGON)
// grants exactly 1 I_RESOURCE_MAP to the victors when killed inside a
// LEVEL_DUNGEON region. This is wired at kill time in Army::Lose() — like
// pirate compasses/whistles — so it does not depend on spawn-time inventory.
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

// Create a player unit with overwhelming, well-armed force so it reliably wins
// and fully wipes even a tough dungeon boss (lich/devil: 50 hits, regen, etc.).
static Unit *create_strong_attacker(UnitTestHelper &helper, ARegion *r)
{
    Faction *player = helper.create_faction("Player");
    Unit *u = helper.create_unit(player, r);
    u->items.SetNum(I_LEADERS, 500);
    u->items.SetNum(I_MSWORD, 500);          // mithril swords: cleaving, +attack
    helper.set_skill_level(u, S_COMBAT, 5);  // max combat skill
    return u;
}

ut::suite<"DungeonRmapDrop"> dungeon_rmap_drop_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // The classifier underpinning the drop: only the four non-pirate
    // boss_kill_items qualify; pirate kings and ordinary mobs do not.
    // -----------------------------------------------------------------------
    "is_dungeon_boss_kill_race classifies boss races"_test = [] {
        expect(is_dungeon_boss_kill_race(I_ETTIN))  << "Kobold Warrens boss";
        expect(is_dungeon_boss_kill_race(I_LICH))   << "Skeleton Ruins boss";
        expect(is_dungeon_boss_kill_race(I_DEVIL))  << "Demon Pit boss";
        expect(is_dungeon_boss_kill_race(I_DRAGON)) << "Dragon Lair boss";

        expect(!is_dungeon_boss_kill_race(I_PIRATE_KING))
            << "pirate king handled by its own loot path";
        expect(!is_dungeon_boss_kill_race(I_KOBOLD)) << "ordinary wander mob";
        expect(!is_dungeon_boss_kill_race(I_TROLL))  << "ordinary wander mob";
        expect(!is_dungeon_boss_kill_race(I_LEADERS)) << "not a monster";
    };

    // -----------------------------------------------------------------------
    // Boss killed inside a dungeon → victors receive exactly 1 RMAP.
    // -----------------------------------------------------------------------
    "dungeon boss killed drops exactly 1 RMAP"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);  // no city, no guards
        // Mark the region's level as a dungeon so the kill-time grant fires.
        r->level->levelType = ARegionArray::LEVEL_DUNGEON;

        Unit *boss     = helper.create_monster(r, I_LICH, 1);
        Unit *attacker = create_strong_attacker(helper, r);

        int result = helper.run_battle(r, attacker, boss);
        expect(result == BATTLE_WON) << "attacker must win";

        expect(faction_item_in_region(r, attacker->faction, I_RESOURCE_MAP) == 1_i)
            << "a killed dungeon boss must drop exactly 1 RMAP";
    };

    // -----------------------------------------------------------------------
    // Same boss race on the surface → no RMAP (gated on LEVEL_DUNGEON so that
    // wild ettins/dragons in the open world do not hand out maps).
    // -----------------------------------------------------------------------
    "boss race on surface drops no RMAP"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);  // surface, no city
        expect(r->level->levelType == ARegionArray::LEVEL_SURFACE)
            << "precondition: region is on the surface";

        Unit *boss     = helper.create_monster(r, I_LICH, 1);
        Unit *attacker = create_strong_attacker(helper, r);

        int result = helper.run_battle(r, attacker, boss);
        expect(result == BATTLE_WON) << "attacker must win";

        expect(faction_item_in_region(r, attacker->faction, I_RESOURCE_MAP) == 0_i)
            << "boss race outside a dungeon must not drop RMAP";
    };

    // -----------------------------------------------------------------------
    // Ordinary wander mob inside a dungeon → no RMAP (only the boss qualifies).
    // -----------------------------------------------------------------------
    "non-boss mob in dungeon drops no RMAP"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        r->level->levelType = ARegionArray::LEVEL_DUNGEON;

        Unit *mob      = helper.create_monster(r, I_KOBOLD, 20);
        Unit *attacker = create_strong_attacker(helper, r);

        int result = helper.run_battle(r, attacker, mob);
        expect(result == BATTLE_WON) << "attacker must win";

        expect(faction_item_in_region(r, attacker->faction, I_RESOURCE_MAP) == 0_i)
            << "ordinary dungeon mobs must not drop RMAP";
    };
};
