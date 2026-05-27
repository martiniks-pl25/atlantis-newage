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
// Helper: build a minimal Location list with one unit for GetSpoils.
// `alive`  — soldiers still alive (SetMen count after battle).
// `dead`   — soldiers that died during battle (unit->losses).
// The unit also carries 1 I_RESOURCE_MAP (the boss drop).
// ---------------------------------------------------------------------------
static std::list<Location *> make_loser_list(Unit *u, int alive, int dead)
{
    // create_unit() adds I_LEADERS=1; clear it so only the explicit soldier
    // type contributes to GetSoldiers(), giving precise percent control.
    u->items.SetNum(I_LEADERS, 0);

    // Set surviving soldier count via I_LICH.
    u->items.SetNum(I_LICH, alive > 0 ? alive : 0);

    u->items.SetNum(I_RESOURCE_MAP, 1);
    u->losses = dead;

    Location *loc = new Location();
    loc->unit   = u;
    loc->obj    = u->object;
    loc->region = u->object ? u->object->region : nullptr;

    std::list<Location *> losers;
    losers.push_back(loc);
    return losers;
}

static void free_loser_list(std::list<Location *>& losers)
{
    for (auto *l : losers) delete l;
    losers.clear();
}

ut::suite<"DungeonRmapDrop"> dungeon_rmap_drop_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Test 1: boss unit fully wiped → exactly 1 RMAP drops.
    // numalive=0, numdead=N → percent=1.0 → num=1 → IT_ALWAYS_SPOIL → num2=1.
    // -----------------------------------------------------------------------
    "boss fully killed drops exactly 1 RMAP"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *mon = helper.get_faction(helper.get_monfaction());
        ARegion *room = helper.get_region(0, 2, 0);

        Unit *boss = helper.create_unit(mon, room);
        // alive=0 (all dead), dead=5
        auto losers = make_loser_list(boss, 0, 5);

        Battle b;
        ItemList spoils;
        b.GetSpoils(losers, spoils, 0, nullptr);

        expect(spoils.GetNum(I_RESOURCE_MAP) == 1_i)
            << "fully wiped boss must drop exactly 1 RMAP";

        free_loser_list(losers);
    };

    // -----------------------------------------------------------------------
    // Test 2: boss unit partially killed → RMAP does NOT drop.
    // numalive=3, numdead=2 → percent=0.4 → num=(int)(1*0.4)=0 → num2=0.
    // The boss is still alive (dungeon stays ACTIVE); loot must not drop.
    // -----------------------------------------------------------------------
    "boss partially killed does not drop RMAP"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *mon = helper.get_faction(helper.get_monfaction());
        ARegion *room = helper.get_region(0, 2, 0);

        Unit *boss = helper.create_unit(mon, room);
        // alive=3, dead=2  (players retreated mid-fight)
        auto losers = make_loser_list(boss, 3, 2);

        Battle b;
        ItemList spoils;
        b.GetSpoils(losers, spoils, 0, nullptr);

        expect(spoils.GetNum(I_RESOURCE_MAP) == 0_i)
            << "partially killed boss must not drop RMAP";

        free_loser_list(losers);
    };

    // -----------------------------------------------------------------------
    // Test 3: populate_dungeon gives the boss unit exactly 1 RMAP.
    // Verifies the drop is wired at spawn time.
    // -----------------------------------------------------------------------
    "populate_dungeon gives boss exactly 1 RMAP"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *room = helper.get_region(0, 2, 0);
        ARegion *surf = helper.get_region(1, 1, 0);

        // Add a fake entrance object so the dungeon looks valid.
        Object *ent = new Object(surf);
        ent->num = 77;
        ent->type = O_DUNGEON_ENTRANCE;
        ent->incomplete = 0;
        surf->objects.push_back(ent);

        // Build a minimal DungeonInstance pointing to room as boss room.
        DungeonInstance d;
        d.id                 = 998;
        d.type               = DungeonType::DUNGEON_SKELETON_RUINS;
        d.state              = DungeonSlotState::ACTIVE;
        d.phase_turn         = 0;
        d.surface_region_num = surf->num;
        d.entrance_object_num = 77;
        d.entry_region_num   = room->num;
        d.boss_region_num    = room->num;
        d.room_nums          = { room->num };

        helper.inject_dungeon(d);
        helper.run_populate_dungeon(d);

        // Find the boss unit (monfaction unit in the boss room).
        Faction *mon = helper.get_faction(helper.get_monfaction());
        Unit *boss_unit = nullptr;
        for (auto *obj : room->objects) {
            for (auto *u : obj->units) {
                if (u->faction == mon) { boss_unit = u; break; }
            }
            if (boss_unit) break;
        }

        expect(boss_unit != nullptr) << "boss unit must be present after populate_dungeon";
        if (boss_unit) {
            expect(boss_unit->items.GetNum(I_RESOURCE_MAP) == 1_i)
                << "boss unit must carry exactly 1 RMAP after populate_dungeon";
        }
    };

    // -----------------------------------------------------------------------
    // Test 4: single-soldier boss (e.g. I_DEVIL = 1) fully killed → 1 RMAP.
    // Confirms the mechanic works even when numdead=1, numalive=0.
    // -----------------------------------------------------------------------
    "single-monster boss fully killed drops 1 RMAP"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *mon = helper.get_faction(helper.get_monfaction());
        ARegion *room = helper.get_region(0, 2, 0);

        Unit *boss = helper.create_unit(mon, room);
        // clear the default I_LEADERS=1 added by create_unit
        boss->items.SetNum(I_LEADERS, 0);
        // 1 devil died, 0 alive
        boss->items.SetNum(I_DEVIL, 0);
        boss->items.SetNum(I_RESOURCE_MAP, 1);
        boss->losses = 1;

        Location *loc = new Location();
        loc->unit   = boss;
        loc->obj    = boss->object;
        loc->region = room;
        std::list<Location *> losers = { loc };

        Battle b;
        ItemList spoils;
        b.GetSpoils(losers, spoils, 0, nullptr);

        expect(spoils.GetNum(I_RESOURCE_MAP) == 1_i)
            << "single-monster boss fully killed must drop 1 RMAP";

        delete loc;
    };
};
