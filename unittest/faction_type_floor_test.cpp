#include "external/boost/ut.hpp"

#include "gamedefs.h"
#include "faction.h"
#include "string_parser.hpp"

#include <unordered_map>
#include <string>

namespace ut = boost::ut;

// parse_faction_type is a free function in parseorders.cpp with no public header;
// declare it here so the test can call it directly.
int parse_faction_type(parser::string_parser &parser, std::unordered_map<std::string, int> &type);

// Trident faction-point model: base 1-1, one floating point, floor 1 per category.
// factionTypeMin is a ruleset global (gamedefs.h); 0 = no floor (legacy behavior).
ut::suite<"Faction type floor"> faction_type_floor_suite = []
{
  using namespace ut;

  "floor fills the unspent category and forbids overspend"_test = []
  {
    // Build the merged two-category shape by hand (normally done by Game::Game()
    // under MARTIAL_MERGED); UnitTestHelper is avoided on purpose — its Game member
    // would populate FactionTypes from the unittest ruleset (DEFAULT, 3 categories).
    FactionTypes->push_back(F_MARTIAL);
    FactionTypes->push_back(F_MAGIC);

    auto savedPoints = Globals->FACTION_POINTS;
    int savedMin = factionTypeMin;
    Globals->FACTION_POINTS = 3;
    factionTypeMin = 1;

    // "magic 2" -> Magic 2, Martial auto-floored to 1 (a valid 1-2 build).
    {
      std::unordered_map<std::string, int> type;
      parser::string_parser p("magic 2");
      expect(parse_faction_type(p, type) == 0_i);
      expect(type[F_MAGIC] == 2_i);
      expect(type[F_MARTIAL] == 1_i);
    }

    // "magic 3" -> Martial floored to 1, sum 4 > FACTION_POINTS 3 -> rejected.
    {
      std::unordered_map<std::string, int> type;
      parser::string_parser p("magic 3");
      expect(parse_faction_type(p, type) == -1_i);
    }

    // "martial 2 magic 1" -> explicit 2-1, accepted unchanged.
    {
      std::unordered_map<std::string, int> type;
      parser::string_parser p("martial 2 magic 1");
      expect(parse_faction_type(p, type) == 0_i);
      expect(type[F_MARTIAL] == 2_i);
      expect(type[F_MAGIC] == 1_i);
    }

    // "generic" -> 1 in each category, valid under the floor.
    {
      std::unordered_map<std::string, int> type;
      parser::string_parser p("generic");
      expect(parse_faction_type(p, type) == 0_i);
      expect(type[F_MARTIAL] == 1_i);
      expect(type[F_MAGIC] == 1_i);
    }

    Globals->FACTION_POINTS = savedPoints;
    factionTypeMin = savedMin;
    FactionTypes->clear();
  };

  "floor of 0 keeps legacy behavior (category may stay 0)"_test = []
  {
    FactionTypes->push_back(F_MARTIAL);
    FactionTypes->push_back(F_MAGIC);

    auto savedPoints = Globals->FACTION_POINTS;
    int savedMin = factionTypeMin;
    Globals->FACTION_POINTS = 3;
    factionTypeMin = 0;

    // "magic 2" -> Martial stays 0: no floor, order accepted.
    {
      std::unordered_map<std::string, int> type;
      parser::string_parser p("magic 2");
      expect(parse_faction_type(p, type) == 0_i);
      expect(type[F_MAGIC] == 2_i);
      expect(type[F_MARTIAL] == 0_i);
    }

    // "magic 3" -> spends all points on one category, legal without a floor.
    {
      std::unordered_map<std::string, int> type;
      parser::string_parser p("magic 3");
      expect(parse_faction_type(p, type) == 0_i);
      expect(type[F_MAGIC] == 3_i);
      expect(type[F_MARTIAL] == 0_i);
    }

    Globals->FACTION_POINTS = savedPoints;
    factionTypeMin = savedMin;
    FactionTypes->clear();
  };
};
