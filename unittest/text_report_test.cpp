#include "../external/boost/ut.hpp"
#include "../external/nlohmann/json.hpp"

using json = nlohmann::json;

#include "../faction.h"                 // TEMPLATE_SHORT
#include "../text_report_generator.hpp"

#include <sstream>
#include <string>

namespace ut = boost::ut;
using namespace std;

// Regression: a repeating order with a long trailing ';' comment must stay on one
// physical line in the orders template, otherwise the wrapped continuation becomes
// an invalid order when the player resubmits the template.
ut::suite<"Text Report Template"> text_report_suite = []
{
  using namespace ut;

  "Long order lines are not wrapped in the template"_test = []
  {
    string long_order =
        "@transport 6222 all wood except 10 ; ERROR: No WOOD to transport from target unit";

    json unit = {
        { "number", 6535 },
        { "own_unit", true },
        { "orders", json::array({ { { "order", long_order } } }) }
    };

    json region = {
        { "present", true },
        { "terrain", "plain" },
        { "coordinates", { { "x", 0 }, { "y", 0 }, { "label", "surface" } } },
        { "province", "Testville" },
        { "units", json::array({ unit }) }
    };

    json report = {
        { "number", 1 },
        { "administrative", { { "password_unset", true } } },
        { "regions", json::array({ region }) }
    };

    stringstream ss;
    TextReportGenerator generator;
    generator.output_template(ss, report, TEMPLATE_SHORT, false);

    string output = ss.str();

    // Whole order (command + comment) on a single physical line.
    expect(output.find(long_order) != string::npos)
        << "order line should not be split across physical lines";
    // No orphaned indented continuation fragment.
    expect(output.find("\n  target unit") == string::npos)
        << "no orphaned continuation line";
  };
};

// Builds the smallest report the generator will accept: one region with one unit and one
// structure, optionally carrying a description. A null description means "key absent".
static json build_description_report(const json& unit_description, const json& structure_description) {
  json unit = {
    { "name", "Scout" },
    { "number", 42 },
    { "own_unit", true },
    { "flags", { { "guard", false } } },
    { "items", json::array({
      { { "name", "leader" }, { "plural", "leaders" }, { "tag", "LEAD" }, { "amount", 1 } }
    }) }
  };
  if (!unit_description.is_null()) unit["description"] = unit_description;

  json structure = {
    { "name", "Ancient Ruins of Dregrod Wetland" },
    { "number", 1 },
    { "type", "Dungeon Entrance" },
    { "inner_location", true }
  };
  if (!structure_description.is_null()) structure["description"] = structure_description;

  json region = {
    { "terrain", "plain" },
    { "coordinates", { { "x", 0 }, { "y", 0 }, { "label", "surface" } } },
    { "province", "Testville" },
    { "products", json::array() },
    { "exits", json::array() },
    { "units", json::array({ unit }) },
    { "structures", json::array({ structure }) }
  };

  return json{ { "regions", json::array({ region }) } };
}

static string generate_report(const json& report) {
  stringstream ss;
  TextReportGenerator generator;
  generator.output(ss, report, false);
  ss.flush();
  return ss.str();
}

// Regression: the generator terminates every unit/structure line with its own '.', so a
// description that is already a full sentence used to render as "... is slain..".
ut::suite<"Text Report Descriptions"> text_report_description_suite = []
{
  using namespace ut;

  "a description ending in a period does not double the terminating period"_test = []
  {
    string output = generate_report(build_description_report(
      "A grizzled veteran of the southern wars. Full treasure trove.",
      "Opened in October, Year 6. Ancient and unstable, such rifts rarely endure."
    ));

    expect(output.find("..") == string::npos) << "no doubled period anywhere in the report";
    expect(output.find("Full treasure trove.") != string::npos) << "unit description still terminated";
    expect(output.find("rarely endure.") != string::npos) << "structure description still terminated";
  };

  "a description without terminal punctuation is left alone"_test = []
  {
    string output = generate_report(build_description_report("a battered old scout", "carved from black stone"));

    expect(output.find("; a battered old scout.") != string::npos) << "unit description terminated once";
    expect(output.find("; carved from black stone.") != string::npos) << "structure description terminated once";
  };

  "'!' and '?' are preserved"_test = []
  {
    string output = generate_report(build_description_report("Do not approach!", "Who built it?"));

    expect(output.find("; Do not approach!") != string::npos) << "exclamation kept";
    expect(output.find("; Who built it?") != string::npos) << "question mark kept";
  };

  "a description of nothing but periods produces no dangling clause"_test = []
  {
    string output = generate_report(build_description_report("...", "  .  "));

    expect(output.find("; .") == string::npos) << "no dangling '; .' clause";
    expect(output.find("; \n") == string::npos) << "no dangling empty clause";
    expect(output.find("Scout (42)") != string::npos) << "unit line still generated";
    expect(output.find("Ancient Ruins of Dregrod Wetland [1]") != string::npos)
        << "structure line still generated";
  };

  "structures without a description are unaffected"_test = []
  {
    string output = generate_report(build_description_report(json(), json()));

    expect(output.find("; ") == string::npos) << "no description clause emitted";
    // The structure line is longer than the 70 column wrap, so match its wrapped tail.
    expect(output.find("Dungeon Entrance, contains an") != string::npos) << "structure line generated";
    expect(output.find("inner location.\n") != string::npos) << "structure line terminated normally";
  };

  "a fleet description is terminated once"_test = []
  {
    json report = build_description_report(json(), json());
    json& structure = report["regions"][0]["structures"][0];
    structure = {
      { "name", "Pale Kraken" },
      { "number", 200 },
      { "type", "Fleet" },
      { "ships", json::array({ { { "name", "Cog" }, { "plural", "Cogs" }, { "number", 1 } } }) },
      { "description", "Riding low in the water." }
    };

    string output = generate_report(report);

    expect(output.find("..") == string::npos) << "no doubled period on the fleet line";
    expect(output.find("; Riding low in the water.") != string::npos) << "fleet description terminated once";
  };
};
