#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "testhelper.hpp"

namespace ut = boost::ut;

ut::suite<"Balance Notification"> balance_notification_suite = []
{
    using namespace ut;

    // ----------------------------------------------------------------
    // deliver_balance_patch — skills
    // ----------------------------------------------------------------

    "deliver_balance_patch sends skill description to faction that knows the skill"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        Faction *fac = helper.create_faction("Test Faction");
        // Simulate: faction has been shown HEAL up to level 3
        fac->skills.SetDays(S_HEALING, 3);

        Game::PatchNotification p{ 1, { S_HEALING }, {} };
        helper.game_object().deliver_balance_patch(p);

        auto& shows = fac->shows;
        bool found = std::any_of(shows.begin(), shows.end(), [](const ShowSkill& s) {
            return s.skill == S_HEALING && s.level == 3;
        });
        expect(found) << "expected ShowSkill entry for S_HEALING lv3";
    };

    "deliver_balance_patch does not send skill to faction that does not know it"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        Faction *fac = helper.create_faction("Test Faction");
        // faction has never studied HEAL (GetDays returns 0)

        Game::PatchNotification p{ 1, { S_HEALING }, {} };
        helper.game_object().deliver_balance_patch(p);

        auto& shows = fac->shows;
        bool found = std::any_of(shows.begin(), shows.end(), [](const ShowSkill& s) {
            return s.skill == S_HEALING;
        });
        expect(!found) << "expected no ShowSkill entry for unknown skill";
    };

    "deliver_balance_patch sends all levels 1..max when faction knows skill at higher level"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        Faction *fac = helper.create_faction("Test Faction");
        fac->skills.SetDays(S_HEALING, 5);

        Game::PatchNotification p{ 1, { S_HEALING }, {} };
        helper.game_object().deliver_balance_patch(p);

        auto& shows = fac->shows;
        for (int lvl = 1; lvl <= 5; lvl++) {
            bool found = std::any_of(shows.begin(), shows.end(), [lvl](const ShowSkill& s) {
                return s.skill == S_HEALING && s.level == lvl;
            });
            expect(found) << "expected ShowSkill entry at level " << lvl;
        }
        expect(shows.size() >= 5_ul) << "expected at least 5 ShowSkill entries";
    };

    // ----------------------------------------------------------------
    // deliver_balance_patch — items
    // ----------------------------------------------------------------

    "deliver_balance_patch sends item description to faction that has seen the item"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        Faction *fac = helper.create_faction("Test Faction");
        // Mark item as fully known (value 2 = full description seen)
        fac->items.SetNum(I_HEALPOTION, 2);
        fac->itemshows.clear();

        Game::PatchNotification p{ 1, {}, { I_HEALPOTION } };
        helper.game_object().deliver_balance_patch(p);

        bool found = std::any_of(fac->itemshows.begin(), fac->itemshows.end(),
            [](const ShowItem& s) { return s.item == I_HEALPOTION; });
        expect(found) << "expected ShowItem entry for I_HEALPOTION";
    };

    "deliver_balance_patch does not send item to faction that has not seen it"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        Faction *fac = helper.create_faction("Test Faction");
        // faction has never seen HEALPOTION

        Game::PatchNotification p{ 1, {}, { I_HEALPOTION } };
        helper.game_object().deliver_balance_patch(p);

        bool found = std::any_of(fac->itemshows.begin(), fac->itemshows.end(),
            [](const ShowItem& s) { return s.item == I_HEALPOTION; });
        expect(!found) << "expected no ShowItem entry for unseen item";
    };

    // ----------------------------------------------------------------
    // upgrade_patch_level — version gating
    // ----------------------------------------------------------------

    "upgrade_patch_level fires notification when saved patch is lower than current"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        Faction *fac = helper.create_faction("Test Faction");
        fac->skills.SetDays(S_HEALING, 2);

        // saved patch 0 < current patch 1 → should fire
        helper.game_object().upgrade_patch_level(MAKE_ATL_VER(8, 1, 0));

        bool found = std::any_of(fac->shows.begin(), fac->shows.end(),
            [](const ShowSkill& s) { return s.skill == S_HEALING; });
        expect(found) << "expected notification fired for patch 0 < 1";
    };

    "upgrade_patch_level skips notification when saved patch equals current"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        Faction *fac = helper.create_faction("Test Faction");
        fac->skills.SetDays(S_HEALING, 2);

        // saved patch 1 == current patch 1 → should NOT fire
        helper.game_object().upgrade_patch_level(MAKE_ATL_VER(8, 1, 1));

        bool found = std::any_of(fac->shows.begin(), fac->shows.end(),
            [](const ShowSkill& s) { return s.skill == S_HEALING; });
        expect(!found) << "expected no notification when patch already current";
    };

    "upgrade_patch_level notifies multiple factions"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        Faction *fac1 = helper.create_faction("Faction One");
        Faction *fac2 = helper.create_faction("Faction Two");
        fac1->skills.SetDays(S_HEALING, 1);
        fac2->skills.SetDays(S_HEALING, 4);

        helper.game_object().upgrade_patch_level(MAKE_ATL_VER(8, 1, 0));

        auto has_heal = [](Faction *f) {
            return std::any_of(f->shows.begin(), f->shows.end(),
                [](const ShowSkill& s) { return s.skill == S_HEALING; });
        };
        expect(has_heal(fac1)) << "fac1 should receive notification";
        expect(has_heal(fac2)) << "fac2 should receive notification";
    };
};
