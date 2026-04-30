#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "testhelper.hpp"

// Because boost::ut has it's own concept of events, as does Game, we cannot just use do
// using namespace boost::ut; here. Instead, we alias it, and then use the alias inside the
// closure to make the user defined literals and all the other niceness available.
namespace ut = boost::ut;

// This suite will test various aspects of the Faction class in isolation.
ut::suite<"Produce"> produce_suite = []
{
  using namespace ut;

  "Producing an item with ORed inputs consumes personal items first"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    helper.setup_turn();
    helper.enable(UnitTestHelper::Type::ITEM, I_FOOD, true);
    helper.enable(UnitTestHelper::Type::SKILL, S_COOKING, true);

    std::string name = "Test Faction";
    Faction *faction = helper.create_faction(name);
    Unit *unit = helper.get_first_unit(faction);
    unit->items.SetNum(I_LEADERS, 10); // 10 men so we can produce up to 10 meals/month
    helper.set_skill_level(unit, S_COOKING, 1);
    unit->items.SetNum(I_LIVESTOCK, 5);

    // We have another unit which has grain.  Bug was the grain would be used before the personal items
    Unit *unit2 = helper.create_unit(faction, helper.get_region(0, 0, 0));
    unit2->SetFlag(1, 2);
    unit2->items.SetNum(I_LEADERS, 1);
    unit2->items.SetNum(I_GRAIN, 10);

    std::stringstream ss;
    ss << "#atlantis " << faction->num << "\n";
    ss << "unit " << unit->num << "\n";
    ss << "produce 3 meal\n";
    ss << "unit " << unit2->num << "\n";
    ss << "share 1\n";
    helper.parse_orders(faction->num, ss);
    helper.run_productions();

    // Check the results after running the productions
    expect(unit->items.GetNum(I_FOOD) == 3_i);
    expect(unit->items.GetNum(I_LIVESTOCK) == 2_i); // 5 - 3 meals = 2 livestock left
    expect(unit2->items.GetNum(I_GRAIN) == 10_i); // Grain should not be consumed since we used personal items first
  };

  "COOK with SKILLOUT: 1 grain produces level meals per man"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    helper.setup_turn();
    helper.enable(UnitTestHelper::Type::ITEM, I_FOOD, true);
    helper.enable(UnitTestHelper::Type::SKILL, S_COOKING, true);

    std::string name = "Test Faction";
    Faction *faction = helper.create_faction(name);
    Unit *unit = helper.get_first_unit(faction);
    unit->items.SetNum(I_LEADERS, 1); // 1 man
    helper.set_skill_level(unit, S_COOKING, 3);
    unit->items.SetNum(I_GRAIN, 5);

    std::stringstream ss;
    ss << "#atlantis " << faction->num << "\n";
    ss << "unit " << unit->num << "\n";
    ss << "produce meal\n";
    helper.parse_orders(faction->num, ss);
    helper.run_productions();

    // 1 man, COOK level 3, SKILLOUT: maxproduced=1 (1 man), output=1*3=3 meals, 1 grain consumed
    expect(unit->items.GetNum(I_FOOD) == 3_i);
    expect(unit->items.GetNum(I_GRAIN) == 4_i);
  };

  "COOK with SKILLOUT: 2 men COOK3 with 5 grain produce 6 meals consume 2 grain"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    helper.setup_turn();
    helper.enable(UnitTestHelper::Type::ITEM, I_FOOD, true);
    helper.enable(UnitTestHelper::Type::SKILL, S_COOKING, true);

    std::string name = "Test Faction";
    Faction *faction = helper.create_faction(name);
    Unit *unit = helper.get_first_unit(faction);
    unit->items.SetNum(I_LEADERS, 2); // 2 men
    helper.set_skill_level(unit, S_COOKING, 3);
    unit->items.SetNum(I_GRAIN, 5);

    std::stringstream ss;
    ss << "#atlantis " << faction->num << "\n";
    ss << "unit " << unit->num << "\n";
    ss << "produce meal\n";
    helper.parse_orders(faction->num, ss);
    helper.run_productions();

    // 2 men, COOK level 3, SKILLOUT: maxproduced=2 (2 men), output=2*3=6 meals, 2 grain consumed
    expect(unit->items.GetNum(I_FOOD) == 6_i);
    expect(unit->items.GetNum(I_GRAIN) == 3_i);
  };

  "Producing an item with ORed inputs consumes shared items if personal items are insufficient"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    helper.setup_turn();
    helper.enable(UnitTestHelper::Type::ITEM, I_FOOD, true);
    helper.enable(UnitTestHelper::Type::SKILL, S_COOKING, true);

    std::string name = "Test Faction";
    Faction *faction = helper.create_faction(name);
    Unit *unit = helper.get_first_unit(faction);
    unit->items.SetNum(I_LEADERS, 10); // 10 men so we can produce up to 10 meals/month
    helper.set_skill_level(unit, S_COOKING, 1);
    unit->items.SetNum(I_LIVESTOCK, 5);

    // We have another unit which has grain.  Bug was the grain would be used before the personal items
    Unit *unit2 = helper.create_unit(faction, helper.get_region(0, 0, 0));
    unit2->SetFlag(1, 2);
    unit2->items.SetNum(I_LEADERS, 1);
    unit2->items.SetNum(I_GRAIN, 10);

    std::stringstream ss;
    ss << "#atlantis " << faction->num << "\n";
    ss << "unit " << unit->num << "\n";
    ss << "produce 6 meal\n";
    ss << "unit " << unit2->num << "\n";
    ss << "share 1\n";
    helper.parse_orders(faction->num, ss);
    helper.run_productions();

    // Check the results after running the productions
    expect(unit->items.GetNum(I_FOOD) == 6_i);
    expect(unit->items.GetNum(I_LIVESTOCK) == 0_i); // no livestock left
    expect(unit2->items.GetNum(I_GRAIN) == 9_i); // 1 grain used from shared.
  };
};
