#pragma once
#ifndef TEST_HELPER_HPP
#define TEST_HELPER_HPP

#include "../game.h"

#include <algorithm>
#include <iostream>
#include <sstream>

// In order to allow testing spells, this is a generic structure to capture the Run<Spell> possible arguments.
// Each spell takes a different number of arguments, though most of them take just the region and the unit.
// However, some take an object and one or two integer values as well.
struct SpellTestHelper {
    ARegion *region;
    Unit *unit;
    Object *object;
    int val1;
    int val2;
};

// This class is used to access private members of Game for testing.
// It also provides utilities to capture the output of cout into a string, as well as provide
// utility functions to manipulate the game object for testing.
//
// It does not currently provide a mechanism to to force input into cin, nor to capture files that the
// various game functions might open/write/read for more extensive end to end tests.
// The intent would be to create an integration/end-to-end test suite that used much of the same unit test
// framework, but a much more involved helper class to allow interception of those sorts of inputs/outputs
// in the future if needed.
class UnitTestHelper {
public:
    UnitTestHelper();
    ~UnitTestHelper();

    // initialize a game
    int initialize_game();
    // Set up the turn so that month gets incremented, etc.
    void setup_turn();
    // Initialize data needed for reports, such as which factions are in which regions.
    void setup_reports();

    // Some functions need to pass the game object in, so.
    Game& game_object();

