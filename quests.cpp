#include "quests.h"
#include "events.h"
#include "gamedata.h"
#include "game.h"
#include "object.h"
#include "aregion.h"
#include "quest_data.h"
#include "rng.hpp"
#include "logger.hpp"
#include <iterator>
#include <memory>

QuestList quests;

Quest::Quest()
{
    type = -1;
    target = -1;
    objective.type = -1;
    objective.num = 0;
    building = -1;
    regionnum = -1;
    regionname = "-";
}

Quest::~Quest()
{
    rewards.clear();
}

std::string Quest::get_rewards()
{
    std::string quest_rewards;
    bool first = true;

    quest_rewards = "Quest rewards: ";

    if (rewards.size() == 0) {
        return quest_rewards + "none.";
    }

    for(auto i: rewards) {
        if (!first) quest_rewards += ", ";
        first = false;
        quest_rewards += item_string(i.type, i.num);
    }
    quest_rewards += ".";

    return quest_rewards;
}

// Quest save format v5.2.7: flat list of ints per quest, fixed field order.
// Legacy formats (< 5.2.7) are no longer supported — server migrated 2026-05-14.

static int read_quests_v527(QuestList& list, std::istream& f, ATL_VER engine_version);

int QuestList::read_quests(std::istream& f, ATL_VER engine_version)
{
    quests.clear();
    return read_quests_v527(*this, f, engine_version);
}

static int read_quests_v527(QuestList& list, std::istream& f, ATL_VER engine_version)
{
    int count;
    f >> count;
    logger::write("  Quest count: " + std::to_string(count));
    if (count < 0) return 0;

    while (count-- > 0) {
        auto q = std::make_shared<Quest>();
        f >> q->type;
        f >> q->num;
        f >> q->scope;
        f >> q->subtype;
        f >> q->tokens;
        f >> q->issuer_unit;
        f >> q->issuer_region;
        f >> q->created_turn;
        f >> q->expires_turn;
        f >> q->target_monster;
        f >> q->kills_so_far;
        f >> q->amount_so_far;
        f >> q->target;
        f >> q->regionnum;
        f >> q->building;
        // Do NOT add a building-encoding migration here. Dungeon LOCAL_LAIR_CLEAR
        // quests always use the -(index+1) encoding, so a positive `building` is
        // never a dungeon index — it's an ObjectDefs index (e.g. O_FLEET=1). A past
        // migration that flipped positive values mis-fired on pirate-fleet lair quests.
        // regionname persisted since 5.2.8; older saves leave the constructor default "-",
        // which Game::ReadGame migrates from issuer_region after both regions and quests load.
        if (engine_version >= MAKE_ATL_VER(5, 2, 8))
            std::getline(f >> std::ws, q->regionname);
        list.push_back(q);
    }

    int trailing;
    f >> trailing;  // sentinel
    return 1;
}

void QuestList::write_quests(std::ostream& f)
{
    f << quests.size() << '\n';
    for (auto q : quests) {
        f << q->type           << '\n';
        f << q->num            << '\n';
        f << q->scope          << '\n';
        f << q->subtype        << '\n';
        f << q->tokens         << '\n';
        f << q->issuer_unit    << '\n';
        f << q->issuer_region  << '\n';
        f << q->created_turn   << '\n';
        f << q->expires_turn   << '\n';
        f << q->target_monster << '\n';
        f << q->kills_so_far   << '\n';
        f << q->amount_so_far  << '\n';
        f << q->target         << '\n';
        f << q->regionnum      << '\n';
        f << q->building       << '\n';
        f << q->regionname     << '\n';
    }
    f << 0 << '\n';  // sentinel
}

std::string QuestList::distribute_rewards(Unit *u, std::shared_ptr<Quest> q)
{
    for(auto i: q->rewards) {
        u->items.SetNum(i.type, u->items.GetNum(i.type) + i.num);
        u->faction->DiscoverItem(i.type, 0, 1);
    }
    return q->get_rewards();
}

