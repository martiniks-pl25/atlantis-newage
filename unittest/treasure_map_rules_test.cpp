#include "external/boost/ut.hpp"

#include "gamedata.h"
#include "items.h"
#include "ruleset_config.h"
#include "testhelper.hpp"

#include <format>
#include <string>

namespace ut = boost::ut;

static bool mentions(const std::string& text, const std::string& phrase)
{
    return text.find(phrase) != std::string::npos;
}

// The chance arithmetic is pure, so it is checked at compile time.
static_assert(TreasureMapRules{}.chance(false) == 25, "engine default: 25% without a compass");
static_assert(TreasureMapRules{}.chance(true) == 50, "engine default: a compass doubles it");
static_assert(TreasureMapRules{.decipher_chance = 70}.chance(true) == 100,
              "a multiplied chance is capped at 100%");
static_assert(TreasureMapRules{.decipher_chance = 30, .compass_multiplier = 1}.chance(true) == 30,
              "a multiplier of 1 makes the compass worthless");

ut::suite<"TreasureMapRules"> treasure_map_rules_suite = []
{
    using namespace ut;

    // The descriptions print the configured numbers rather than their own copy of them,
    // so retuning the ruleset retunes the text with it. Tests link the stub ruleset
    // (GAME=unittest), so these compare against ruleset_config(), not NewOrigins values.
    "treasure map description states every configured chance"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        const TreasureMapRules& tmap = ruleset_config().pirates.treasure_map;

        const std::string text = item_description(I_TREASURE_MAP, 1);

        expect(mentions(text, std::format("a {}% chance of success", tmap.chance(false)))) << text;
        expect(mentions(text, std::format("or {}% if the same unit carries a compass",
                                          tmap.chance(true)))) << text;
        expect(mentions(text, std::format("destroys the map {}% of the time",
                                          tmap.burn_on_fail))) << text;
    };

    "compass description states the chance without and with it"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        const TreasureMapRules& tmap = ruleset_config().pirates.treasure_map;

        const std::string text = item_description(I_COMPASS, 1);

        expect(mentions(text, std::format("from {}% to {}%",
                                          tmap.chance(false), tmap.chance(true)))) << text;
    };
};
