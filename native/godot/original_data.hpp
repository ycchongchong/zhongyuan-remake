#pragma once
#include "zhongyuan/original_rom.hpp"
#include "zhongyuan/original_session.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <memory>

namespace godot {
class ZhongyuanOriginalData : public RefCounted {
    GDCLASS(ZhongyuanOriginalData,RefCounted)
    std::unique_ptr<zhongyuan::OriginalRom> rom_;
    std::unique_ptr<zhongyuan::OriginalSession> session_;
protected:
    static void _bind_methods();
public:
    Dictionary load_rom(const String &path);
    Ref<Image> battlefield_image(int city) const;
    Ref<Image> tactical_image() const;
    Ref<Image> unification_image() const;
    Ref<Image> town_image(int faction) const;
    Ref<Image> name_image(bool is_officer,int index) const;
    String start_session(int ruler,int difficulty);
    String start_two_player_session(int first,int second,int difficulty);
    Dictionary advance_turn();
    String end_turn();
    Dictionary session_snapshot() const;
    String music_cue() const;
    Dictionary prepare_development(int city,int officer,int kind);
    String confirm_development();
    void cancel_development();
    String move_officers(int source,int target,const Array &officers);
    String search(int city,int officer);
    Dictionary expedition_quote(int source,int target,const Array &officers,int leader) const;
    Dictionary dispatch_expedition(int source,int target,const Array &officers,int leader);
    String begin_tactical_retreat(int slot);
    String cancel_tactical_retreat();
    String confirm_tactical_retreat();
    String finish_tactical_retreat();
    String advance_withdrawal_result();
    String begin_defender_defeat_result();
    String begin_commander_defeat_result();
    String advance_time_limit_result();
    String begin_human_failure();
    String finish_human_failure();
    String begin_ruler_defeat_result();
    String advance_ruler_annexation();
    String finish_withdrawal_result();
    String begin_tactics();
    String end_tactical_turn();
    String resume_computer_attacker_retreat();
    String advance_computer_attack();
    String advance_computer_tactics();
    String plan_computer_tactics();
    String continue_computer_motion();
    String continue_computer_nearby();
    Dictionary player_tactical_strategies(int slot) const;
    String prepare_tactical_strategy(int slot,int target_slot,int strategy);
    String cancel_tactical_strategy();
    String confirm_tactical_strategy();
    String finish_tactical_strategy();
    String continue_computer_role();
    String resume_computer_role_after_handover();
    String continue_empty_computer_role();
    bool can_continue_computer_role() const;
    String continue_computer_occupied_fort();
    String resume_computer_occupied_fort();
    String finish_exhausted_computer_attack();
    String continue_computer_flank();
    String continue_computer_scan();
    String retry_computer_strategy();
    String resume_computer_unit();
    String evaluate_computer_strategy();
    String execute_computer_strategy();
    String finish_computer_strategy();
    String select_tactical_unit(int slot);
    String move_tactical(int slot,int direction);
    String adjust_tactical_formation(int slot,int direction);
    String scout_tactical(int slot,int target_slot);
    String close_tactical_scout();
    String attack_tactical(int slot,int direction);
    String cancel_tactical_attack();
    String confirm_tactical_attack();
    String begin_clash();
    String advance_clash();
    String resume_clash_strategy();
    String recover_clash_strategy();
    String advance_clash_retreat();
    String advance_clash_defeat();
    String begin_duel();
    String choose_duel_command(int command);
    String answer_duel_surrender(bool accept);
    String advance_duel();
    String cycle_clash_order(int side,int kind);
    String request_clash_surrender();
    String answer_clash_surrender(bool accept);
    String advance_clash_surrender();
    String finish_clash_result();
    Ref<Image> clash_image() const;
    String prepare_deployment();
    String move_deployment(int direction);
    String confirm_deployment();
    Dictionary execute_command(int city,const String &kind,const Dictionary &args);
    String recruit(int city,int hundreds);
    String assign_troops(int officer,int hundreds);
    void finish_recruitment();
    Dictionary visit_city(int city);
    Dictionary finish_search(bool accept);
    void tick(int frames);
    String save_session(const String &path);
    String load_session(const String &path);
};
}
