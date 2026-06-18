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
