#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "aregion.h"
#include "object.h"
#include "items.h"
#include "testhelper.hpp"

namespace ut = boost::ut;
using namespace std;

// ---------------------------------------------------------------------------
// Gazette "regalia" block: per-faction crown counts and declared capitals
// (Trident victory summary). Built by Game::BuildRegaliaJson(), emitted into
// the JSON newspaper next to settlement_stats. Crowns reuse CountItem() so a
// crown carried outside any town is still counted; capitals resolve
// Faction::capital_region to its town/region (no coordinates, by design).
// See docs/TRIDENT_VICTORY_MECHANIC_DESIGN.md.
// ---------------------------------------------------------------------------

namespace {
    // Find the regalia entry for a faction number in a crowns/capitals array.
    // Returns a null json (is_null()) when the faction is absent.
    json find_by_faction(const json& arr, int faction_num) {
        for (const auto& e : arr)
            if (e.value("faction_num", -1) == faction_num) return e;
        return json{};
    }
}

ut::suite<"RegaliaGazette"> regalia_gazette_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // Crowns are summed per faction across ALL regions (including outside a
    // town), ranked by count desc; factions without a crown are omitted.
    // -----------------------------------------------------------------------
    "crowns are counted world-wide and ranked"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        // Faction A holds 2 crowns in its starting (town) region.
        Faction *facA = helper.create_faction("House Crownward");
        Unit *aLeader = helper.get_first_unit(facA);
        aLeader->items.SetNum(I_CROWN, 2);

        // Faction B holds 1 crown on a unit in a region with no town/city,
        // proving crowns outside settlements are still counted.
        Faction *facB = helper.create_faction("Wandering Banner");
        ARegion *wild = helper.get_region(0, 2, 0);
        expect(wild != nullptr) << "test world must have region (0,2,0)";
        Unit *bUnit = helper.create_unit(facB, wild);
        bUnit->items.SetNum(I_CROWN, 1);

        // Faction C holds no crown at all → must not appear.
        Faction *facC = helper.create_faction("Crownless");
        (void)helper.get_first_unit(facC);

        helper.setup_reports();
        json regalia = helper.build_regalia_json();
        const json& crowns = regalia["crowns"];

        json a = find_by_faction(crowns, facA->num);
        json b = find_by_faction(crowns, facB->num);
        json c = find_by_faction(crowns, facC->num);

        expect(!a.is_null()) << "crown-holding faction A must be present";
        expect(!b.is_null()) << "crown-holding faction B must be present";
        expect(c.is_null())  << "crownless faction C must be omitted";

        expect(a.value("crowns", 0) == 2_i) << "A must report 2 crowns";
        expect(b.value("crowns", 0) == 1_i) << "B must report 1 crown (counted outside a town)";

        // Ranked by crown count desc: A (2) must precede B (1).
        int ia = -1, ib = -1;
        for (int i = 0; i < (int)crowns.size(); i++) {
            int fn = crowns[i].value("faction_num", -1);
            if (fn == facA->num) ia = i;
            if (fn == facB->num) ib = i;
        }
        expect(ia >= 0 && ib >= 0 && ia < ib) << "A (2 crowns) must rank before B (1 crown)";
    };

    // -----------------------------------------------------------------------
    // A declared capital resolves to its city + region (terrain & name, no
    // coordinates); factions without a capital are omitted.
    // -----------------------------------------------------------------------
    "capitals resolve to city and region"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Throne Holders");
        Unit *leader = helper.get_first_unit(fac);
        ARegion *r = leader->object->region;
        expect(r->town != nullptr) << "starting region must have a settlement";
        if (!r->town) return;
        fac->capital_region = r->num;   // emit-side test: set directly, bypass CAPITAL order

        // A second faction with no capital must not appear.
        Faction *other = helper.create_faction("No Throne");
        (void)helper.get_first_unit(other);

        helper.setup_reports();
        json regalia = helper.build_regalia_json();
        const json& capitals = regalia["capitals"];

        json cap = find_by_faction(capitals, fac->num);
        expect(!cap.is_null()) << "faction with a capital must be present";
        expect(cap.value("city", string()) == r->town->name)
            << "capital city must match the settlement name";
        expect(cap.contains("region")) << "capital must carry its region";
        expect(cap["region"].value("name", string()) == r->name)
            << "region name must match";
        expect(!cap["region"].value("terrain", string()).empty())
            << "region terrain must be populated";
        // No coordinates by design.
        expect(!cap["region"].contains("x")) << "capital region must not leak coordinates";

        expect(find_by_faction(capitals, other->num).is_null())
            << "faction without a capital must be omitted";
    };

    // -----------------------------------------------------------------------
    // No crowns and no capitals anywhere → both arrays present but empty.
    // -----------------------------------------------------------------------
    "empty regalia yields empty arrays"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *fac = helper.create_faction("Plain Folk");
        (void)helper.get_first_unit(fac);

        helper.setup_reports();
        json regalia = helper.build_regalia_json();

        expect(regalia.contains("crowns") && regalia["crowns"].is_array());
        expect(regalia.contains("capitals") && regalia["capitals"].is_array());
        // The newly created faction holds no crown and declared no capital.
        expect(find_by_faction(regalia["crowns"], fac->num).is_null())
            << "a faction with no crown must not appear";
        expect(find_by_faction(regalia["capitals"], fac->num).is_null())
            << "a faction with no capital must not appear";
    };
};
