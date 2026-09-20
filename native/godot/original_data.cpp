#include "original_data.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {
namespace {
template<class Action> String battle_action(zhongyuan::OriginalSession *session,Action action){
    if(!session)return "请先载入游戏";
    return String::utf8(session->perform_battle_action(action).c_str());
}
}
void ZhongyuanOriginalData::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_rom","path"),&ZhongyuanOriginalData::load_rom);
    ClassDB::bind_method(D_METHOD("town_image","faction"),&ZhongyuanOriginalData::town_image);
    ClassDB::bind_method(D_METHOD("name_image","is_officer","index"),&ZhongyuanOriginalData::name_image);
    ClassDB::bind_method(D_METHOD("start_session","ruler","difficulty"),&ZhongyuanOriginalData::start_session);
    ClassDB::bind_method(D_METHOD("start_two_player_session","first","second","difficulty"),&ZhongyuanOriginalData::start_two_player_session);
    ClassDB::bind_method(D_METHOD("advance_turn"),&ZhongyuanOriginalData::advance_turn);
    ClassDB::bind_method(D_METHOD("end_turn"),&ZhongyuanOriginalData::end_turn);
    ClassDB::bind_method(D_METHOD("session_snapshot"),&ZhongyuanOriginalData::session_snapshot);
    ClassDB::bind_method(D_METHOD("prepare_development","city","officer","kind"),&ZhongyuanOriginalData::prepare_development);
    ClassDB::bind_method(D_METHOD("confirm_development"),&ZhongyuanOriginalData::confirm_development);
    ClassDB::bind_method(D_METHOD("cancel_development"),&ZhongyuanOriginalData::cancel_development);
    ClassDB::bind_method(D_METHOD("move_officers","source","target","officers"),&ZhongyuanOriginalData::move_officers);
    ClassDB::bind_method(D_METHOD("search","city","officer"),&ZhongyuanOriginalData::search);
    ClassDB::bind_method(D_METHOD("expedition_quote","source","target","officers","leader"),&ZhongyuanOriginalData::expedition_quote);
    ClassDB::bind_method(D_METHOD("dispatch_expedition","source","target","officers","leader"),&ZhongyuanOriginalData::dispatch_expedition);
    ClassDB::bind_method(D_METHOD("adjust_tactical_formation","slot","direction"),&ZhongyuanOriginalData::adjust_tactical_formation);
    ClassDB::bind_method(D_METHOD("scout_tactical","slot","target_slot"),&ZhongyuanOriginalData::scout_tactical);
    ClassDB::bind_method(D_METHOD("close_tactical_scout"),&ZhongyuanOriginalData::close_tactical_scout);
    ClassDB::bind_method(D_METHOD("attack_tactical","slot","direction"),&ZhongyuanOriginalData::attack_tactical);
    ClassDB::bind_method(D_METHOD("cancel_tactical_attack"),&ZhongyuanOriginalData::cancel_tactical_attack);
    ClassDB::bind_method(D_METHOD("confirm_tactical_attack"),&ZhongyuanOriginalData::confirm_tactical_attack);
    ClassDB::bind_method(D_METHOD("begin_clash"),&ZhongyuanOriginalData::begin_clash);
    ClassDB::bind_method(D_METHOD("advance_clash"),&ZhongyuanOriginalData::advance_clash);
    ClassDB::bind_method(D_METHOD("resume_clash_strategy"),&ZhongyuanOriginalData::resume_clash_strategy);
    ClassDB::bind_method(D_METHOD("recover_clash_strategy"),&ZhongyuanOriginalData::recover_clash_strategy);
    ClassDB::bind_method(D_METHOD("advance_clash_retreat"),&ZhongyuanOriginalData::advance_clash_retreat);
    ClassDB::bind_method(D_METHOD("advance_clash_defeat"),&ZhongyuanOriginalData::advance_clash_defeat);
    ClassDB::bind_method(D_METHOD("begin_duel"),&ZhongyuanOriginalData::begin_duel);
    ClassDB::bind_method(D_METHOD("choose_duel_command","command"),&ZhongyuanOriginalData::choose_duel_command);
    ClassDB::bind_method(D_METHOD("answer_duel_surrender","accept"),&ZhongyuanOriginalData::answer_duel_surrender);
    ClassDB::bind_method(D_METHOD("advance_duel"),&ZhongyuanOriginalData::advance_duel);
    ClassDB::bind_method(D_METHOD("cycle_clash_order","side","kind"),&ZhongyuanOriginalData::cycle_clash_order);
    ClassDB::bind_method(D_METHOD("request_clash_surrender"),&ZhongyuanOriginalData::request_clash_surrender);
    ClassDB::bind_method(D_METHOD("answer_clash_surrender","accept"),&ZhongyuanOriginalData::answer_clash_surrender);
    ClassDB::bind_method(D_METHOD("advance_clash_surrender"),&ZhongyuanOriginalData::advance_clash_surrender);
    ClassDB::bind_method(D_METHOD("finish_clash_result"),&ZhongyuanOriginalData::finish_clash_result);
    ClassDB::bind_method(D_METHOD("clash_image"),&ZhongyuanOriginalData::clash_image);
    ClassDB::bind_method(D_METHOD("begin_tactical_retreat","slot"),&ZhongyuanOriginalData::begin_tactical_retreat);
    ClassDB::bind_method(D_METHOD("cancel_tactical_retreat"),&ZhongyuanOriginalData::cancel_tactical_retreat);
    ClassDB::bind_method(D_METHOD("confirm_tactical_retreat"),&ZhongyuanOriginalData::confirm_tactical_retreat);
    ClassDB::bind_method(D_METHOD("finish_tactical_retreat"),&ZhongyuanOriginalData::finish_tactical_retreat);
    ClassDB::bind_method(D_METHOD("advance_withdrawal_result"),&ZhongyuanOriginalData::advance_withdrawal_result);
    ClassDB::bind_method(D_METHOD("begin_defender_defeat_result"),&ZhongyuanOriginalData::begin_defender_defeat_result);
    ClassDB::bind_method(D_METHOD("begin_commander_defeat_result"),&ZhongyuanOriginalData::begin_commander_defeat_result);
    ClassDB::bind_method(D_METHOD("advance_time_limit_result"),&ZhongyuanOriginalData::advance_time_limit_result);
    ClassDB::bind_method(D_METHOD("begin_human_failure"),&ZhongyuanOriginalData::begin_human_failure);
    ClassDB::bind_method(D_METHOD("finish_human_failure"),&ZhongyuanOriginalData::finish_human_failure);
    ClassDB::bind_method(D_METHOD("begin_ruler_defeat_result"),&ZhongyuanOriginalData::begin_ruler_defeat_result);
    ClassDB::bind_method(D_METHOD("advance_ruler_annexation"),&ZhongyuanOriginalData::advance_ruler_annexation);
    ClassDB::bind_method(D_METHOD("finish_withdrawal_result"),&ZhongyuanOriginalData::finish_withdrawal_result);
    ClassDB::bind_method(D_METHOD("begin_tactics"),&ZhongyuanOriginalData::begin_tactics);
    ClassDB::bind_method(D_METHOD("plan_computer_tactics"),&ZhongyuanOriginalData::plan_computer_tactics);
    ClassDB::bind_method(D_METHOD("continue_computer_flank"),&ZhongyuanOriginalData::continue_computer_flank);
    ClassDB::bind_method(D_METHOD("continue_computer_scan"),&ZhongyuanOriginalData::continue_computer_scan);
    ClassDB::bind_method(D_METHOD("retry_computer_strategy"),&ZhongyuanOriginalData::retry_computer_strategy);
    ClassDB::bind_method(D_METHOD("resume_computer_unit"),&ZhongyuanOriginalData::resume_computer_unit);
    ClassDB::bind_method(D_METHOD("continue_computer_motion"),&ZhongyuanOriginalData::continue_computer_motion);
    ClassDB::bind_method(D_METHOD("continue_computer_nearby"),&ZhongyuanOriginalData::continue_computer_nearby);
    ClassDB::bind_method(D_METHOD("player_tactical_strategies","slot"),&ZhongyuanOriginalData::player_tactical_strategies);
    ClassDB::bind_method(D_METHOD("prepare_tactical_strategy","slot","target_slot","strategy"),&ZhongyuanOriginalData::prepare_tactical_strategy);
    ClassDB::bind_method(D_METHOD("cancel_tactical_strategy"),&ZhongyuanOriginalData::cancel_tactical_strategy);
    ClassDB::bind_method(D_METHOD("confirm_tactical_strategy"),&ZhongyuanOriginalData::confirm_tactical_strategy);
    ClassDB::bind_method(D_METHOD("finish_tactical_strategy"),&ZhongyuanOriginalData::finish_tactical_strategy);
    ClassDB::bind_method(D_METHOD("continue_computer_role"),&ZhongyuanOriginalData::continue_computer_role);
    ClassDB::bind_method(D_METHOD("resume_computer_role_after_handover"),&ZhongyuanOriginalData::resume_computer_role_after_handover);
    ClassDB::bind_method(D_METHOD("can_continue_computer_role"),&ZhongyuanOriginalData::can_continue_computer_role);
    ClassDB::bind_method(D_METHOD("continue_empty_computer_role"),&ZhongyuanOriginalData::continue_empty_computer_role);
    ClassDB::bind_method(D_METHOD("continue_computer_occupied_fort"),&ZhongyuanOriginalData::continue_computer_occupied_fort);
    ClassDB::bind_method(D_METHOD("resume_computer_occupied_fort"),&ZhongyuanOriginalData::resume_computer_occupied_fort);
    ClassDB::bind_method(D_METHOD("finish_exhausted_computer_attack"),&ZhongyuanOriginalData::finish_exhausted_computer_attack);
    ClassDB::bind_method(D_METHOD("evaluate_computer_strategy"),&ZhongyuanOriginalData::evaluate_computer_strategy);
    ClassDB::bind_method(D_METHOD("execute_computer_strategy"),&ZhongyuanOriginalData::execute_computer_strategy);
    ClassDB::bind_method(D_METHOD("finish_computer_strategy"),&ZhongyuanOriginalData::finish_computer_strategy);
    ClassDB::bind_method(D_METHOD("resume_computer_attacker_retreat"),&ZhongyuanOriginalData::resume_computer_attacker_retreat);
    ClassDB::bind_method(D_METHOD("advance_computer_attack"),&ZhongyuanOriginalData::advance_computer_attack);
    ClassDB::bind_method(D_METHOD("advance_computer_tactics"),&ZhongyuanOriginalData::advance_computer_tactics);
    ClassDB::bind_method(D_METHOD("end_tactical_turn"),&ZhongyuanOriginalData::end_tactical_turn);
    ClassDB::bind_method(D_METHOD("select_tactical_unit","slot"),&ZhongyuanOriginalData::select_tactical_unit);
    ClassDB::bind_method(D_METHOD("move_tactical","slot","direction"),&ZhongyuanOriginalData::move_tactical);
    ClassDB::bind_method(D_METHOD("prepare_deployment"),&ZhongyuanOriginalData::prepare_deployment);
    ClassDB::bind_method(D_METHOD("move_deployment","direction"),&ZhongyuanOriginalData::move_deployment);
    ClassDB::bind_method(D_METHOD("confirm_deployment"),&ZhongyuanOriginalData::confirm_deployment);
    ClassDB::bind_method(D_METHOD("execute_command","city","kind","args"),&ZhongyuanOriginalData::execute_command);
    ClassDB::bind_method(D_METHOD("battlefield_image","city"),&ZhongyuanOriginalData::battlefield_image);
    ClassDB::bind_method(D_METHOD("tactical_image"),&ZhongyuanOriginalData::tactical_image);
    ClassDB::bind_method(D_METHOD("music_cue"),&ZhongyuanOriginalData::music_cue);
    ClassDB::bind_method(D_METHOD("recruit","city","hundreds"),&ZhongyuanOriginalData::recruit);
    ClassDB::bind_method(D_METHOD("assign_troops","officer","hundreds"),&ZhongyuanOriginalData::assign_troops);
    ClassDB::bind_method(D_METHOD("finish_recruitment"),&ZhongyuanOriginalData::finish_recruitment);
    ClassDB::bind_method(D_METHOD("visit_city","city"),&ZhongyuanOriginalData::visit_city);
    ClassDB::bind_method(D_METHOD("finish_search","accept"),&ZhongyuanOriginalData::finish_search);
    ClassDB::bind_method(D_METHOD("tick","frames"),&ZhongyuanOriginalData::tick);
    ClassDB::bind_method(D_METHOD("save_session","path"),&ZhongyuanOriginalData::save_session);
    ClassDB::bind_method(D_METHOD("load_session","path"),&ZhongyuanOriginalData::load_session);
}
Dictionary ZhongyuanOriginalData::load_rom(const String &path) {
    Dictionary result;
    try {
        Ref<FileAccess> file=FileAccess::open(path,FileAccess::READ);
        if(file.is_null()) throw std::runtime_error("Cannot open reference ROM");
        if(file->get_length()!=zhongyuan::OriginalRom::FILE_SIZE) throw std::runtime_error("Wrong reference ROM size");
        const auto bytes=file->get_buffer(file->get_length());
        auto candidate=std::make_unique<zhongyuan::OriginalRom>(std::vector<std::uint8_t>(bytes.ptr(),bytes.ptr()+bytes.size()));
        result=JSON::parse_string(String::utf8(candidate->inventory().dump().c_str()));
        rom_=std::move(candidate);
        session_.reset();
    }catch(const std::exception &e) {result["error"]=String::utf8(e.what());}
    return result;
}
String ZhongyuanOriginalData::start_session(int ruler,int difficulty) {
    if(!rom_)return "请先载入参考游戏";
    try {auto next=std::make_unique<zhongyuan::OriginalSession>(*rom_);next->start(ruler,difficulty);session_=std::move(next);return {};}
    catch(const std::exception &e){return String::utf8(e.what());}
}
Dictionary ZhongyuanOriginalData::session_snapshot() const {
    if(!session_){Dictionary error;error["error"]="战局尚未初始化";return error;}
    return JSON::parse_string(String::utf8(session_->snapshot().dump().c_str()));
}
String ZhongyuanOriginalData::music_cue() const {
    return session_ ? String::utf8(session_->music_cue().c_str()) : String();
}
String ZhongyuanOriginalData::start_two_player_session(int first,int second,int difficulty){
    if(!rom_)return "请先载入参考游戏";
    try{auto next=std::make_unique<zhongyuan::OriginalSession>(*rom_);next->start(first,difficulty,second);session_=std::move(next);return {};}
    catch(const std::exception &e){return String::utf8(e.what());}
}
Dictionary ZhongyuanOriginalData::advance_turn(){
    if(!session_){Dictionary error;error["error"]="战局尚未初始化";return error;}
    return JSON::parse_string(String::utf8(session_->advance().dump().c_str()));
}
String ZhongyuanOriginalData::end_turn(){return session_?String::utf8(session_->end_turn().c_str()):String("战局尚未初始化");}
Dictionary ZhongyuanOriginalData::prepare_development(int city,int officer,int kind) {
    if(!session_){Dictionary error;error["error"]="战局尚未初始化";return error;}
    return JSON::parse_string(String::utf8(session_->prepare_development(city,officer,kind).dump().c_str()));
}
String ZhongyuanOriginalData::confirm_development(){return session_?String::utf8(session_->confirm_development().c_str()):String("战局尚未初始化");}
void ZhongyuanOriginalData::cancel_development(){if(session_)session_->cancel_development();}
String ZhongyuanOriginalData::move_officers(int source,int target,const Array &officers){
    if(!session_)return "战局尚未初始化";
    std::vector<int> ids;for(int i=0;i<officers.size();++i){if(officers[i].get_type()!=Variant::INT)return "无效武将编号";ids.push_back(int(officers[i]));}
    return String::utf8(session_->move(source,target,ids).c_str());
}
void ZhongyuanOriginalData::tick(int frames){if(session_ && frames>=0 && frames<=3600)session_->tick(frames);}
String ZhongyuanOriginalData::search(int city,int officer){return session_?String::utf8(session_->search(city,officer).c_str()):String("战局尚未初始化");}
Dictionary ZhongyuanOriginalData::expedition_quote(int source,int target,const Array &officers,int leader)const{
    if(!session_){Dictionary d;d["error"]="战局尚未初始化";return d;}
    std::vector<int> ids;for(int i=0;i<officers.size();++i)ids.push_back(int(officers[i]));
    return JSON::parse_string(String::utf8(session_->expedition_quote(source,target,ids,leader).dump().c_str()));
}
Dictionary ZhongyuanOriginalData::dispatch_expedition(int source,int target,const Array &officers,int leader){
    if(!session_){Dictionary d;d["error"]="战局尚未初始化";return d;}
    std::vector<int> ids;for(int i=0;i<officers.size();++i)ids.push_back(int(officers[i]));
    return JSON::parse_string(String::utf8(session_->dispatch_expedition(source,target,ids,leader).dump().c_str()));
}
Dictionary ZhongyuanOriginalData::execute_command(int city,const String &kind,const Dictionary &args){
    if(!session_){Dictionary error;error["error"]="战局尚未初始化";return error;}
    const auto data=zhongyuan::Json::parse(JSON::stringify(args).utf8().get_data());
    return JSON::parse_string(String::utf8(session_->execute_command(city,kind.utf8().get_data(),data).dump().c_str()));
}
String ZhongyuanOriginalData::adjust_tactical_formation(int slot,int direction){return battle_action(session_.get(),[&](auto &game){return game.adjust_tactical_formation(slot,direction);});}
String ZhongyuanOriginalData::scout_tactical(int slot,int target_slot){return battle_action(session_.get(),[&](auto &game){return game.scout_tactical(slot,target_slot);});}
String ZhongyuanOriginalData::close_tactical_scout(){return battle_action(session_.get(),[&](auto &game){return game.close_tactical_scout();});}
String ZhongyuanOriginalData::attack_tactical(int slot,int direction){return battle_action(session_.get(),[&](auto &game){return game.attack_tactical(slot,direction);});}
String ZhongyuanOriginalData::cancel_tactical_attack(){return battle_action(session_.get(),[&](auto &game){return game.cancel_tactical_attack();});}
String ZhongyuanOriginalData::confirm_tactical_attack(){return battle_action(session_.get(),[&](auto &game){return game.confirm_tactical_attack();});}
String ZhongyuanOriginalData::advance_clash_retreat(){return battle_action(session_.get(),[&](auto &game){return game.advance_clash_retreat();});}
String ZhongyuanOriginalData::advance_clash_defeat(){return battle_action(session_.get(),[&](auto &game){return game.advance_clash_defeat();});}
String ZhongyuanOriginalData::begin_duel(){return battle_action(session_.get(),[&](auto &game){return game.begin_duel();});}
String ZhongyuanOriginalData::choose_duel_command(int command){return battle_action(session_.get(),[&](auto &game){return game.choose_duel_command(command);});}
String ZhongyuanOriginalData::answer_duel_surrender(bool accept){return battle_action(session_.get(),[&](auto &game){return game.answer_duel_surrender(accept);});}
String ZhongyuanOriginalData::advance_duel(){return battle_action(session_.get(),[&](auto &game){return game.advance_duel();});}
String ZhongyuanOriginalData::advance_clash(){return battle_action(session_.get(),[&](auto &game){return game.advance_clash();});}
String ZhongyuanOriginalData::resume_clash_strategy(){return battle_action(session_.get(),[&](auto &game){return game.resume_clash_strategy();});}
String ZhongyuanOriginalData::recover_clash_strategy(){return battle_action(session_.get(),[&](auto &game){return game.recover_clash_strategy();});}
String ZhongyuanOriginalData::begin_clash(){return battle_action(session_.get(),[&](auto &game){return game.begin_clash();});}
String ZhongyuanOriginalData::cycle_clash_order(int side,int kind){return battle_action(session_.get(),[&](auto &game){return game.cycle_clash_order(side,kind);});}
String ZhongyuanOriginalData::request_clash_surrender(){return battle_action(session_.get(),[&](auto &game){return game.request_clash_surrender();});}
String ZhongyuanOriginalData::answer_clash_surrender(bool accept){return battle_action(session_.get(),[&](auto &game){return game.answer_clash_surrender(accept);});}
String ZhongyuanOriginalData::advance_clash_surrender(){return battle_action(session_.get(),[&](auto &game){return game.advance_clash_surrender();});}
String ZhongyuanOriginalData::finish_clash_result(){return battle_action(session_.get(),[&](auto &game){return game.finish_clash_result();});}
Ref<Image> ZhongyuanOriginalData::clash_image() const {
    if(!session_)return {};
    const auto pixels=session_->clash_rgb();if(pixels.empty())return {};
    PackedByteArray rgb;rgb.resize(pixels.size());std::copy(pixels.begin(),pixels.end(),rgb.ptrw());
    return Image::create_from_data(256,160,false,Image::FORMAT_RGB8,rgb);
}
String ZhongyuanOriginalData::begin_tactical_retreat(int slot){return battle_action(session_.get(),[&](auto &game){return game.begin_tactical_retreat(slot);});}
String ZhongyuanOriginalData::cancel_tactical_retreat(){return battle_action(session_.get(),[&](auto &game){return game.cancel_tactical_retreat();});}
String ZhongyuanOriginalData::confirm_tactical_retreat(){return battle_action(session_.get(),[&](auto &game){return game.confirm_tactical_retreat();});}
String ZhongyuanOriginalData::finish_tactical_retreat(){return battle_action(session_.get(),[&](auto &game){return game.finish_tactical_retreat();});}
String ZhongyuanOriginalData::advance_withdrawal_result(){return battle_action(session_.get(),[&](auto &game){return game.advance_withdrawal_result();});}
String ZhongyuanOriginalData::begin_commander_defeat_result(){return battle_action(session_.get(),[&](auto &game){return game.begin_commander_defeat_result();});}
String ZhongyuanOriginalData::begin_human_failure(){return battle_action(session_.get(),[&](auto &game){return game.begin_human_failure();});}
String ZhongyuanOriginalData::finish_human_failure(){return battle_action(session_.get(),[&](auto &game){return game.finish_human_failure();});}
String ZhongyuanOriginalData::begin_ruler_defeat_result(){return battle_action(session_.get(),[&](auto &game){return game.begin_ruler_defeat_result();});}
String ZhongyuanOriginalData::advance_ruler_annexation(){return battle_action(session_.get(),[&](auto &game){return game.advance_ruler_annexation();});}
String ZhongyuanOriginalData::advance_time_limit_result(){return battle_action(session_.get(),[&](auto &game){return game.advance_time_limit_result();});}
String ZhongyuanOriginalData::begin_defender_defeat_result(){return battle_action(session_.get(),[&](auto &game){return game.begin_defender_defeat_result();});}
String ZhongyuanOriginalData::finish_withdrawal_result(){return battle_action(session_.get(),[&](auto &game){return game.finish_withdrawal_result();});}
String ZhongyuanOriginalData::plan_computer_tactics(){return battle_action(session_.get(),[&](auto &game){return game.plan_computer_tactics();});}
String ZhongyuanOriginalData::continue_computer_scan(){return battle_action(session_.get(),[&](auto &game){return game.continue_computer_scan();});}
String ZhongyuanOriginalData::continue_computer_flank(){return battle_action(session_.get(),[&](auto &game){return game.continue_computer_flank();});}
String ZhongyuanOriginalData::retry_computer_strategy(){return battle_action(session_.get(),[&](auto &game){return game.retry_computer_strategy();});}
String ZhongyuanOriginalData::resume_computer_unit(){return battle_action(session_.get(),[&](auto &game){return game.plan_computer_tactics(true);});}
String ZhongyuanOriginalData::finish_exhausted_computer_attack(){return battle_action(session_.get(),[&](auto &game){return game.finish_exhausted_computer_attack();});}
String ZhongyuanOriginalData::resume_computer_occupied_fort(){return battle_action(session_.get(),[&](auto &game){return game.continue_computer_occupied_fort(true);});}
Dictionary ZhongyuanOriginalData::player_tactical_strategies(int slot) const {if(!session_)return {};return JSON::parse_string(String::utf8(session_->player_tactical_strategies(slot).dump().c_str()));}
String ZhongyuanOriginalData::prepare_tactical_strategy(int slot,int target_slot,int strategy){return battle_action(session_.get(),[&](auto &game){return game.prepare_tactical_strategy(slot,target_slot,strategy);});}
String ZhongyuanOriginalData::cancel_tactical_strategy(){return battle_action(session_.get(),[&](auto &game){return game.cancel_tactical_strategy();});}
String ZhongyuanOriginalData::confirm_tactical_strategy(){return battle_action(session_.get(),[&](auto &game){return game.confirm_tactical_strategy();});}
String ZhongyuanOriginalData::finish_tactical_strategy(){return battle_action(session_.get(),[&](auto &game){return game.finish_tactical_strategy();});}
String ZhongyuanOriginalData::resume_computer_role_after_handover(){return battle_action(session_.get(),[&](auto &game){return game.resume_computer_role_after_handover();});}
String ZhongyuanOriginalData::continue_computer_role(){return battle_action(session_.get(),[&](auto &game){return game.continue_computer_role();});}
bool ZhongyuanOriginalData::can_continue_computer_role() const {return session_&&session_->can_continue_computer_role();}
String ZhongyuanOriginalData::continue_empty_computer_role(){return battle_action(session_.get(),[&](auto &game){return game.continue_empty_computer_role();});}
String ZhongyuanOriginalData::continue_computer_occupied_fort(){return battle_action(session_.get(),[&](auto &game){return game.continue_computer_occupied_fort();});}
String ZhongyuanOriginalData::continue_computer_nearby(){return battle_action(session_.get(),[&](auto &game){return game.continue_computer_nearby();});}
String ZhongyuanOriginalData::continue_computer_motion(){return battle_action(session_.get(),[&](auto &game){return game.continue_computer_motion();});}
String ZhongyuanOriginalData::evaluate_computer_strategy(){return battle_action(session_.get(),[&](auto &game){return game.evaluate_computer_strategy();});}
String ZhongyuanOriginalData::execute_computer_strategy(){return battle_action(session_.get(),[&](auto &game){return game.execute_computer_strategy();});}
String ZhongyuanOriginalData::finish_computer_strategy(){return battle_action(session_.get(),[&](auto &game){return game.finish_computer_strategy();});}
String ZhongyuanOriginalData::resume_computer_attacker_retreat(){return battle_action(session_.get(),[&](auto &game){return game.resume_computer_attacker_retreat();});}
String ZhongyuanOriginalData::advance_computer_attack(){return battle_action(session_.get(),[&](auto &game){return game.advance_computer_attack();});}
String ZhongyuanOriginalData::advance_computer_tactics(){return battle_action(session_.get(),[&](auto &game){return game.advance_computer_tactics();});}
String ZhongyuanOriginalData::end_tactical_turn(){return battle_action(session_.get(),[&](auto &game){return game.end_tactical_turn();});}
String ZhongyuanOriginalData::begin_tactics(){return battle_action(session_.get(),[&](auto &game){return game.begin_tactics();});}
String ZhongyuanOriginalData::select_tactical_unit(int slot){return battle_action(session_.get(),[&](auto &game){return game.select_tactical_unit(slot);});}
String ZhongyuanOriginalData::move_tactical(int slot,int direction){return battle_action(session_.get(),[&](auto &game){return game.move_tactical(slot,direction);});}
String ZhongyuanOriginalData::prepare_deployment(){return session_?String::utf8(session_->prepare_deployment().c_str()):String("战局尚未初始化");}
String ZhongyuanOriginalData::move_deployment(int direction){return session_?String::utf8(session_->move_deployment(direction).c_str()):String("战局尚未初始化");}
String ZhongyuanOriginalData::confirm_deployment(){return session_?String::utf8(session_->confirm_deployment().c_str()):String("战局尚未初始化");}
String ZhongyuanOriginalData::recruit(int city,int hundreds){return session_?String::utf8(session_->recruit(city,hundreds).c_str()):String("战局尚未初始化");}
String ZhongyuanOriginalData::assign_troops(int officer,int hundreds){return session_?String::utf8(session_->assign_troops(officer,hundreds).c_str()):String("战局尚未初始化");}
void ZhongyuanOriginalData::finish_recruitment(){if(session_)session_->finish_recruitment();}
Dictionary ZhongyuanOriginalData::visit_city(int city){
    Dictionary result;
    if(!session_){result["error"]="战局尚未初始化";return result;}
    try {const auto report=session_->visit_city(city);if(!report.is_null())result=JSON::parse_string(String::utf8(report.dump().c_str()));}
    catch(const std::exception &e){result["error"]=String::utf8(e.what());}
    return result;
}
Dictionary ZhongyuanOriginalData::finish_search(bool accept){
    Dictionary result;
    if(!session_){result["error"]="战局尚未初始化";return result;}
    try {result=JSON::parse_string(String::utf8(session_->finish_search(accept).dump().c_str()));}
    catch(const std::exception &e){result["error"]=String::utf8(e.what());}
    return result;
}
String ZhongyuanOriginalData::save_session(const String &path){
    if(!session_)return "战局尚未初始化";
    const auto data=session_->save().dump();
    if(data.size()>zhongyuan::OriginalSession::MAX_SAVE_BYTES)return "存档文件过大";
    const String absolute=ProjectSettings::get_singleton()->globalize_path(path),temporary=absolute+String(".tmp");
    Ref<FileAccess> file=FileAccess::open(temporary,FileAccess::WRITE);if(file.is_null())return "无法写入存档";
    file->store_string(String::utf8(data.c_str()));file->flush();const Error error=file->get_error();file->close();
    if(error!=OK){DirAccess::remove_absolute(temporary);return "写入存档失败";}
    if(DirAccess::rename_absolute(temporary,absolute)!=OK){DirAccess::remove_absolute(temporary);return "替换存档失败";}
    return {};
}
String ZhongyuanOriginalData::load_session(const String &path){
    if(!rom_)return "请先载入参考游戏";
    if(!FileAccess::file_exists(path))return "没有原版战局存档";
    Ref<FileAccess> file=FileAccess::open(path,FileAccess::READ);if(file.is_null())return "无法读取存档";
    if(file->get_length()>zhongyuan::OriginalSession::MAX_SAVE_BYTES)return "存档文件过大";
    try {auto next=std::make_unique<zhongyuan::OriginalSession>(*rom_);const auto text=file->get_as_text().utf8();auto error=next->restore(zhongyuan::Json::parse(std::string(text.get_data(),text.length())));if(!error.empty())return String::utf8(error.c_str());session_=std::move(next);return {};}
    catch(const std::exception &){return "存档格式损坏";}
}
Ref<Image> ZhongyuanOriginalData::battlefield_image(int city) const {
    if(!rom_||city<0||city>=30)return Ref<Image>();
    const auto pixels=rom_->battlefield_pixels(city);PackedByteArray rgba;rgba.resize(256*160*4);
    for(int i=0;i<256*160;++i){
        std::array<int,3> color{};
        switch(pixels[i]){
            case 0x29:color={130,211,16};break;case 0x19:color={0,150,0};break;
            case 0x21:color={60,190,255};break;case 0x27:color={255,154,56};break;
            case 0x07:color={125,8,0};break;case 0x20:color={255,255,255};break;
            case 0x17:color={203,77,12};break;
        }
        for(int c=0;c<3;++c)rgba.set(i*4+c,color[c]);rgba.set(i*4+3,255);
    }
    return Image::create_from_data(256,160,false,Image::FORMAT_RGBA8,rgba);
}
Ref<Image> ZhongyuanOriginalData::tactical_image() const {
    if(!session_)return {};
    const auto pixels=session_->tactical_pixels();if(pixels.empty())return {};
    PackedByteArray rgb;rgb.resize(256*160*3);
    for(int i=0;i<256*160;++i){
        std::array<int,3> color{};
        switch(pixels[i]){
            case 7:color={125,8,0};break;case 17:color={0,113,239};break;
            case 22:color={219,40,0};break;case 23:color={203,77,12};break;
            case 25:color={0,150,0};break;case 32:color={255,255,255};break;
            case 33:color={60,190,255};break;case 36:color={247,121,255};break;
            case 38:color={255,117,97};break;case 39:color={255,154,56};break;
            case 41:color={130,211,16};break;case 56:color={255,231,162};break;
        }
        for(int c=0;c<3;++c)rgb.set(i*3+c,color[c]);
    }
    return Image::create_from_data(256,160,false,Image::FORMAT_RGB8,rgb);
}
Ref<Image> ZhongyuanOriginalData::town_image(int faction) const {
    if(!rom_ || faction<0 || faction>=6) return Ref<Image>();
    const auto sprite=rom_->world_map().at("sprites").at(faction);
    // Reference renderer RGB565-expanded colours, verified against the road screen.
    static const unsigned char palette[2][4][3]={{{0,0,0},{0,0,0},{215,203,255},{219,40,0}},
        {{0,0,0},{255,255,255},{0,113,239},{255,117,97}}};
    PackedByteArray rgba;rgba.resize(8*8*4);
    for(int i=0;i<64;++i) {
        const int pixel=sprite.at("pixels").at(i), pal=sprite.at("palette");
        for(int c=0;c<3;++c) rgba.set(i*4+c,palette[pal][pixel][c]);
        rgba.set(i*4+3,pixel==0 ? 0 : 255);
    }
    return Image::create_from_data(8,8,false,Image::FORMAT_RGBA8,rgba);
}
Ref<Image> ZhongyuanOriginalData::name_image(bool is_officer,int index) const {
    if(!rom_ || index<0 || index>=int(is_officer ? rom_->OFFICER_COUNT : rom_->CITY_COUNT)) return Ref<Image>();
    const auto source=rom_->name_pixels(is_officer,std::size_t(index));
    PackedByteArray rgba;rgba.resize(48*16*4);
    for(int i=0;i<48*16;++i) {
        rgba.set(i*4,0);rgba.set(i*4+1,0);rgba.set(i*4+2,0);
        rgba.set(i*4+3,source[i]==0 ? 255 : 0);
    }
    return Image::create_from_data(48,16,false,Image::FORMAT_RGBA8,rgba);
}
}
