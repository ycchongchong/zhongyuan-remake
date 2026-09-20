#pragma once
#include "original_rom.hpp"
namespace zhongyuan {
class OriginalAi;
// Byte-exact state for independently ported original commands. Does not execute 6502 code.
// Kept separate from the provisional campaign until all interacting rules are ported.
class OriginalState {
public:
    OriginalState(const OriginalRom &rom, std::vector<std::uint8_t> sram);
    Json city(int id) const;
    Json officer(int id) const;
    Json snapshot() const;
    Json unification_summary() const;
    void set_players(int first, int second = -1);
    void begin_ruler_turn(int ruler);
    // D373-D437 only: calendar/control component, before other monthly systems.
    void advance_calendar(std::uint8_t &random_cursor);
    void settle_month(std::uint8_t &random_cursor);
    std::string move_officers(int source, int destination, const std::vector<int> &officers);
    std::string dispatch_search(int city, int officer);
    std::string recruit_reserves(int city,int hundreds);
    std::string assign_troops(int city,int officer,int hundreds);
    Json expedition_quote(int source,int target,const std::vector<int> &officers,int leader) const;
    Json dispatch_expedition(int source,int target,const std::vector<int> &officers,int leader);
    // B715-B760: collect residents into the defender ledger; no deployment yet.
    Json collect_defenders(int target);
    void auto_deploy_defenders(int target);
    void auto_deploy_attackers(int target);
    void finish_defense_deployment(int target);
    int prepare_deployment(int source,int target,bool defending=false,bool two_players=false);
    void start_deployment_unit(int target,int slot,bool defending=false);
    std::string move_deployment_unit(int target,int slot,int direction,bool defending=false);
    Json deployment_area(int target,bool defending=false) const;
    bool valid_deployment(int source,int target,int current) const;
    bool valid_defense_deployment(int source,int target,int current,const std::vector<std::uint8_t> &attackers) const;
    bool valid_pvp_deployment(int source,int target,int current,bool defending,bool handover) const;
    Json retreat_attacker(int slot);
    Json retreat_defender(int slot, int target, bool human, std::uint8_t &cursor);
    Json withdrawal_result_step(int target,int source,int stage,Json &captives,std::uint8_t &cursor);
    Json retreat_army_step(std::uint8_t &cursor);
    void close_battle(int target);
    int tactical_mobility() const;
    Json player_tactical_strategies(int side,int slot) const;
    Json quote_tactical_strategy(int side,int slot,int target_slot,int strategy,int points) const;
    Json tactical_role_return(int argument,int points,const Json &status);
    Json tactical_empty_role_return(int argument,int points,const Json &status);
    Json tactical_ai_scan(int side,int previous,int marker) const;
    Json tactical_ai_flank(int target,int slot,int points,const Json &status);
    Json tactical_ai_motion(int target,int slot,int points,const Json &status,bool nearby=false,int *scratch21=nullptr,int side=0,int enemy_commander=-1) const;
    Json tactical_ai_occupied_fort(int target,int slot,int points,const Json &status,std::uint8_t &cursor,int scratch21=-1);
    Json tactical_ai_attacker_plan(int target,int previous_slot,bool reuse_unit=false);
    Json tactical_ai_fort(int target,int previous_slot,int points,const Json &status,bool reuse_unit=false);
    Json tactical_ai_strategy(int target,int slot,int points,const Json &status,std::uint8_t &cursor,bool direct=false,int *scratch21=nullptr,int side=0,int round=0);
    Json resolve_tactical_strategy(int city,int side,int slot,int target_slot,int strategy,int frame,const Json &status,std::uint8_t &cursor);
    Json tactical_ai_assessment(int side,int round,int points,std::uint8_t &cursor);
    Json tactical_end_turn(const Json &context) const;
    Json tactical_handover(const Json &context) const;
    Json human_failure_step(int loser,int winner,int inputs) const;
    Json time_limit_step(int stage,int human_mask,int speaker) const;
    Json annex_ruler_city(int city,int loser,int winner,std::uint8_t &cursor);
    std::string tactical_step(int slot,int direction,int &points,int side=128);
    std::string tactical_formation(int slot,int direction,int side=128);
    Json tactical_scout(int slot,int cell,int &points,int side=128) const;
    Json tactical_attack(int slot,int direction,int &points,int side=128) const;
    Json prepare_clash(const Json &attack,bool human_defender,std::uint8_t frame,bool human_attacker=true);
    Json clash_units(int first,int second) const;
    Json initialize_clash(const Json &context);
    Json clash_step(const Json &runtime,const Json &context,std::uint8_t &cursor);
    Json clash_menu(const Json &orders,const Json &context,int input) const;
    Json clash_order(const Json &units,const Json &orders,int active,int second_token,int command,std::uint8_t &cursor) const;
    std::string clash_move(Json &units,int active,int direction);
    Json clash_attack(const Json &units,int active,int command,int previous_target);
    Json clash_end_action(const Json &units,int first,int second,int scene,int active,int extra,bool moved);
    Json clash_projectile(const Json &units,int active,int direction,const Json &flight,bool launch);
    std::string clash_resolve_hit(Json &units,int active,int target,int orientation,int tactical_side);
    Json clash_handover(const Json &units,const Json &runtime) const;
    Json clash_strategy(const Json &units,const Json &orders,const Json &context,std::uint8_t &cursor) const;
    Json resolve_clash_strategy(const Json &units,const Json &orders,const Json &context,std::uint8_t &cursor) const;
    Json clash_retreat_injury(Json &units,int active,std::uint8_t &cursor) const;
    Json clash_retreat_desertion(Json &units,const Json &context,std::uint8_t &cursor);
    Json clash_retreat_confirm(Json &units,const Json &context,std::uint8_t &cursor);
    std::string restore_tactical_terrain(int target);
    std::string repaint_tactical_unit(int token);
    std::string restore_tactical_board(int target);
    Json clash_surrender(Json &units,Json &captives,const Json &context,std::uint8_t &cursor);
    Json clash_general_defeat(int first,int second,int active,int orientation,int tactical_side);
    Json finish_clash_defeat(const Json &units,const Json &context);
    Json duel_strike(Json &units,const Json &context,std::uint8_t &cursor) const;
    Json duel_ai(const Json &units,const Json &context,std::uint8_t &cursor) const;
    Json duel_finish(Json &units,Json &captives,const Json &context,std::uint8_t &cursor);
    // A7DF-A8B0 only: damage settlement, before the death animation removes units.
    std::string clash_damage(Json &units,int first,int second,int attacker,int target,std::uint8_t &cursor) const;
    std::string settle_clash(const Json &units,int first,int second,int first_token,int second_token,int target_city);
    Json execute_command(int city,const std::string &kind,const Json &args,std::uint8_t &cursor,std::uint8_t frame);
    // Return collection is triggered by entering the destination city.
    int collect_search(int city);
    int search_kind(int officer, std::uint8_t &cursor, bool exclude_people = false) const;
    Json search_find(int city, int kind, std::uint8_t &cursor);
    bool recruit_search(int city, int officer, int kind, int cost, std::uint8_t &cursor);
    // Proposal selection and commit are separate: original UI timing determines
    // the sequence cursor and the frame-counter byte, not a fixed PRNG seed.
    Json development_options(int city, int officer, int kind) const;
    Json next_development_offer(int city, int officer, int kind, std::uint8_t &sequence_cursor) const;
    std::string develop(int city, int officer, int kind, int proposal, std::uint8_t frame_counter);
    const std::vector<std::uint8_t> &sram() const { return bytes_; }
    Json begin_ai() const;
    Json ai_phase(const Json &runtime);
    void finish_ai(const Json &result);
    bool next_ruler(std::uint8_t &random_cursor);
    void end_orders();
    bool human_turn() const;
    Json ending() const;
private:
    friend class OriginalAi;
    void auto_deploy(int target,bool attacking);
    OriginalRom rom_;
    std::vector<std::uint8_t> bytes_;
    Json roads_;
    Json development_;
    Json commands_;
    bool resident(int city, int officer) const;
};
}
