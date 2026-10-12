#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "ruleset_config.h"
#include "unit.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

namespace {
    constexpr GuardRules guards {};

    // The engine defaults are the NewOrigins garrison levels.
    static_assert(guards.fighter_skill(TOWN_VILLAGE) == 2 && guards.fighter_skill(TOWN_CITY) == 4);
    static_assert(guards.observation(TOWN_VILLAGE) == 3 && guards.observation(TOWN_CITY) == 5);
    static_assert(guards.mage_level(TOWN_VILLAGE, true, 0) == 3);
    static_assert(guards.mage_level(TOWN_CITY, true, 0) == 5);
    static_assert(guards.mage_level(TOWN_CITY, false, 0) == 4);
    // START_CITY_MAGES raises the base; the energy shield bonus goes on top of it.
    static_assert(guards.mage_level(TOWN_CITY, false, 6) == 6);
    static_assert(guards.mage_level(TOWN_CITY, true, 6) == 7);
    // A town that shrank to a village keeps its town-tier observation.
    static_assert(guards.reached_observation(TOWN_VILLAGE, 3) == 4);

    Unit *create_guard_mage(UnitTestHelper& helper, ARegion *r, const int spell, const int level) {
        Faction *gfac = helper.get_faction(helper.get_guardfaction());
        Unit *u = helper.create_unit(gfac, r);
        u->type = U_GUARDMAGE;
        u->combat = spell;
        u->SetSkill(S_FORCE, level);
        u->SetSkill(spell, level);
        return u;
    }

    // The faction's starting region. The stub world counts every settlement as a starting
    // city, so the garrison there is a TOWN_CITY one with START_CITY_MAGES applied.
    ARegion *start_city_region(UnitTestHelper& helper) {
        helper.initialize_game();
        helper.setup_turn();
        Faction *f = helper.create_faction("Test Faction");
        ARegion *r = helper.get_first_unit(f)->object->region;
        ut::expect(r->town != nullptr) << "starting region must have a settlement";
        ut::expect(r->IsStartingCity() != 0) << "starting region must be a starting city";
        return (r->town && r->IsStartingCity()) ? r : nullptr;
    }
}

ut::suite<"Guard skill levels"> guard_skill_levels_suite = [] {
    using namespace ut;

    "AdjustCityMons raises a weak garrison mage to its tier level"_test = [] {
        UnitTestHelper helper;
        ARegion *r = start_city_region(helper);
        if (!r) return;

        Unit *eshi = create_guard_mage(helper, r, S_ENERGY_SHIELD, 1);
        Unit *fshi = create_guard_mage(helper, r, S_FORCE_SHIELD, 1);
        helper.run_adjust_city_mons(r);

        const GuardRules& rules = ruleset_config().guards;
        const int eshi_level = rules.mage_level(TOWN_CITY, true, Globals->START_CITY_MAGES);
        const int fshi_level = rules.mage_level(TOWN_CITY, false, Globals->START_CITY_MAGES);
        expect(eshi->GetRealSkill(S_FORCE) == eshi_level);
        expect(eshi->GetRealSkill(S_ENERGY_SHIELD) == eshi_level);
        expect(fshi->GetRealSkill(S_FORCE) == fshi_level);
        expect(fshi->GetRealSkill(S_FORCE_SHIELD) == fshi_level);
    };

    "AdjustCityMons never lowers a stronger garrison mage"_test = [] {
        UnitTestHelper helper;
        ARegion *r = start_city_region(helper);
        if (!r) return;

        const int strong = ruleset_config().guards.mage_level(TOWN_CITY, true, Globals->START_CITY_MAGES) + 1;
        Unit *eshi = create_guard_mage(helper, r, S_ENERGY_SHIELD, strong);
        helper.run_adjust_city_mons(r);

        expect(eshi->GetRealSkill(S_FORCE) == strong);
        expect(eshi->GetRealSkill(S_ENERGY_SHIELD) == strong);
    };
};
