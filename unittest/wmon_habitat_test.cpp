#include "external/boost/ut.hpp"

#include <algorithm>

#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "items.h"
#include "../neworigins/wmon_table.h"

namespace ut = boost::ut;

// A wandering monster spawned outside its habitat can never leave the region:
// Unit::DefaultOrders (unit.cpp) only picks preferred terrain, or neutral
// terrain that touches preferred terrain, and a terrain that exists only on
// another map level is unreachable no matter the neighbourhood. That is how
// Troll Packs stood in surface swamps and Lizard Men in the deepforest for a
// whole game. Every spawn slot must therefore lie inside the monster's habitat
// (or name a free roamer with no preferences at all).

ut::suite<"WMonHabitat"> wmon_habitat_suite = [] {
    using namespace ut;

    "every wandering-monster slot lies inside the monster's habitat"_test = [] {
        for (const auto &slot : NewOriginsWMons) {
            const int terrain = TerrainDefs[slot.terrain].similar_type;

            for (const int mon : {slot.small, slot.big, slot.humanoid}) {
                if (mon == -1) continue;

                const auto found = find_monster(ItemDefs[mon].abr, 0);
                expect(found.has_value())
                    << "wandering-monster table names an undefined monster";
                if (!found) continue;

                const MonType &monster = found->get();

                expect(std::ranges::find(monster.forbiddenTerrain, terrain)
                           == monster.forbiddenTerrain.end())
                    << monster.name << " is spawned on "
                    << TerrainDefs[slot.terrain].name
                    << ", which it refuses to enter";

                const bool free_roamer = monster.preferredTerrain.empty();
                const bool in_habitat =
                    std::ranges::find(monster.preferredTerrain, terrain)
                    != monster.preferredTerrain.end();
                expect(free_roamer || in_habitat)
                    << monster.name << " is spawned on "
                    << TerrainDefs[slot.terrain].name
                    << " but its habitat does not include it; the unit would never move";
            }
        }
    };
};
