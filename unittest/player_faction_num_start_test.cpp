#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "testhelper.hpp"

// boost::ut has its own event concept that clashes with Game's, so we alias the
// namespace and pull it in inside the suite closure (same pattern as the other tests).
namespace ut = boost::ut;

// PLAYER_FACTION_NUM_START shifts the starting number of player factions.
// factionseq is baked at NewGame(), so the parameter must be set BEFORE
// initialize_game(). Globals is the process-wide GameDefs pointer; restore it
// to 0 at the end of each test so the value does not bleed into later suites.
ut::suite<"Player faction num start"> player_faction_num_start_suite = []
{
  using namespace ut;

  "default 0 keeps players starting at slot 3"_test = []
  {
    UnitTestHelper helper;
    Globals->PLAYER_FACTION_NUM_START = 0;
    helper.initialize_game();

    // Guardsmen=1, Creatures=2 → first player faction is #3.
    Faction *p1 = helper.create_faction("Player One");
    expect(p1->num == 3_i);

    Globals->PLAYER_FACTION_NUM_START = 0;  // restore for later suites
  };

  "start number 5 makes first player faction #5"_test = []
  {
    UnitTestHelper helper;
    Globals->PLAYER_FACTION_NUM_START = 5;
    helper.initialize_game();

    // Slots 3 & 4 are empty (never allocated).
    expect(helper.get_faction(3) == nullptr);
    expect(helper.get_faction(4) == nullptr);

    // First real player faction takes slot 5; the next takes 6.
    Faction *p1 = helper.create_faction("Player One");
    expect(p1->num == 5_i);
    Faction *p2 = helper.create_faction("Player Two");
    expect(p2->num == 6_i);

    // The two built-in NPC factions still exist at 1 and 2.
    expect(helper.get_faction(helper.get_guardfaction()) != nullptr);
    expect(helper.get_faction(helper.get_monfaction()) != nullptr);

    Globals->PLAYER_FACTION_NUM_START = 0;  // restore for later suites
  };
};
