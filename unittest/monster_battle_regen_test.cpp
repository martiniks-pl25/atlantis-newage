#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "items.h"
#include "testhelper.hpp"

#include <string>

namespace ut = boost::ut;

// ---------------------------------------------------------------------------
// Guard: what a report says about in-battle regeneration matches the battle.
//
// The battle Soldier, the monster description and the report JSON all take the
// value from battle_regen(), which honours GameDefs::MONSTER_BATTLE_REGEN. With
// the flag off no monster regenerates, so the description must not mention it;
// with the flag on the table value is shown. Hydra is the probe: it carries the
// largest regen in MonDefs.
// ---------------------------------------------------------------------------

// unittest/rules.cpp sets MONSTER_BATTLE_REGEN = 0; a test states the value it
// needs and puts it back afterwards.
struct ScopedBattleRegen {
    int saved;
    explicit ScopedBattleRegen(int value) : saved(Globals->MONSTER_BATTLE_REGEN) {
        Globals->MONSTER_BATTLE_REGEN = value;
    }
    ~ScopedBattleRegen() { Globals->MONSTER_BATTLE_REGEN = saved; }
};

ut::suite<"Monster Battle Regen"> monster_battle_regen_suite = []
{
    using namespace ut;

    "flag off: no regen in battle value or description"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ScopedBattleRegen regen_off(0);

        const MonType& hydra = find_monster("HYDR", 0)->get();
        expect(hydra.regen > 0_i) << "probe lost its table regen - test covers nothing";

        expect(battle_regen(hydra) == 0_i);
        std::string text = item_description(I_HYDRA, 1);
        expect(!text.empty()) << "hydra description is empty";
        expect(text.find("regenerates") == std::string::npos)
            << "description promises regeneration the battle does not perform";
    };

    "flag on: table regen in battle value and description"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ScopedBattleRegen regen_on(1);

        const MonType& hydra = find_monster("HYDR", 0)->get();
        expect(battle_regen(hydra) == hydra.regen);
        std::string text = item_description(I_HYDRA, 1);
        std::string sentence = "This monster regenerates " + std::to_string(hydra.regen) + " hits";
        expect(text.find(sentence) != std::string::npos) << "description lacks:" << sentence;
    };
};