int QuestList::check_kill_target(Unit *u, ItemList& spoils, std::string *quest_rewards,
                                 int *out_issuer_region, Events *events,
                                 int *out_quest_num, std::string *out_unaware_msg)
{
    for (auto q : quests) {
        bool match = false;
        if (q->subtype == Quest::LOCAL_HUNT ||
            q->subtype == Quest::LOCAL_LAIR_CLEAR ||
            q->subtype == Quest::GLOBAL_BOSS_HUNT) {
            match = (q->target == u->num);
        }
        if (!match) continue;

        // Inject I_BOUNTY tokens into battle spoils; Army::Win distributes them
        // among winners and credits quest_debts proportionally.
        spoils.SetNum(I_BOUNTY, spoils.GetNum(I_BOUNTY) + q->tokens);
        // q->regionname holds the mayor's settlement name (set at quest creation).
        // Pre-5.2.8 saves resurfaced as constructor default "-" before migration ran;
        // treat that sentinel the same as empty so the text never reads "of -".
        bool is_global = (q->subtype == Quest::GLOBAL_BOSS_HUNT);
        bool name_known = !q->regionname.empty() && q->regionname != "-";
        std::string settlement = name_known ? ("the mayor of " + q->regionname) : "the mayor";
        std::string token_str  = std::to_string(q->tokens) + " Bounty Token" +
                                 (q->tokens != 1 ? "s" : "");
        if (is_global) {
            *quest_rewards = "Global bounty completed: +" + token_str
                + ". Tokens may be redeemed at any Town Hall.";
        } else {
            *quest_rewards = "Bounty quest for " + settlement + " completed: +" + token_str + ".";
        }
        if (out_unaware_msg) {
            // Surprise-discovery wording for factions that never read the notice board.
            *out_unaware_msg = "Found a bounty notice on the slain creature - " +
                               (is_global ? std::string("any mayor")
                                          : settlement)
                               + " will pay " + token_str + " for this deed.";
        }
        if (out_issuer_region) *out_issuer_region = q->issuer_region;
        if (out_quest_num)     *out_quest_num     = q->num;

        // Gazette event — skip dungeon LOCAL_LAIR_CLEAR (BOSS_KILLED DungeonFact fires instead).
        bool is_dungeon = (q->subtype == Quest::LOCAL_LAIR_CLEAR && q->building < 0);
        if (events && !is_dungeon) {
            auto *f = new QuestCompletedFact();
            f->subtype     = q->subtype;
            f->target_name = u->name;
            f->settlement  = q->regionname;
            events->AddFact(f);
        }

        erase_with_cleanup(q);
        return 1;
    }
    return 0;
}

int QuestList::check_harvest_target(ARegion *r, int item, int harvested, int max, Unit *u, std::string *quest_rewards)
{
    for(auto q: quests) {
        if (q->type == Quest::HARVEST && q->regionnum == r->num && q->objective.type == item) {
            if (rng::get_random(max) < harvested) {
                *quest_rewards = distribute_rewards(u, q);
                erase(q); // this is safe since we immediately return and don't use the iterator again
                return 1;
            }
        }
    }
    return 0;
}

int QuestList::check_build_target(ARegion *r, int building, Unit *u, std::string *quest_rewards)
{
    for(auto q: quests) {
        if (q->type == Quest::BUILD && q->building == building && q->regionname == r->name) {
            *quest_rewards = distribute_rewards(u, q);
            erase(q); // this is safe since we immediately return and don't use the iterator again
            return 1;
        }
    }
    return 0;
}

int QuestList::check_road_quest(ARegion *r, int road_type, Unit *u, std::string *quest_rewards, ARegionList& regions, Events *events)
{
    std::shared_ptr<Quest> found;
    for (const auto& q : quests) {
        if (q->subtype != Quest::LOCAL_BUILD_ROAD) continue;
        if (q->regionnum != r->num) continue;
        if (q->building != road_type) continue;
        found = q;
        break;
    }
    if (!found) return 0;

    int tok = found->tokens;
    u->items.SetNum(I_BOUNTY, u->items.GetNum(I_BOUNTY) + tok);
    u->faction->quest_debts[found->issuer_region] += tok;

    // Find the mayor's settlement name from the issuer region.
    ARegion *ir = regions.GetRegion(found->issuer_region);
    std::string settlement = ir && ir->town ? ir->town->name : (ir ? ir->name : "unknown");

    if (quest_rewards) {
        if (!quest_rewards->empty()) *quest_rewards += " ";
        *quest_rewards += "Road quest for the mayor of " + settlement + " completed: +" +
                          std::to_string(tok) + " Bounty Token" + (tok != 1 ? "s" : "") + ".";
    }

    if (events) {
        auto *f = new QuestCompletedFact();
        f->subtype    = Quest::LOCAL_BUILD_ROAD;
        f->settlement = settlement;
        events->AddFact(f);
    }

    logger::write("[quest] road quest #" + std::to_string(found->num) +
                  " completed by " + u->name + " in " + r->short_print());
    erase_with_cleanup(found);
    return 1;
}

