#include "external/boost/ut.hpp"

#include "aregion.h"
#include "gamedata.h"
#include "items.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

// Where a race can be found: race_for_roll() (the habitat die, used by world creation)
// and race_terrains() (the JSON report and the rules), which is built on it.
namespace {
    bool contains(const std::vector<int>& list, const int value) {
        return std::ranges::find(list, value) != list.end();
    }

    // A terrain with races 1..4 inland and 5..7 on the coast, so every slot is visible.
    TerrainType numbered_terrain() {
        TerrainType t = TerrainDefs[R_PLAIN];
        for (int i = 0; i < 4; i++) t.races[i] = i + 1;
        for (int i = 0; i < 3; i++) t.coastal_races[i] = i + 5;
        return t;
    }
}

ut::suite<"Race terrain"> race_terrain_suite = [] {
    using namespace ut;

    "the habitat die has 4 faces inland and 7 by water"_test = [] {
        expect(race_roll_faces(false) == 4_i);
        expect(race_roll_faces(true) == 7_i);
    };

    // Worlds were generated with this mapping; changing it changes which race a hex gets.
    "faces map to race slots as world creation always did"_test = [] {
        const TerrainType t = numbered_terrain();
        for (int n = 0; n < 4; n++) expect(race_for_roll(t, n) == n + 1) << "inland face" << n;
        expect(race_for_roll(t, 4) == 4_i) << "face 4 repeats the last inland race";
        expect(race_for_roll(t, 5) == 5_i);
        expect(race_for_roll(t, 6) == 6_i);
        for (int n = 0; n < race_roll_faces(true); n++)
            expect(race_for_roll(t, n) != 7) << "the third coastal slot is never rolled";
    };

    "a race in an inland slot is found anywhere in that terrain"_test = [] {
        for (int t = 0; t < R_NUM; t++) {
            const TerrainType& terrain = TerrainDefs[t];
            if (!(terrain.flags & TerrainType::SHOW_RULES) || terrain.pop == 0) continue;
            for (const int race : terrain.races) {
                if (race < 0 || (ItemDefs[race].flags & ItemType::DISABLED)) continue;
                const RaceTerrains found = race_terrains(race);
                const bool listed = std::ranges::any_of(found.anywhere, [&](const int f) {
                    return TerrainDefs[f].name == terrain.name;
                });
                expect(listed) << ItemDefs[race].abr << "in" << terrain.name;
            }
        }
    };

    "the coast list holds only terrains a coastal roll can give"_test = [] {
        for (int race = 0; race < NITEMS; race++) {
            if (!(ItemDefs[race].type & IT_MAN)) continue;
            const RaceTerrains found = race_terrains(race);
            for (const int t : found.coast) {
                expect(!contains(found.anywhere, t)) << ItemDefs[race].abr << "listed twice";
                bool rolled = false;
                for (int n = race_roll_faces(false); n < race_roll_faces(true); n++)
                    rolled = rolled || race_for_roll(TerrainDefs[t], n) == race;
                expect(rolled) << ItemDefs[race].abr << "on the coast of" << TerrainDefs[t].name;
            }
        }
    };

    "a race only in the third coastal slot is not listed for that terrain"_test = [] {
        for (int t = 0; t < R_NUM; t++) {
            const TerrainType& terrain = TerrainDefs[t];
            if (!(terrain.flags & TerrainType::SHOW_RULES) || terrain.pop == 0) continue;
            const int race = terrain.coastal_races[2];
            if (race < 0) continue;
            const bool elsewhere = std::ranges::find(terrain.races, race) != std::end(terrain.races)
                || terrain.coastal_races[0] == race || terrain.coastal_races[1] == race;
            if (elsewhere) continue;
            const RaceTerrains found = race_terrains(race);
            const auto here = [&](const int f) { return TerrainDefs[f].name == terrain.name; };
            expect(std::ranges::none_of(found.anywhere, here) && std::ranges::none_of(found.coast, here))
                << ItemDefs[race].abr << "listed for" << terrain.name;
        }
    };

    "leaders and disabled races are found nowhere"_test = [] {
        const RaceTerrains leaders = race_terrains(I_LEADERS);
        expect(leaders.anywhere.empty() && leaders.coast.empty());
        for (int race = 0; race < NITEMS; race++) {
            if (!(ItemDefs[race].type & IT_MAN) || !(ItemDefs[race].flags & ItemType::DISABLED)) continue;
            const RaceTerrains found = race_terrains(race);
            expect(found.anywhere.empty() && found.coast.empty()) << ItemDefs[race].abr;
        }
    };
};
