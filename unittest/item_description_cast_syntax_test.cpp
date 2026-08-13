#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "items.h"
#include "skills.h"
#include "string_parser.hpp"
#include "testhelper.hpp"

#include <string>
#include <vector>

namespace ut = boost::ut;

// ---------------------------------------------------------------------------
// Guard: the spell name printed in an item description must be typable.
//
// An item with grantSkill lets its bearer CAST a spell, and item_description()
// spells out the order for them. CAST reads a single token (string_parser splits
// on whitespace) and parse_skill matches that one token against the skill name,
// so a multi-word name only reaches the parser when its spaces are written as
// underscores - "call pirates" has to be printed as "Call_Pirates".
//
// The check is a round trip rather than a comparison against expected wording:
// whatever word the description tells the player to type is fed back through the
// parser's own matching, so the test keeps holding when the prose changes.
// ---------------------------------------------------------------------------

// Every token that directly follows "CAST " in the text, punctuation stripped.
static std::vector<std::string> tokens_after_cast(const std::string& text)
{
    static const std::string marker = "CAST ";
    std::vector<std::string> found;

    for (size_t pos = text.find(marker); pos != std::string::npos; pos = text.find(marker, pos + 1)) {
        size_t start = pos + marker.size();
        size_t end = text.find_first_of(" ;.,", start);
        if (end == std::string::npos) end = text.size();
        if (end > start) found.push_back(text.substr(start, end - start));
    }

    return found;
}

// Items whose granted skill can be CAST. A ruleset that leaves such an item
// disabled still inherits its text as written, so those are covered too - but
// item_description() returns nothing for a disabled item, hence the temporary
// enable and the restore afterwards.
static std::vector<int> items_granting_a_spell()
{
    std::vector<int> found;

    for (int i = 0; i < NITEMS; i++) {
        auto granted = FindSkill(ItemDefs[i].grantSkill);
        if (granted && (granted->get().flags & SkillType::CAST)) found.push_back(i);
    }

    return found;
}

ut::suite<"Item Description Order Syntax"> item_description_cast_syntax_suite = []
{
    using namespace ut;

    "spell names in item descriptions parse as CAST arguments"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        std::vector<int> items = items_granting_a_spell();
        std::vector<int> temporarily_enabled;

        for (int item : items) {
            if (!(ItemDefs[item].flags & ItemType::DISABLED)) continue;
            helper.enable(UnitTestHelper::ITEM, item, true);
            temporarily_enabled.push_back(item);
        }

        for (int item : items) {
            const SkillType& skill = FindSkill(ItemDefs[item].grantSkill)->get();
            std::string text = item_description(item, 1);
            std::vector<std::string> names;

            // "CAST the <spell> spell" is prose, not an order; only the argument matters.
            for (const std::string& token : tokens_after_cast(text))
                if (token != "the") names.push_back(token);

            expect(!names.empty())
                << "description of item" << ItemDefs[item].abr
                << "grants castable skill" << skill.abbr
                << "but never states the CAST order";

            for (const std::string& name : names) {
                parser::token typed(name);

                // The comparison parse_skill performs; ci_traits reads '_' as a space.
                expect(typed == skill.name || typed == skill.abbr)
                    << "item" << ItemDefs[item].abr << "tells the player to type CAST" << name
                    << "- which does not match skill" << skill.name;

                // parse_skill skips disabled skills, so the full call is made only
                // where the ruleset can actually accept the order.
                if (!(skill.flags & SkillType::DISABLED)) {
                    expect(parse_skill(typed) != -1)
                        << "item" << ItemDefs[item].abr << "tells the player to type CAST" << name
                        << "- which the order parser rejects";
                }
            }
        }

        for (int item : temporarily_enabled) helper.enable(UnitTestHelper::ITEM, item, false);

        // Guards the sweep itself: were the granting items ever dropped from the
        // tables, the loop above would pass by covering nothing.
        expect(!items.empty()) << "no item grants a castable skill - sweep covered nothing";
    };
};
