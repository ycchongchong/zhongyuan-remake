#pragma once
#include "original_state.hpp"
namespace zhongyuan {
// Native strategic controller; tactical battles retain their original ledger.
class OriginalSession {
public:
    static constexpr std::size_t HISTORY_SEGMENT_TARGET=768;
    static constexpr std::size_t MAX_HISTORY_SEGMENTS=256;
    static constexpr std::size_t MAX_SAVE_BYTES=32u*1024u*1024u;
    bool battle_history_needs_archive() const;
    std::string archive_battle_history();
    // Wrap one existing action. Archiving is committed only if it succeeds;
    // direct action methods retain their exact legacy replay semantics.
    template<class Action> std::string perform_battle_action(Action action){
        if(!battle_history_needs_archive())return action(*this);
        auto next=*this;
        auto error=next.archive_battle_history();if(!error.empty())return error;
        error=action(next);if(error.empty())*this=std::move(next);
        return error;
    }
    explicit OriginalSession(const OriginalRom &rom);
    void start(int ruler,int difficulty,int second = -1);
    Json advance();
    std::string end_turn();
    Json snapshot() const;
    // Presentation cue only; reading it never advances gameplay, RNG or history.
    std::string music_cue() const;
    Json prepare_development(int city,int officer,int kind);
    std::string confirm_development();
    void cancel_development(){pending_=nullptr;}
    std::string move(int source,int destination,const std::vector<int>&officers);
    std::string search(int city,int officer);
    Json expedition_quote(int source,int target,const std::vector<int> &officers,int leader) const;
    Json dispatch_expedition(int source,int target,const std::vector<int> &officers,int leader);
    std::string begin_tactical_retreat(int slot);
    std::string cancel_tactical_retreat();
    std::string confirm_tactical_retreat();
    std::string finish_tactical_retreat();
    std::string advance_withdrawal_result();
    std::string begin_defender_defeat_result();
    std::string begin_commander_defeat_result();
    std::string advance_time_limit_result();
    std::string begin_human_failure();
    std::string finish_human_failure();
    std::string begin_ruler_defeat_result();
    std::string advance_ruler_annexation();
    std::string finish_withdrawal_result();
    std::string begin_tactics();
    std::string end_tactical_turn();
    std::string resume_computer_attacker_retreat();
    std::string advance_computer_attack();
    std::string advance_computer_tactics();
    std::string continue_computer_role();
    std::string continue_empty_computer_role();
    std::string resume_computer_role_after_handover();
    bool can_continue_computer_role() const;
    std::string plan_computer_tactics(bool reuse_unit=false);
    std::string continue_computer_flank();
    std::string continue_computer_scan();
    std::string retry_computer_strategy();
    std::string continue_computer_motion();
    std::string continue_computer_nearby();
    std::string continue_computer_occupied_fort(bool recover_scratch=false);
    std::string finish_exhausted_computer_attack();
    std::string evaluate_computer_strategy();
    std::string execute_computer_strategy();
    std::string finish_computer_strategy();
    Json player_tactical_strategies(int slot) const;
    std::string prepare_tactical_strategy(int slot,int target_slot,int strategy);
    std::string cancel_tactical_strategy();
    std::string confirm_tactical_strategy();
    std::string finish_tactical_strategy();
    std::string select_tactical_unit(int slot);
    std::string move_tactical(int slot,int direction);
    std::string adjust_tactical_formation(int slot,int direction);
    std::string scout_tactical(int slot,int target_slot);
    std::string close_tactical_scout();
    std::string attack_tactical(int slot,int direction);
    std::string cancel_tactical_attack();
    std::string confirm_tactical_attack();
    std::string begin_clash();
    // Transient presentation event, emitted only for a committed live action.
    // Replay callers omit this output; sound never enters the save or RNG state.
    std::string advance_clash(std::string *sound=nullptr);
    std::string resume_clash_strategy();
    std::string recover_clash_strategy();
    std::string advance_clash_retreat();
    std::string advance_clash_defeat();
    std::string begin_duel();
    std::string choose_duel_command(int command);
    std::string answer_duel_surrender(bool accept);
    std::string advance_duel();
    std::string cycle_clash_order(int side,int kind);
    std::string request_clash_surrender();
    std::string answer_clash_surrender(bool accept);
    std::string advance_clash_surrender();
    std::string finish_clash_result();
    std::vector<std::uint8_t> clash_rgb() const;
    std::string prepare_deployment();
    std::string move_deployment(int direction);
    std::string confirm_deployment();
    Json execute_command(int city,const std::string &kind,const Json &args);
    std::string recruit(int city,int hundreds);
    std::string assign_troops(int officer,int hundreds);
    void finish_recruitment(){army_=nullptr;}
    Json visit_city(int city);
    Json finish_search(bool accept);
    void tick(int frames);
    Json save() const;
    std::vector<std::uint8_t> tactical_pixels() const;
    std::string restore(const Json &save);
private:
    std::string continue_computer_role_impl(bool empty_slot);
    std::string apply_computer_motion(Json decision,const char *event);
    int pending_clash_result_reason() const;
    bool can_settle_defender_defeat() const;
    bool can_settle_commander_defeat() const;
    bool can_settle_ruler_defeat() const;
    bool can_report_human_failure() const;
    int tactical_side() const;
    int first_tactical_slot() const;
    std::string tactical_action_error() const;
    std::string apply_duel_command(int command);
    OriginalRom rom_;
    OriginalState state_;
    Json pending_=nullptr;
    Json search_=nullptr;
    std::uint16_t clock_=0;
    std::uint8_t random_=0;
    // Derived by replay, never accepted from serialized input. -1 is unknown.
    int computer_scratch21_=-1;
    int computer_argument_=-1; // Replayed movement argument; cleared at untracked boundaries.
    int handover_argument_=-1; // Verified pre-handover byte, reconstructed by replay.
    // v3 tracks original CPU $06A8 across strategic/tactical boundaries. Legacy
    // saves lack this history and retain their conservative replay behaviour.
    bool track_argument_=true;
    int persistent_argument_=0;
    void remember_argument(int argument){if(track_argument_)persistent_argument_=argument;}
    std::string phase_="player_commands";
    Json ai_=nullptr,battle_=nullptr,ending_=nullptr;
    Json army_=nullptr;
    Json events_=Json::array();
    bool defending_deployment() const;
    bool has_battle_captives() const;
    void enter_turn();
    void rotate_turn();
    std::string command_error() const;
};
}
