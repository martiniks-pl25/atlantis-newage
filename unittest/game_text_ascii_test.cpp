#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "items.h"
#include "skills.h"
#include "object.h"
#include "string_filters.hpp"
#include "testhelper.hpp"

#include <string>

namespace ut = boost::ut;

// ---------------------------------------------------------------------------
// Guard: player-facing game text must stay plain 7-bit ASCII.
//
// Turn reports are plain text with no encoding declaration, so a typographic
// character pasted into a string literal (an em dash, curly quotes, an ellipsis)
// renders as mojibake in whatever the player reads the report with.
//
// Unit and object descriptions are worse than cosmetic: Unit::Readin re-filters
// describe through filter::legal_characters on every load, and that filter keeps
// only alphanumerics plus an ASCII whitelist. A non-ASCII byte therefore survives
// exactly one turn and is then silently deleted, leaving a hole in the sentence -
// an em dash in the Dread Admiral's description used to decay into
// "The Dread Admiral  a legendary pirate warlord".
//
// Server-side text (logger::write) is not covered here: it is never shown to a
// player and the log is read as UTF-8.
// ---------------------------------------------------------------------------

// Offset of the first byte outside 7-bit ASCII, or npos if the text is clean.
static size_t first_non_ascii(const std::string& text)
{
    for (size_t i = 0; i < text.size(); i++)
        if (static_cast<unsigned char>(text[i]) > 0x7f) return i;
    return std::string::npos;
}

// A window around pos, so a failure message names the offending phrase.
// Arguments of a streamed expectation message are evaluated even when the
// expectation holds, so npos (nothing found) must return quietly.
static std::string context_at(const std::string& text, size_t pos)
{
    if (pos == std::string::npos) return "";
    size_t start = pos > 30 ? pos - 30 : 0;
    return text.substr(start, 60);
}

ut::suite<"Game Text Encoding"> game_text_ascii_suite = []
{
    using namespace ut;

    // -----------------------------------------------------------------------
    // Item tables and the SHOW ITEM text.
    // -----------------------------------------------------------------------
    "item names and descriptions are plain ASCII"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        for (int i = 0; i < NITEMS; i++) {
            if (ItemDefs[i].flags & ItemType::DISABLED) continue;

            for (const std::string& text : { ItemDefs[i].name, ItemDefs[i].names, ItemDefs[i].abr,
                                             item_description(i, 1), item_description(i, 0) }) {
                size_t pos = first_non_ascii(text);
                expect(pos == std::string::npos)
                    << "non-ASCII byte in the text of item" << ItemDefs[i].abr
                    << "near:" << context_at(text, pos);
            }
        }
    };

    // -----------------------------------------------------------------------
    // Skill tables and the skill descriptions sent with SHOW SKILL / study.
    // -----------------------------------------------------------------------
    "skill names and descriptions are plain ASCII"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        Faction *fac = helper.create_faction("Text Audit");

        for (int i = 0; i < NSKILLS; i++) {
            if (SkillDefs[i].flags & SkillType::DISABLED) continue;

            for (const std::string& text : { SkillDefs[i].name, SkillDefs[i].abbr }) {
                size_t pos = first_non_ascii(text);
                expect(pos == std::string::npos)
                    << "non-ASCII byte in the name of skill" << SkillDefs[i].abbr
                    << "near:" << context_at(text, pos);
            }

            for (int level = 1; level <= 5; level++) {
                std::string text = ShowSkill{ i, level }.Report(fac);
                size_t pos = first_non_ascii(text);
                expect(pos == std::string::npos)
                    << "non-ASCII byte in the description of skill" << SkillDefs[i].abbr
                    << "level" << level << "near:" << context_at(text, pos);
            }
        }
    };

    // -----------------------------------------------------------------------
    // Object names appear on every structure line of every region report.
    // -----------------------------------------------------------------------
    "object names are plain ASCII"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();

        for (int i = 0; i < NOBJECTS; i++) {
            if (ObjectDefs[i].flags & ObjectType::DISABLED) continue;

            size_t pos = first_non_ascii(ObjectDefs[i].name);
            expect(pos == std::string::npos)
                << "non-ASCII byte in object name" << ObjectDefs[i].name;
        }
    };

    // -----------------------------------------------------------------------
    // Monster descriptions are the surface that actually loses characters:
    // they are stored on the unit and re-filtered by Unit::Readin every load.
    // Anything the filter would drop must not be there in the first place.
    // -----------------------------------------------------------------------
    "monster descriptions survive the save and load character filter"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = helper.get_region(0, 2, 0);

        for (int i = 0; i < NITEMS; i++) {
            if (ItemDefs[i].flags & ItemType::DISABLED) continue;
            if (!(ItemDefs[i].type & IT_MONSTER)) continue;

            // Unit::free drives the loot clause, so every wording variant is exercised.
            for (int free_turns = 0; free_turns <= 3; free_turns++) {
                Unit *mon = helper.create_monster(r, i, 1);
                mon->free = free_turns;
                mon->UpdateMonsterDescription();

                expect((mon->describe | filter::legal_characters) == mon->describe)
                    << "description of monster" << ItemDefs[i].abr << "at free" << free_turns
                    << "loses characters on reload:" << mon->describe;
            }
        }
    };

    // Regression: the Dread Admiral's description was written with an em dash.
    "the Dread Admiral description survives the save and load character filter"_test = []
    {
        UnitTestHelper helper;
        helper.initialize_game();
        ARegion *r = helper.get_region(0, 2, 0);

        Unit *admiral = helper.create_monster(r, I_PIRATE_KING, 1);
        admiral->UpdateMonsterDescription();

        expect(!admiral->describe.empty()) << "the Admiral must have a description";
        expect(first_non_ascii(admiral->describe) == std::string::npos)
            << "Admiral description must be plain ASCII:" << admiral->describe;
        expect((admiral->describe | filter::legal_characters) == admiral->describe)
            << "Admiral description must not change on reload:" << admiral->describe;
        expect(admiral->describe.find("  ") == std::string::npos)
            << "no doubled space (the scar a filtered-out character leaves):" << admiral->describe;
    };
};
