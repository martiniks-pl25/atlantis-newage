#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "testhelper.hpp"
#include "orders.h"

namespace ut = boost::ut;

// The same student listed more than once in a TEACH order ("TEACH 1111 1112 1112 1113")
// must be collapsed before students are counted: a duplicate used to consume a second
// share of the teacher's days, and the legacy AList dedup walked through a freed list
// node when the repeated unit followed its twin.
ut::suite<"teach_order"> teach_order_suite = [] {
    using namespace ut;

    "TEACH with a duplicated student teaches it once"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        Faction *faction = helper.create_faction("Test Faction");
        // The starting unit is one leader with combat 3, so it can teach combat.
        Unit *teacher = helper.get_first_unit(faction);
        ARegion *region = teacher->object->region;

        Unit *student = helper.create_unit(faction, region);
        Unit *other = helper.create_unit(faction, region);
        student->SetMen(I_LEADERS, 10);
        other->SetMen(I_LEADERS, 10);

        std::stringstream ss;
        ss << "#atlantis " << faction->num << "\n";
        ss << "unit " << student->num << "\n";
        ss << "study combat\n";
        ss << "unit " << other->num << "\n";
        ss << "study combat\n";
        ss << "unit " << teacher->num << "\n";
        // The repeated student sits next to its twin, as in the original crash report.
        ss << "teach " << student->num << " " << student->num << " " << other->num << "\n";
        helper.parse_orders(faction->num, ss, nullptr);

        helper.game_object().RunTeachOrders();

        expect(faction->errors.empty()) << "a duplicated student is not a TEACH error";

        int teach_events = 0;
        for (const auto &ev : faction->events) {
            if (ev.category == "teach") teach_events++;
        }
        expect(teach_events == 2_i) << "the duplicated student is taught once, not twice";

        StudyOrder *first = dynamic_cast<StudyOrder *>(student->monthorders);
        StudyOrder *second = dynamic_cast<StudyOrder *>(other->monthorders);
        expect(first != nullptr && second != nullptr) << "students keep their STUDY orders";
        if (!first || !second) return;

        // 30 days * 1 teacher man * STUDENTS_PER_TEACHER (10) = 300 days, split evenly
        // between the two unique students of 10 men each.
        expect(first->days == 150_i) << "the duplicate must not consume a second share";
        expect(second->days == 150_i);
    };
};