int QuestList::check_tower_quest(ARegion *r, Unit *u, std::string *quest_rewards, ARegionList& regions, Events *events)
{
    std::shared_ptr<Quest> found;
    for (const auto& q : quests) {
        if (q->subtype != Quest::LOCAL_BUILD_TOWER) continue;
        if (q->regionnum != r->num) continue;
        found = q;
        break;
    }
    if (!found) return 0;

    int tok = found->tokens;
    u->items.SetNum(I_BOUNTY, u->items.GetNum(I_BOUNTY) + tok);
    u->faction->quest_debts[found->issuer_region] += tok;

    // q->regionname holds the mayor's settlement name (set at quest creation).
    // Pre-5.2.8 saves resurfaced as constructor default "-" before migration ran;
    // treat that sentinel the same as empty.
    std::string settlement = (found->regionname.empty() || found->regionname == "-")
                             ? "unknown" : found->regionname;

    if (quest_rewards) {
        if (!quest_rewards->empty()) *quest_rewards += " ";
        *quest_rewards += "Tower quest for the mayor of " + settlement + " completed: +" +
                          std::to_string(tok) + " Bounty Token" + (tok != 1 ? "s" : "") + ".";
    }

    if (events) {
        auto *f = new QuestCompletedFact();
        f->subtype    = Quest::LOCAL_BUILD_TOWER;
        f->settlement = settlement;
        events->AddFact(f);
    }

    logger::write("[quest] tower quest #" + std::to_string(found->num) +
                  " completed by " + u->name + " in " + r->short_print());
    erase_with_cleanup(found);
    return 1;
}

int QuestList::check_inn_quest(ARegion *r, Unit *u, std::string *quest_rewards, ARegionList& regions, Events *events)
{
    std::shared_ptr<Quest> found;
    for (const auto& q : quests) {
        if (q->subtype != Quest::LOCAL_BUILD_INN) continue;
        if (q->regionnum != r->num) continue;
        found = q;
        break;
    }
    if (!found) return 0;

    int tok = found->tokens;
    u->items.SetNum(I_BOUNTY, u->items.GetNum(I_BOUNTY) + tok);
    u->faction->quest_debts[found->issuer_region] += tok;

    // q->regionname holds the mayor's settlement name (set at quest creation).
    // Pre-5.2.8 saves resurfaced as constructor default "-" before migration ran;
    // treat that sentinel the same as empty.
    std::string settlement = (found->regionname.empty() || found->regionname == "-")
                             ? "unknown" : found->regionname;

    if (quest_rewards) {
        if (!quest_rewards->empty()) *quest_rewards += " ";
        *quest_rewards += "Inn quest for the mayor of " + settlement + " completed: +" +
                          std::to_string(tok) + " Bounty Token" + (tok != 1 ? "s" : "") + ".";
    }

    if (events) {
        auto *f = new QuestCompletedFact();
        f->subtype    = Quest::LOCAL_BUILD_INN;
        f->settlement = settlement;
        events->AddFact(f);
    }

    logger::write("[quest] inn quest #" + std::to_string(found->num) +
                  " completed by " + u->name + " in " + r->short_print());
    erase_with_cleanup(found);
    return 1;
}

int QuestList::check_visit_target(ARegion *r, Unit *u, std::string *quest_rewards)
{
    std::set<std::string> intersection;

    for(auto q: quests) {
        if (q->type != Quest::VISIT) continue;
        if (!q->destinations.count(r->name)) continue;
        for(const auto o : r->objects) {
            if (o->type == q->building) {
                u->visited.insert(r->name);
                intersection.clear();
                set_intersection(
                    q->destinations.begin(),
                    q->destinations.end(),
                    u->visited.begin(),
                    u->visited.end(),
                    inserter(intersection, intersection.begin()),
                    std::less<std::string>()
                );
                if (intersection.size() == q->destinations.size()) {
                    // This unit has visited the required buildings in all those regions, so they completed a quest
                    *quest_rewards = distribute_rewards(u, q);
                    erase(q); // this is safe since we immediately return and don't use the iterator again
                    return 1;
                }
            }
        }
    }
    return 0;
}

int QuestList::check_demolish_target(ARegion *r, int building, Unit *u, std::string *quest_rewards)
{
    for(auto q: quests) {
        if (q->type == Quest::DEMOLISH && q->regionnum == r->num && q->target == building) {
            *quest_rewards = distribute_rewards(u, q);
            erase(q); // this is safe since we immediately return and don't use the iterator again
            return 1;
        }
    }
    return 0;
}