    // Just get a total count of regions so we can make sure we got the world we exepected.
    int get_region_count();
    // Create a faction that we can use for testing.
    Faction *create_faction(std::string name);
    // Get any existing faction
    Faction *get_faction(int id);
    // Get a region by coordinates
    ARegion *get_region(int x, int y, int level);
    // Get all regions
    ARegionList& get_regions() { return game.regions; }
    // get the game month
    int get_month() { return game.month; }
    // Find the first unit for a given faction
    Unit *get_first_unit(Faction *faction);
    // Create a unit for the given faction in the given region.
    Unit *create_unit(Faction *faction, ARegion *region);
    // Create a fleet in the given region.
    void create_fleet(ARegion *region, Unit *owner, int ship_type, int ship_count);
    // Create a building in the given region.
    void create_building(ARegion *region, Unit *owner, int building_type);
    // Parse the orders contained in the input stream.
    void parse_orders(int faction_id, std::istream& orders, orders_check *check = nullptr);
    // Run the transport order checks
    void check_transport_orders();
    // Execute movement orders
    void move_units();
    // Run enter orders
    void run_enter();
    // Run destroy orders
    void run_destroy();
    // Run one phase of the transport orders
    void transport_phase(TransportOrder::TransportPhase phase);
    // Collect the transported goods from the quartermasters
    void collect_transported_goods();
    // connected distance
    int connected_distance(ARegion *reg1, ARegion *reg2, int penalty, int max);
    // Run the maintenance phase
    void maintain_units();
    // Run sacrifice orders
    void run_sacrifice();
    // Run annihilation orders
    void run_annihilation();
    // Run pirate raid on buildings for a specific unit
    void run_pirate_raid(ARegion *r, Unit *u);
    // Run pirate land crew recruitment for all fleets
    void run_pirate_recruit_land_crew();
    // Run pirate empty ship seizure for all regions
    void run_pirate_seize_empty_ships();
    // Create an empty fleet (no units) in region with the given ship type and name
    Object *create_empty_fleet(ARegion *region, int ship_type, const std::string& fleet_name = "Ship");
    // Run a battle between attacker and target; returns BATTLE_WON/BATTLE_LOST/BATTLE_IMPOSSIBLE
    int run_battle(ARegion *r, Unit *attacker, Unit *target);
    // Create an NPC pirate fleet (O_FLEET + galleon + pirate unit) in region; returns the pirate unit
    Unit *create_npc_pirate_fleet(ARegion *region, int pirate_count = 1);
    // Create an NPC pirate captain unit inside an existing fleet object
    Unit *create_npc_pirate_captain(ARegion *region, Object *fleet);
    // Create a wandering monster pirate unit in the given region
    Unit *create_pirate_unit(ARegion *region, int count);
    // Enable ruleset specific data for testing
    void set_ruleset_specific_data(const json &data);
    // Run productions
    void run_productions();
    // Run all month orders for all factions.
    void run_month_orders();
    // Run all buy orders.
    void run_buy_orders();
    // Run all sell orders.
    void run_sell_orders();
    // Set a skill level for a unit, including all prerequisites
    void set_skill_level(Unit *unit, int skill, int level);
    // Run Grow() on a specific region (private method, exposed for testing)
    void run_grow(ARegion *region) { region->Grow(); }
    // Run AutoNameSoloUnits() (private method, exposed for testing)
    void run_auto_name_solo_units() { game.AutoNameSoloUnits(); }
    // Run ProcessDungeons() — dungeon lifecycle (boss death, DYING timer, collapse)
    void run_process_dungeons() { game.ProcessDungeons(); }
    // Run AdjustCityMons() on a region — spawns/equips/cleans up city guards and mayor
    void run_adjust_city_mons(ARegion *r) { game.AdjustCityMons(r); }
    // Spawn a mayor in the given region (test bootstrap; no guards needed)
    void spawn_mayor(ARegion *r, Object *target = nullptr) { game.CreateMayor(r, target); }
    // Get guardfaction id (faction 1)
    int get_guardfaction() { return game.guardfaction; }
    // Quest system helpers
    std::list<Faction*>& get_factions() { return game.factions; }
    void run_generate_quests_for_mayor(ARegion *r, Unit *mayor) {
        game.GenerateLocalQuestsForMayor(r, mayor);
    }
    // Remove all monfaction units from every object in every domain region.
    // Use before quest generation tests to ensure pre-spawned world monsters
    // (from NewGame CreateWMons/CreateLMons) don't compete for hunt slots.
    void clear_domain_monsters(ARegion *city) {
        std::set<int> domain = game.ComputeMayorDomain(city);
        int mf = game.monfaction;
        for (int rnum : domain) {
            ARegion *dr = game.regions.GetRegion(rnum);
            if (!dr) continue;
            for (auto o : dr->objects) {
                for (auto it = o->units.begin(); it != o->units.end(); ) {
                    if ((*it)->faction->num == mf)
                        it = o->units.erase(it);
                    else
                        ++it;
                }
            }
        }
    }
    void run_steal_orders() { game.RunStealthOrders(); }
    // Remove all NPC faction units from a specific region (guard + monster factions).
    // Needed in steal/assassinate tests to prevent city guards or wandering monsters
    // from appearing in CanSeeSteal seers and triggering unexpected succ=0.
    void clear_npc_units_in_region(ARegion *r) {
        for (auto o : r->objects) {
            for (auto it = o->units.begin(); it != o->units.end(); ) {
                if ((*it)->faction->is_npc)
                    it = o->units.erase(it);
                else
                    ++it;
            }
        }
    }
    void run_expire_local_quests()    { game.ExpireLocalQuests(); }
    void run_update_quest_awareness() { game.UpdateQuestAwareness(); }
    void run_quest_orders()           { game.RunQuestOrders(); }
    std::set<int> compute_mayor_domain(ARegion *r) { return game.ComputeMayorDomain(r); }
    // Create a monfaction unit with one race item in a region (dummy object).
    Unit *create_monster(ARegion *region, int race_item, int count) {
        Faction *mon = get_faction(game.monfaction);
        Unit *u = create_unit(mon, region);
        u->type = U_WMON;
        u->items.SetNum(race_item, count);
        return u;
    }
    // Dungeon test helpers
    void inject_dungeon(const DungeonInstance &d) { game.activeDungeons.push_back(d); }
    void run_populate_dungeon(const DungeonInstance &d) { game.populate_dungeon(d); }
    int get_monfaction() { return game.monfaction; }
    size_t dungeon_count() { return game.activeDungeons.size(); }
    DungeonInstance get_dungeon(size_t idx) { return game.activeDungeons[idx]; }
    bool dungeons_empty() { return game.activeDungeons.empty(); }
    int turn_number() { return game.TurnNumber(); }

    // dummy
    int get_seed() { return rng::get_random(10000); };

    // Get the contents of the log stream as a string.
    std::string log_output();

    // Activate a specific spell for testing.   Since the spells can have different arguments, we use a helper struct
    // to pass in the arguments and call the appropriate Run<Spell> function based on the skill associate with the spell.
    void activate_spell(int spell, SpellTestHelper helper);

    enum Type { SKILL, ITEM, OBJECT};
    // enable or disable items, objects etc
    void enable(Type type, int id, bool enable);

private:
    void set_prerequisites(Unit *unit, int skill, int level);
    int calculate_days_for_level(int level);

    std::stringstream log_stream;
    Game game;
};

#endif // TEST_HELPER_HPP
