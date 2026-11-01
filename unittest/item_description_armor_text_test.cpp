#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "items.h"
#include "testhelper.hpp"

#include <string>
#include <vector>

namespace ut = boost::ut;

// ---------------------------------------------------------------------------
// Guard: how item descriptions name defense and print penalties.
//
// The armor defense modifier, a weapon's own defense bonus and its riding bonus
// to defense all land on dskill[ATTACK_COMBAT] (Soldier::Soldier in army.cpp),
// i.e. on melee defense, so the text must say "melee defense" - weapons and monsters already
// call ATTACK_COMBAT "melee". Attack modifiers stay "combat attack": askill is
// used by every weapon, ranged included.
//
// A penalty is printed as a magnitude ("penalty of 1"); "imposes a -1 penalty"
// contradicts itself.
//
// Many armor and weapon items are disabled in the test ruleset, and
// item_description() returns nothing for a disabled item, so each sweep enables
// them temporarily and restores them afterwards, like
// item_description_cast_syntax_test.cpp.
// ---------------------------------------------------------------------------

namespace {

bool armor_has_defense_modifier(int item)
{
    if (!(ItemDefs[item].type & IT_ARMOR)) return false;
    auto armor = find_armor(ItemDefs[item].abr);
    return armor && armor->get().defenseBonus != 0;
}

// A weapon's own defense bonus and its riding bonus to defense both feed
// dskill[ATTACK_COMBAT], and each is described in the weapon text.
bool weapon_has_defense_modifier(int item)
{
    if (!(ItemDefs[item].type & IT_WEAPON)) return false;
    auto weapon = find_weapon(ItemDefs[item].abr);
    if (!weapon) return false;
    return weapon->get().defenseBonus != 0 ||
        (weapon->get().flags & (WeaponType::RIDINGBONUS | WeaponType::RIDINGBONUSDEFENSE));
}

// "on defense" / "and defense" without "melee" in front of it.
bool has_bare_defense(const std::string& text)
{
    return text.find(" on defense") != std::string::npos ||
        text.find(" and defense") != std::string::npos;
}

bool prints_penalty_sentence(int item)
{
    return (ItemDefs[item].type & IT_ARMOR) || (ItemDefs[item].type & IT_BATTLE);
}

} // namespace

ut::suite<"Armor And Weapon Modifier Text"> armor_modifier_text_suite = []
{
    using namespace ut;

    "melee defense modifiers are described as melee"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        std::vector<int> temporarily_enabled;
        int armor_checked = 0;
        int weapons_checked = 0;

        for (int i = 0; i < NITEMS; i++) {
            const bool armor = armor_has_defense_modifier(i);
            const bool weapon = weapon_has_defense_modifier(i);
            if (!armor && !weapon) continue;

            if (ItemDefs[i].flags & ItemType::DISABLED) {
                helper.enable(UnitTestHelper::ITEM, i, true);
                temporarily_enabled.push_back(i);
            }

            const std::string text = item_description(i, 1);

            expect(text.find("melee defense") != std::string::npos)
                << "item" << ItemDefs[i].abr
                << "modifies melee defense but the text does not say 'melee defense':" << text;

            expect(text.find("combat defense") == std::string::npos)
                << "item" << ItemDefs[i].abr
                << "still calls melee defense 'combat defense':" << text;

            expect(!has_bare_defense(text))
                << "item" << ItemDefs[i].abr
                << "says 'defense' without 'melee':" << text;

            if (armor) armor_checked++;
            if (weapon) weapons_checked++;
        }

        for (int item : temporarily_enabled) helper.enable(UnitTestHelper::ITEM, item, false);

        // Guards the sweep itself: were the modifiers ever dropped from the
        // tables, the loop above would pass by covering nothing.
        expect(armor_checked > 0) << "sweep covered no armor with a defense modifier";
        expect(weapons_checked > 0) << "sweep covered no weapon with a defense modifier";
    };

    "penalties print a magnitude, never a negative number"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        std::vector<int> temporarily_enabled;
        int penalties_seen = 0;

        for (int i = 0; i < NITEMS; i++) {
            if (!prints_penalty_sentence(i)) continue;

            if (ItemDefs[i].flags & ItemType::DISABLED) {
                helper.enable(UnitTestHelper::ITEM, i, true);
                temporarily_enabled.push_back(i);
            }

            const std::string text = item_description(i, 1);

            expect(text.find("imposes a -") == std::string::npos &&
                   text.find("penalty of -") == std::string::npos)
                << "item" << ItemDefs[i].abr
                << "prints a negative number next to the word 'penalty':" << text;

            if (text.find("imposes a penalty of ") != std::string::npos) penalties_seen++;
        }

        for (int item : temporarily_enabled) helper.enable(UnitTestHelper::ITEM, item, false);

        // Guards the sweep itself: with no penalty sentence in the new wording
        // the loop above would pass by covering nothing.
        expect(penalties_seen > 0) << "sweep reached no 'imposes a penalty of' sentence";
    };
};
