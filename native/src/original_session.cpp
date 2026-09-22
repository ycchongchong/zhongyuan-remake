#include "zhongyuan/original_session.hpp"
#include <set>
namespace zhongyuan {
OriginalSession::OriginalSession(const OriginalRom &rom):rom_(rom),state_(rom,rom.initial_sram(0)){start(4,0);}
std::vector<std::uint8_t> OriginalSession::tactical_pixels() const {
    if(!battle_.is_object()||!battle_.contains("deployment"))return {};
    return rom_.tactical_pixels(battle_["target"],state_.sram());
}
bool OriginalSession::battle_history_needs_archive() const {
    return battle_.is_object()&&battle_.contains("tactics")&&battle_["tactics"]["moves"].size()>=HISTORY_SEGMENT_TARGET;
}
std::string OriginalSession::archive_battle_history(){
    if((phase_!="battle"&&phase_!="expedition")||!battle_history_needs_archive())return "当前战斗记录无需分段";
    const auto &old=battle_["tactics"];
    if(old["moves"].size()>1024)return "无效战斗记录分段";
    if(old.contains("history")&&old["history"].size()>=MAX_HISTORY_SEGMENTS)return "战斗记录已达到存档容量上限";
    auto next=*this;auto &t=next.battle_["tactics"];
    if(!t.contains("history"))t["history"]=Json::array();
    t["history"].push_back(std::move(t["moves"]));t["moves"]=Json::array();
    // All game state and derived AI scratch survive the segment boundary.
    *this=std::move(next);return {};
}
void OriginalSession::start(int ruler,int difficulty,int second){
    OriginalState next(rom_,rom_.initial_sram(difficulty));
    next.set_players(ruler,second);next.begin_ruler_turn(0);
    state_=std::move(next);pending_=nullptr;search_=nullptr;clock_=0;random_=0;computer_scratch21_=-1;computer_argument_=-1;handover_argument_=-1;
    track_argument_=true;persistent_argument_=0;
    ai_=nullptr;battle_=nullptr;ending_=nullptr;army_=nullptr;events_=Json::array();enter_turn();
}
void OriginalSession::enter_turn(){
    ending_=state_.ending();
    if(!ending_.is_null()){phase_="ending";ai_=nullptr;battle_=nullptr;return;}
    if(state_.human_turn()){phase_="player_commands";ai_=nullptr;}
    else{phase_="ai_turn";ai_=state_.begin_ai();ai_["random_cursor"]=random_;}
}
void OriginalSession::rotate_turn(){
    if(!state_.ending().is_null()){enter_turn();return;}
    const bool month=state_.next_ruler(random_);
    if(month)events_.push_back({{"kind","month"},{"year",state_.sram()[0xd85]+256*state_.sram()[0xd86]},{"month",state_.sram()[0xd87]}});
    if(state_.ending().is_null())state_.begin_ruler_turn(state_.sram()[0xd8b]);
    enter_turn();
}
std::string OriginalSession::command_error()const{
    if(!army_.is_null())return "请先完成征兵编成";
    return phase_=="player_commands"?std::string{}:"当前不是玩家指令阶段";
}
std::string OriginalSession::end_turn(){
    if(auto error=command_error();!error.empty())return error;
    if(!pending_.is_null()||!search_.is_null())return "请先处理当前方案或搜索报告";
    // Commit the complete transition only after all state operations succeed.
    auto next=*this;
    try{next.events_=Json::array();next.state_.end_orders();next.rotate_turn();*this=std::move(next);return {};}
    catch(const std::exception &e){return e.what();}
}
Json OriginalSession::advance(){
    if(phase_!="ai_turn")return {{"phase",phase_}};
    auto next=*this;
    try{
        const int ruler=next.state_.sram()[0xd8b];
        next.ai_["random_cursor"]=next.random_;
        auto result=next.state_.ai_phase(next.ai_);
        next.random_=result["random_cursor"];next.ai_=result;
        // C6A8-C6C3 stores the winner of an automatic strategic battle.
        if(result["battle_result"]==1||result["battle_result"]==2)next.remember_argument(result["winner"]);
        if(result["done"].get<bool>()){
            if(result["battle_result"]==3){next.battle_=result;next.phase_="battle";}
            else{
                next.state_.finish_ai(result);
                next.events_.push_back({{"kind","ai"},{"ruler",ruler},{"battle_result",result["battle_result"]},{"source",result["source"]},{"target",result["target"]}});
                next.rotate_turn();
            }
        }
        if(next.events_.size()>64)next.events_.erase(next.events_.begin(),next.events_.end()-64);
        *this=std::move(next);return {{"phase",phase_}};
    }catch(const std::exception &e){return {{"error",e.what()},{"phase",phase_}};}
}
bool OriginalSession::defending_deployment() const {
    return battle_.at("deployment").value("side",std::string(phase_=="battle"?"defender":"attacker"))=="defender";
}
Json OriginalSession::snapshot() const {
    auto result=state_.snapshot();result["pending_development"]=pending_;
    result["pending_search"]=search_;
    result["search_parties"]=Json::array();
    const auto &bytes=state_.sram();
    for(int i=0;i<93;i+=3)if(bytes[0xc30+i]<241 && bytes[0xc31+i]!=255)
        result["search_parties"].push_back({{"officer",bytes[0xc30+i]},{"city",bytes[0xc31+i]&127},{"ruler",bytes[0xc32+i]},{"ready",!(bytes[0xc31+i]&128)}});
    result["frame_counter"]=clock_;result["random_cursor"]=random_;
    result["opening_ai_pending"]=false;
    result["phase"]=phase_;result["ai"]=ai_;result["battle"]=battle_;result["ending"]=ending_;result["events"]=events_;result["pending_army"]=army_;
    if(phase_=="ending"&&ending_.value("kind",std::string{})=="unification")
        result["unification"]=state_.unification_summary();
    if(battle_.is_object()&&battle_.contains("deployment"))result["battle"]["deployment_area"]=state_.deployment_area(battle_.at("target"),defending_deployment());
    if(battle_.is_object()&&battle_.contains("tactics")){
        result["battle"]["army_defeat_result_available"]=can_settle_defender_defeat();
        result["battle"]["commander_defeat_result_available"]=can_settle_commander_defeat();
        result["battle"]["human_failure_available"]=can_report_human_failure();
        result["battle"]["ruler_defeat_result_available"]=can_settle_ruler_defeat();
        auto area=Json::array();for(int i=0;i<160;++i)area.push_back(false);
        const int slot=battle_["tactics"]["selected"];
        for(int direction=0;direction<4;++direction){auto candidate=state_;int points=battle_["tactics"]["points"];
            if(tactical_action_error().empty()&&!battle_["tactics"].contains("attack")&&!battle_["tactics"].contains("scout")&&!battle_["tactics"].contains("retreat")&&candidate.tactical_step(slot,direction,points,tactical_side()).empty())area[candidate.sram()[tactical_side()?0xdc3+slot*3:0xdab+slot*2]]=true;
        }
        result["battle"]["deployment_area"]=area;
    }
    return result;
}
void OriginalSession::tick(int frames){if(frames<0||frames>3600)throw std::out_of_range("Frame increment");clock_=static_cast<std::uint16_t>(clock_+frames);}
Json OriginalSession::prepare_development(int city,int officer,int kind){
    if(auto error=command_error();!error.empty())return {{"error",error}};
    if(!search_.is_null())return {{"error","请先处理搜索报告"}};
    if(!pending_.is_null())return {{"error","请先完成或取消当前开发方案"}};
    if(city<0||city>=30||kind<0||kind>2)return {{"error","无效城池或开发类型"}};
    const auto &bytes=state_.sram();const int ruler=bytes[0xd8b];
    if((bytes[city*36]&7)!=ruler)return {{"error","只能开发己方城池"}};
    if(bytes[0xd8f+4*ruler]==0)return {{"error","命令书已用完"}};
    const auto offer=state_.next_development_offer(city,officer,kind,random_);
    if(offer.is_null())return {{"error","请选择本城武将"}};
    pending_={{"city",city},{"officer",officer},{"kind",kind},{"proposal",offer["index"]},{"cost",offer["cost"]},{"dialogue_id",offer["dialogue_id"]}};
    return pending_;
}
std::string OriginalSession::confirm_development(){
    if(auto error=command_error();!error.empty())return error;
    if(pending_.is_null())return "没有待确认的开发方案";
    auto result=state_.develop(pending_["city"],pending_["officer"],pending_["kind"],pending_["proposal"],static_cast<std::uint8_t>(clock_));
    if(result.empty())pending_=nullptr;
    return result;
}
std::string OriginalSession::move(int source,int destination,const std::vector<int>&officers){
    if(auto error=command_error();!error.empty())return error;
    if(!search_.is_null())return "请先处理搜索报告";
    if(!pending_.is_null())return "请先完成或取消当前开发方案";
    return state_.move_officers(source,destination,officers);
}
std::string OriginalSession::search(int city,int officer){
    if(auto error=command_error();!error.empty())return error;
    if(!pending_.is_null()||!search_.is_null())return "请先处理当前方案或报告";
    return state_.dispatch_search(city,officer);
}
Json OriginalSession::expedition_quote(int source,int target,const std::vector<int> &officers,int leader)const{
    if(auto error=command_error();!error.empty())return {{"error",error}};
    if(!pending_.is_null()||!search_.is_null())return {{"error","请先处理当前方案或搜索报告"}};
    return state_.expedition_quote(source,target,officers,leader);
}
Json OriginalSession::dispatch_expedition(int source,int target,const std::vector<int> &officers,int leader){
    const auto quote=expedition_quote(source,target,officers,leader);if(quote.contains("error"))return quote;
    auto next=state_;
    try {
        auto result=next.dispatch_expedition(source,target,officers,leader);
        if(result.contains("error"))return result;
        result["defenders"]=next.collect_defenders(target);
        state_=std::move(next);battle_=result;phase_="expedition";ai_=nullptr;
        return result;
    }catch(const std::exception &e){return {{"error",e.what()}};}
}
Json OriginalSession::execute_command(int city,const std::string &kind,const Json &args){
    if(auto error=command_error();!error.empty())return {{"error",error}};
    if(!pending_.is_null()||!search_.is_null())return {{"error","请先处理当前方案或搜索报告"}};
    return state_.execute_command(city,kind,args,random_,static_cast<std::uint8_t>(clock_));
}

std::string OriginalSession::begin_tactical_retreat(int slot){
    if(auto error=tactical_action_error();!error.empty())return error;
    if((phase_!="expedition"&&phase_!="battle")||!battle_.contains("tactics"))return "请先进入战术指令";
    auto &t=battle_["tactics"];
    if(t.contains("attack")||t.contains("scout")||t.contains("retreat"))return "请先处理当前战术指令";
    if(t["points"]==0)return "撤退需要 1 点机动力";
    if(t["moves"].size()>=1022)return "当前行动记录已满";
    const bool defending=tactical_side()==0;
    if(slot<0||slot>=(defending?12:11)||state_.sram()[defending?0xdaa+slot*2:0xdc2+slot*3]>=241)return "请选择本方部队";
    t["selected"]=slot;t["retreat"]={{"slot",slot},{"commander",!defending&&bool(state_.sram()[0xdc4+slot*3]&128)},{"stage","confirm"}};
    if(defending)t["retreat"]["defending"]=true;
    t["moves"].push_back({{"kind","begin_retreat"},{"slot",slot}});return {};
}
std::string OriginalSession::cancel_tactical_retreat(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("retreat")||battle_["tactics"]["retreat"]["stage"]!="confirm")return "当前没有可取消的撤退";
    auto &t=battle_["tactics"];t.erase("retreat");t["moves"].push_back({{"kind","cancel_retreat"}});return {};
}
std::string OriginalSession::confirm_tactical_retreat(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("retreat")||battle_["tactics"]["retreat"]["stage"]!="confirm")return "当前没有待确认撤退";
    auto next=*this;auto &t=next.battle_["tactics"];auto &retreat=t["retreat"];
    const int cursor=next.random_;const bool defending=retreat.value("defending",false);
    Json outcomes=Json::array();
    if(defending){
        auto result=next.state_.retreat_defender(retreat["slot"],next.battle_["target"],true,next.random_);
        if(result.contains("error"))return result["error"];
        if(result["departed"])t["points"]=t["points"].get<int>()-1;
        outcomes.push_back(std::move(result));
    }else if(retreat["commander"]){
        for(int i=0;i<12;++i){auto result=next.state_.retreat_army_step(next.random_);
            if(result.contains("error"))return result["error"];
            if(result["done"])break;outcomes.push_back(std::move(result));}
    }else{
        auto result=next.state_.retreat_attacker(retreat["slot"]);if(result.contains("error"))return result["error"];
        outcomes.push_back(std::move(result));
    }
    if(!defending)t["points"]=t["points"].get<int>()-1;
    bool remaining=false;for(int i=0;i<11;++i)remaining|=next.state_.sram()[0xdc2+i*3]!=255;
    if(defending){
        bool defenders=false;for(int i=0;i<11;++i)defenders|=next.state_.sram()[0xdaa+i*2]!=255;
        // D39D/BEFE counts the first eleven defender slots, even with slot 12 alive.
        remaining=remaining&&defenders;
        if(!outcomes[0]["departed"].get<bool>())remaining=true;
    }
    retreat["stage"]="result";retreat["outcomes"]=outcomes;retreat["ended"]=!remaining;
    t["moves"].push_back({{"kind","confirm_retreat"},{"cursor",cursor}});
    *this=std::move(next);return {};
}
std::string OriginalSession::finish_tactical_retreat(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("retreat")||battle_["tactics"]["retreat"]["stage"]!="result")return "当前没有撤退结果";
    auto &t=battle_["tactics"];
    if(t["retreat"].value("defending",false)){
        const bool ended=t["retreat"]["ended"];
        t["last_retreat"]=t["retreat"];t.erase("retreat");
        if(ended){t["turn_boundary"]="battle_result";t["turn_reason"]=2;}
        else {const int selected=first_tactical_slot();if(selected>=0)t["selected"]=selected;}
        t["moves"].push_back({{"kind","finish_retreat"}});return {};
    }
    if(phase_=="battle"){
        if(t["moves"].size()>=1024)return "当前行动记录已满";
        const bool ended=t["retreat"]["ended"];
        t["last_retreat"]=t["retreat"];t.erase("retreat");t["attacker_ai"]["stage"]="assess";
        if(ended){t["turn_boundary"]="battle_result";t["turn_reason"]=36;}
        else{
            // D39D returns to C489 even when mobility remains. A computer
            // commander can leave alone; its absence then selects reason20.
            Json context={{"side",128},{"points",0},{"round",t["round"]},{"carry",t["carry"]},
                {"phase",5},{"counter",1},{"reason",0},{"status",t["status"]}};
            const auto boundary=state_.tactical_handover(context);
            if(!boundary.contains("error")&&boundary["phase"]==14){t["turn_boundary"]="battle_result";t["turn_reason"]=boundary["reason"];}
            const int selected=first_tactical_slot();if(selected>=0)t["selected"]=selected;
        }
        t["moves"].push_back({{"kind","finish_retreat"}});return {};
    }
    if(t["retreat"]["ended"]&&has_battle_captives()){
        if(t["moves"].size()>=1024)return "当前行动记录已满";
        t["last_retreat"]=t["retreat"];t.erase("retreat");
        t["turn_boundary"]="battle_result";t["turn_reason"]=36;
        t["moves"].push_back({{"kind","finish_retreat"}});return {};
    }
    if(t["retreat"]["ended"]){state_.close_battle(battle_["target"]);battle_=nullptr;enter_turn();}
    else{
        t.erase("retreat");for(int i=0;i<11;++i)if(state_.sram()[0xdc2+i*3]!=255){t["selected"]=i;break;}
        t["moves"].push_back({{"kind","finish_retreat"}});
    }
    return {};
}
std::string OriginalSession::begin_tactics(){
    if(phase_!="expedition"&&phase_!="battle")return "请先进入已布阵的战场";
    if(!battle_.contains("deployment")||battle_["deployment"]["current"]!=-1||battle_["deployment"].value("handover",false))return "请先完成双方布阵与交接";
    if(battle_.contains("tactics"))return "已进入战术指令";
    int selected=-1;for(int i=0;i<11;++i)if(state_.sram()[0xdc2+i*3]!=255){selected=i;break;}
    if(selected<0)return "没有可行动部队";
    computer_scratch21_=-1;computer_argument_=-1;handover_argument_=-1;
    battle_["tactics"]={{"deployment_sram",state_.sram()},{"points",state_.tactical_mobility()},{"selected",selected},{"moves",Json::array()}};
    if(track_argument_)battle_["tactics"]["entry_argument"]=persistent_argument_;
    if(phase_=="battle"){
        Json context={{"side",0},{"points",0},{"round",0},{"carry",0},{"phase",5},{"counter",1},{"reason",0},{"status",Json(std::vector<int>(24,0))}};
        const auto boundary=state_.tactical_handover(context);
        if(boundary.contains("error")||boundary["phase"]!=5){battle_.erase("tactics");return "无效电脑先攻布阵结果";}
        auto &t=battle_["tactics"];for(const auto *key:{"side","round","carry","status"})t[key]=boundary[key];
        t["points"]=std::min(40,boundary["points"].get<int>());t["turn_boundary"]="computer";t["computer_cursor"]=0;t["attacker_initial_reuse"]=true;
    }
    return {};
}
std::string OriginalSession::select_tactical_unit(int slot){
    if(auto error=tactical_action_error();!error.empty())return error;
    if(!battle_.is_object()||!battle_.contains("tactics")||slot<0||slot>=(tactical_side()?11:12)||state_.sram()[tactical_side()?0xdc2+slot*3:0xdaa+slot*2]>=241)return "请选择本方战场武将";
    if((battle_["tactics"].contains("attack")||battle_["tactics"].contains("scout")||battle_["tactics"].contains("retreat"))&&battle_["tactics"]["selected"]!=slot)return "请先处理当前攻击";
    battle_["tactics"]["selected"]=slot;return {};
}
std::string OriginalSession::move_tactical(int slot,int direction){
    if(auto error=tactical_action_error();!error.empty())return error;
    if((phase_!="expedition"&&phase_!="battle")||!battle_.contains("tactics"))return "请先进入战术移动";
    if(battle_["tactics"].contains("attack")||battle_["tactics"].contains("scout")||battle_["tactics"].contains("retreat"))return "请先处理当前攻击或侦察报告";
    if(battle_["tactics"]["moves"].size()>=1022)return "当前行动记录已满";
    auto next=*this;int points=next.battle_["tactics"]["points"];
    const auto error=next.state_.tactical_step(slot,direction,points,tactical_side());if(!error.empty())return error;
    next.battle_["tactics"]["points"]=points;next.battle_["tactics"]["selected"]=slot;
    next.battle_["tactics"]["moves"].push_back({{"slot",slot},{"direction",direction}});
    *this=std::move(next);return {};
}
std::string OriginalSession::adjust_tactical_formation(int slot,int direction){
    if(auto error=tactical_action_error();!error.empty())return error;
    if((phase_!="expedition"&&phase_!="battle")||!battle_.contains("tactics"))return "请先进入战术指令";
    auto &t=battle_["tactics"];
    if(t.contains("attack")||t.contains("scout")||t.contains("retreat"))return "请先处理当前攻击或侦察报告";
    if(t["points"]==0)return "本方机动力已用尽";
    if(t["moves"].size()>=1022)return "当前行动记录已满";
    const auto error=state_.tactical_formation(slot,direction,tactical_side());if(!error.empty())return error;
    t["selected"]=slot;t["moves"].push_back({{"kind","formation"},{"slot",slot},{"direction",direction}});return {};
}
std::string OriginalSession::scout_tactical(int slot,int target_slot){
    if(auto error=tactical_action_error();!error.empty())return error;
    if((phase_!="expedition"&&phase_!="battle")||!battle_.contains("tactics"))return "请先进入战术指令";
    auto &t=battle_["tactics"];
    if(t.contains("attack")||t.contains("scout")||t.contains("retreat"))return "请先处理当前攻击或侦察报告";
    if(t["moves"].size()>=1022)return "当前行动记录已满";
    if(target_slot<0||target_slot>=(tactical_side()?12:11)||state_.sram()[tactical_side()?0xdaa+target_slot*2:0xdc2+target_slot*3]>=241)return "请选择敌军部队";
    int points=t["points"];auto report=state_.tactical_scout(slot,state_.sram()[tactical_side()?0xdab+target_slot*2:0xdc3+target_slot*3],points,tactical_side());
    if(report.contains("error"))return report["error"];
    t["selected"]=slot;t["points"]=points;t["scout"]=std::move(report);
    t["moves"].push_back({{"kind","scout"},{"slot",slot},{"target_slot",target_slot}});return {};
}
std::string OriginalSession::close_tactical_scout(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("scout"))return "当前没有侦察报告";
    auto &t=battle_["tactics"];t.erase("scout");t["moves"].push_back({{"kind","close_scout"}});return {};
}
std::string OriginalSession::attack_tactical(int slot,int direction){
    if(auto error=tactical_action_error();!error.empty())return error;
    if((phase_!="expedition"&&phase_!="battle")||!battle_.contains("tactics"))return "请先进入战术指令";
    if(battle_["tactics"].contains("attack")||battle_["tactics"].contains("scout")||battle_["tactics"].contains("retreat"))return "请先处理当前攻击或侦察报告";
    if(battle_["tactics"]["moves"].size()>=1022)return "当前行动记录已满";
    int points=battle_["tactics"]["points"];auto attack=state_.tactical_attack(slot,direction,points,tactical_side());
    if(attack.contains("error"))return attack["error"];
    auto &t=battle_["tactics"];t["points"]=points;t["selected"]=slot;t["attack"]=std::move(attack);
    t["moves"].push_back({{"kind","attack"},{"slot",slot},{"direction",direction}});return {};
}
std::string OriginalSession::cancel_tactical_attack(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack")||battle_["tactics"]["attack"]["stage"]!="confirm")return "当前没有可取消的攻击";
    auto &t=battle_["tactics"];t.erase("attack");t["moves"].push_back({{"kind","cancel_attack"}});return {};
}
std::string OriginalSession::confirm_tactical_attack(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack")||battle_["tactics"]["attack"]["stage"]!="confirm")return "当前没有待确认攻击";
    auto &t=battle_["tactics"];const int owner=tactical_side()?(state_.sram()[0xde3]&7):(state_.sram()[0xde3]>>4);
    const bool human=(state_.sram()[0xd89]&7)==owner||((state_.sram()[0xd8a]&128)&&(state_.sram()[0xd8a]&7)==owner);
    const auto frame=static_cast<std::uint8_t>(clock_);
    t["attack"]["clash"]=state_.prepare_clash(t["attack"],human,frame);t["attack"]["stage"]="clash_ready";
    t["moves"].push_back({{"kind","confirm_attack"},{"frame",frame}});return {};
}
std::string OriginalSession::begin_clash(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack")||battle_["tactics"]["attack"]["stage"]!="clash_ready")return "请先完成交战准备";
    if(battle_["tactics"]["moves"].size()>=1024)return "当前行动记录已满";
    auto next=*this;auto &t=next.battle_["tactics"];auto &clash=t["attack"]["clash"];
    const auto &s=next.state_.sram();
    auto player=[&](int faction){return faction==(s[0xd89]&7)?1:((s[0xd8a]&128)&&faction==(s[0xd8a]&7)?2:0);};
    const int attacker=s[0xde3]>>4,defender=s[0xde3]&7;
    clash["human_mask"]=(player(attacker)<<4)|player(defender);clash["tactical_side"]=tactical_side();
    auto runtime=next.state_.initialize_clash(clash);if(runtime.contains("error"))return runtime["error"];
    const bool swapped=clash["orientation"].get<int>()&128;
    const int acting=tactical_side()?attacker:defender,opposing=tactical_side()?defender:attacker;
    clash["factions"]=Json::array({swapped?opposing:acting,swapped?acting:opposing});
    clash["players"]=Json::array({player(clash["factions"][0]),player(clash["factions"][1])});
    clash["runtime"]=std::move(runtime);t["attack"]["stage"]="clash_orders";
    t["moves"].push_back({{"kind","begin_clash"}});*this=std::move(next);return {};
}
std::string OriginalSession::advance_clash(std::string *sound){
    if(sound)sound->clear();
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack"))return "当前没有交锋";
    const auto stage=battle_["tactics"]["attack"]["stage"];
    if(stage!="clash_orders"&&stage!="clash_running")return "当前交锋需处理对话或尚未接入的后续流程";
    auto next=*this;auto &t=next.battle_["tactics"];auto &attack=t["attack"];auto &clash=attack["clash"];
    if(t["moves"].size()>1018)return "当前行动记录已满，请保存交锋进度";
    auto &runtime=clash["runtime"];const int cursor=next.random_;
    if(stage=="clash_orders"&&attack.value("duel_return",false)){
        runtime.update({{"phase",2},{"counter",0}});attack.erase("duel_return");
    }else if(stage=="clash_orders"){
        // Initial order menus return to phase 2, then army handover starts side 0.
        // 9ACE resets action scratch/sequential mode; the final roster painter
        // leaves direction 139. AI status is inert until the next NPC dispatch.
        runtime.update({{"global_phase",10},{"phase",2},{"counter",0},{"extra",0},{"direction",139},
            {"distance",0},{"x",0},{"y",0},{"target",0},{"command",0},{"sequential",0},
            {"ai_status",255},{"ai_mode",128},{"ai_side",0}});
    }
    const auto resume=runtime;
    auto result=next.state_.clash_step(runtime,clash,next.random_);if(result.contains("error"))return result["error"];
    runtime=std::move(result);attack["stage"]="clash_running";
    if(runtime["global_phase"]==13){attack["stage"]="clash_boundary";attack["boundary"]="duel";}
    else if(runtime.contains("boundary")){attack["stage"]="clash_boundary";attack["boundary"]=runtime["boundary"];}
    else if(runtime["phase"]==5||runtime["phase"]==7){
        attack["stage"]="clash_boundary";attack["boundary"]=runtime["phase"]==5?"retreat":"general_defeat";
    }else if(runtime["phase"]==6){
        const int side=runtime["active"].get<int>()?1:0,officer=clash[side?"second":"first"];
        if(officer<6)return "君主不能进入普通武将投降流程";
        attack["surrender"]={{"side",side},{"officer",officer},{"resume_runtime",resume},{"resume_stage","clash_running"}};
        const bool human=clash["players"][side]!=0;
        runtime["counter"]=human?2:5;attack["stage"]=human?"surrender_confirm":"surrender_notice";
    }
    // Original A56E starts descriptor 25 for melee; A578 starts 23 and 24
    // for an arrow. A964 plays 25 again when that arrow hits an enemy.
    // Generals entering the duel branch do not play either launch sound.
    std::string cue;
    if(resume["global_phase"]==10&&resume["phase"]==4&&resume["counter"]==1&&
       runtime["global_phase"]==10&&runtime["phase"]==4){
        if(runtime["counter"]==3)cue="clash_hit";
        else if(runtime["counter"]==6)cue="clash_bow";
    }
    if(resume["global_phase"]==10&&resume["phase"]==4&&resume["counter"]==7&&
       runtime["global_phase"]==10&&runtime["phase"]==4&&runtime["counter"]==3)cue="clash_hit";
    t["moves"].push_back({{"kind","advance_clash"},{"cursor",cursor}});*this=std::move(next);
    if(sound)*sound=std::move(cue);
    return {};
}
std::string OriginalSession::resume_clash_strategy(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack"))return "当前没有待继续的电脑军令";
    const auto &current=battle_["tactics"]["attack"];
    if(current["stage"]!="clash_boundary"||current.value("boundary",std::string{})!="ai_scratch")return "当前没有待继续的电脑军令";
    const auto &old=current["clash"]["runtime"];
    if(old["global_phase"]!=10||old["phase"]!=3||old["ai_side"]!=0)return "无效电脑军令阶段";
    if(battle_["tactics"]["moves"].size()>1018)return "当前行动记录已满，请保存交锋进度";
    auto next=*this;auto &t=next.battle_["tactics"];auto &attack=t["attack"];auto &clash=attack["clash"];auto &runtime=clash["runtime"];
    auto input=clash;input.update(runtime);input["scene"]=clash["map"];
    const int cursor=next.random_;
    auto result=next.state_.resolve_clash_strategy(runtime["units"],runtime["orders"],input,next.random_);
    if(result.contains("error"))return result["error"];
    runtime.update(result);runtime.update({{"phase",4},{"counter",0}});runtime.erase("boundary");
    attack.erase("boundary");attack["stage"]="clash_running";
    t["moves"].push_back({{"kind","resume_clash_strategy"},{"cursor",cursor}});*this=std::move(next);return {};
}
std::string OriginalSession::recover_clash_strategy(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack"))return "当前没有待恢复的电脑军令";
    const auto &attack=battle_["tactics"]["attack"];
    if(attack["stage"]!="clash_boundary"||attack.value("boundary",std::string{})!="ai_scratch"||
       attack["clash"]["runtime"].contains("tail"))return "当前没有待恢复的电脑军令";
    // E02D-E042 clears RAM 0004-07FF at reset. The normal <=10-troop
    // 9BD7-9D64 initializer and subsequent unit kernels only write 04B0-04F1.
    // Retained reference scenes and sentinel traces are recorded in
    // reference/fixtures/clash-tail-lifetime.json. This is the supported native
    // reset-memory profile, not an import of arbitrary emulator RAM.
    // A separate event keeps old unknown-memory histories exactly replayable.
    auto next=*this;
    next.battle_["tactics"]["attack"]["clash"]["runtime"]["tail"]=Json::array({0,0,0});
    const auto error=next.resume_clash_strategy();if(!error.empty())return error;
    next.battle_["tactics"]["moves"].back()["kind"]="recover_clash_strategy";
    *this=std::move(next);return {};
}
// Continue the existing phase-5 boundary, so old advance_clash histories keep
// their exact replay result. Initial fade/text setup (AEBE/AB49) is folded to
// AB5B injury; subsequent calls acknowledge the original waiting boundaries.
std::string OriginalSession::advance_clash_retreat(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack"))return "当前没有交锋撤离";
    const auto &current=battle_["tactics"]["attack"];
    const bool entering=current["stage"]=="clash_boundary"&&current.value("boundary",std::string{})=="retreat";
    if(!entering&&current["stage"]!="clash_retreat")return "当前没有待推进的交锋撤离";
    const auto &old_runtime=current["clash"]["runtime"];
    if(old_runtime["global_phase"]!=10||old_runtime["phase"]!=5||(entering&&old_runtime["counter"]!=0))return "无效交锋撤离阶段";
    const int active=old_runtime["active"];
    if(active!=0&&active!=128)return "撤离对象必须是本军主将";
    const auto count=battle_["tactics"]["moves"].size();
    if(count>=1024||(entering&&count>1019))return "当前行动记录已满，请保存交锋进度";
    auto next=*this;auto &t=next.battle_["tactics"];auto &attack=t["attack"];auto &clash=attack["clash"];auto &runtime=clash["runtime"];
    const int cursor=next.random_,side=active?1:0,officer=clash[side?"second":"first"];
    if(entering){
        const int hp=runtime["units"][side*33+1];
        auto result=next.state_.clash_retreat_injury(runtime["units"],active,next.random_);
        if(result.contains("error"))return result["error"];
        attack["retreat"]={{"side",side},{"officer",officer},{"hp_before",hp},{"injury",result},{"losses",0}};
        attack.erase("boundary");attack["stage"]="clash_retreat";runtime["counter"]=result["counter"];
    }else{
        auto context=clash;context.update({{"target",next.battle_["target"]},{"active",active},{"counter",runtime["counter"]}});
        auto result=next.state_.clash_retreat_confirm(runtime["units"],context,next.random_);
        if(result.contains("error"))return result["error"];
        runtime["counter"]=result["counter"];
        if(result.contains("losses"))attack["retreat"]["losses"]=result["losses"];
        if(result["phase"]!=10){
            const bool restored_map=result["phase"]==11,defeated=runtime["units"][side*33]==255;
            if(restored_map){const auto error=next.state_.restore_tactical_board(next.battle_["target"]);if(!error.empty())return error;}
            int selected=-1,defenders=0;bool commander=false;const auto &raw=next.state_.sram();
            for(int i=0;i<11;++i)if(raw[0xdc2+i*3]!=255){if(selected<0)selected=i;commander|=(raw[0xdc4+i*3]&128)!=0;}
            for(int i=0;i<12;++i)defenders+=raw[0xdaa+i*2]!=255;
            if(selected>=0){const int next_selected=next.first_tactical_slot();if(next_selected>=0)t["selected"]=next_selected;}
            result.update({{"kind","retreat"},{"officer",officer},{"defeated",defeated},
                {"losses",attack["retreat"]["losses"]},{"map_restored",restored_map},
                {"can_continue",restored_map&&selected>=0&&defenders>0&&commander&&!(defeated&&officer<6)}});
            runtime["global_phase"]=result["phase"];attack["result"]=std::move(result);attack["stage"]="clash_result";
        }
    }
    t["moves"].push_back({{"kind","advance_clash_retreat"},{"cursor",cursor}});*this=std::move(next);return {};
}
// AFAE phase 7: fold the initial fade/readiness wait into AFBA/AFD3,
// then acknowledge B006/B009. Active is the winning unit, not the loser.
std::string OriginalSession::advance_clash_defeat(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack"))return "当前没有武将败北结果";
    const auto &current=battle_["tactics"]["attack"];
    const bool entering=current["stage"]=="clash_boundary"&&current.value("boundary",std::string{})=="general_defeat";
    if(!entering&&current["stage"]!="clash_defeat")return "当前没有待确认的武将败北";
    const auto &old=current["clash"]["runtime"];
    if(old["global_phase"]!=10||old["phase"]!=7||old["counter"]!=(entering?0:2))return "无效武将败北阶段";
    const int active=old["active"];
    if(active<0||active>138||(active&0x70)||(active&15)>10||old["units"][(active&128)?0:33]!=255)return "战败武将仍在交锋中";
    const auto count=battle_["tactics"]["moves"].size();
    if(count>=1024||(entering&&count>1021))return "当前行动记录已满，请保存交锋进度";
    auto next=*this;auto &t=next.battle_["tactics"];auto &attack=t["attack"];auto &clash=attack["clash"];auto &runtime=clash["runtime"];
    if(entering){
        auto result=next.state_.clash_general_defeat(clash["first"],clash["second"],active,clash["orientation"],clash["tactical_side"]);
        if(result.contains("error"))return result["error"];
        attack["defeat"]=std::move(result);attack.erase("boundary");attack["stage"]="clash_defeat";runtime["counter"]=2;
    }else{
        auto context=clash;context.update({{"active",active},{"target",next.battle_["target"]}});
        auto result=next.state_.finish_clash_defeat(runtime["units"],context);
        if(result.contains("error"))return result["error"];
        const bool restored_map=result["phase"]==11;const int officer=result["officer"];
        if(restored_map){const auto error=next.state_.restore_tactical_board(next.battle_["target"]);if(!error.empty())return error;}
        int selected=-1,defenders=0;bool commander=false;const auto &raw=next.state_.sram();
        for(int i=0;i<11;++i)if(raw[0xdc2+i*3]!=255){if(selected<0)selected=i;commander|=(raw[0xdc4+i*3]&128)!=0;}
        for(int i=0;i<12;++i)defenders+=raw[0xdaa+i*2]!=255;
        if(selected>=0){const int next_selected=next.first_tactical_slot();if(next_selected>=0)t["selected"]=next_selected;}
        result.update({{"kind","defeat"},{"defeated",true},{"map_restored",restored_map},
            {"can_continue",restored_map&&selected>=0&&defenders>0&&commander&&officer>=6}});
        runtime["global_phase"]=result["phase"];runtime["counter"]=result["counter"];
        attack["result"]=std::move(result);attack["stage"]="clash_result";
    }
    t["moves"].push_back({{"kind","advance_clash_defeat"}});*this=std::move(next);return {};
}
std::string OriginalSession::cycle_clash_order(int side,int kind){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack"))return "请先进入交战军令";
    if(side<0||side>1||kind<0||kind>3)return "无效军令兵种";
    auto &t=battle_["tactics"];const auto stage=t["attack"]["stage"];
    if(stage!="clash_orders"&&stage!="clash_running")return "请先进入交战军令";
    auto &clash=t["attack"]["clash"];
    if(stage=="clash_running"&&(clash["runtime"]["phase"]!=4||clash["runtime"]["counter"]!=1))return "请等待当前兵队行动结束后再调整军令";
    if(clash["players"][side]==0)return "电脑一方不能由玩家下令";
    if(t["moves"].size()>=1024)return "当前行动记录已满";
    auto context=clash;context.update({{"side",side*128},{"row",kind},{"return_counter",side?14:13}});
    auto result=state_.clash_menu(clash["runtime"]["orders"],context,1);
    if(result.contains("error"))return result["error"];
    clash["runtime"]["orders"]=result["orders"];
    t["moves"].push_back({{"kind","clash_order"},{"side",side},{"unit_kind",kind}});return {};
}
std::vector<std::uint8_t> OriginalSession::clash_rgb() const {
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack"))return {};
    const std::string stage=battle_["tactics"]["attack"]["stage"];
    const bool held_result=stage=="clash_result"&&!battle_["tactics"]["attack"]["result"].value("map_restored",true);
    if(!held_result&&stage.rfind("duel_",0)!=0&&stage!="clash_orders"&&stage!="clash_running"&&stage!="clash_boundary"&&stage!="clash_retreat"&&stage!="clash_defeat"&&stage!="surrender_confirm"&&stage!="surrender_notice"&&stage!="surrender_accepted")return {};
    const auto &clash=battle_["tactics"]["attack"]["clash"];
    return rom_.clash_rgb(clash["map"],clash["factions"][0],clash["factions"][1],clash["runtime"]["units"]);
}
bool OriginalSession::has_battle_captives() const {
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("captives"))return false;
    for(const auto &id:battle_["tactics"]["captives"])if(id!=255)return true;
    return false;
}
std::string OriginalSession::request_clash_surrender(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack")||battle_["tactics"]["attack"]["stage"]!="clash_orders")return "请先进入交战军令";
    auto &t=battle_["tactics"];auto &attack=t["attack"];auto &clash=attack["clash"];
    // Reserve the remaining acknowledgement/return records before starting.
    if(t["moves"].size()>1019)return "当前行动记录已满";
    const auto &orders=clash["runtime"]["orders"];
    const int side=orders[3]==3?0:(orders[7]==3?1:-1);
    if(side<0)return "请将本方普通武将的军令切换为投降";
    const int officer=clash[side?"second":"first"];
    if(clash["players"][side]==0||officer<6)return "只能提交本方普通武将的投降军令";
    attack["surrender"]={{"side",side},{"officer",officer},{"resume_runtime",clash["runtime"]}};
    // AE1F/AE31/AE3E: a human surrender first waits at the yes/no prompt.
    clash["runtime"].update({{"phase",6},{"counter",2},{"active",side*128}});
    attack["stage"]="surrender_confirm";t["moves"].push_back({{"kind","request_surrender"}});return {};
}
std::string OriginalSession::answer_clash_surrender(bool accept){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack")||battle_["tactics"]["attack"]["stage"]!="surrender_confirm")return "当前没有待确认投降";
    auto &t=battle_["tactics"];auto &attack=t["attack"];
    if(!accept){
        attack["clash"]["runtime"]=attack["surrender"]["resume_runtime"];
        attack["stage"]=attack["surrender"].value("resume_stage",std::string("clash_orders"));attack.erase("surrender");
    }else{
        // AE78 -> AEA5/AECC: presentation setup ends at counter 5.
        attack["clash"]["runtime"]["counter"]=5;attack["stage"]="surrender_notice";
    }
    t["moves"].push_back({{"kind","answer_surrender"},{"accept",accept}});return {};
}
std::string OriginalSession::advance_clash_surrender(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack"))return "当前没有投降对话";
    const auto stage=battle_["tactics"]["attack"]["stage"];
    if(stage!="surrender_notice"&&stage!="surrender_accepted")return "当前没有投降对话";
    auto next=*this;auto &t=next.battle_["tactics"];auto &attack=t["attack"];auto &clash=attack["clash"];
    const int cursor=next.random_;
    if(stage=="surrender_notice"){
        // AEF9: first acknowledgement opens the opponent's acceptance.
        clash["runtime"]["counter"]=6;attack["stage"]="surrender_accepted";
    }else{
        auto context=clash;context["active"]=attack["surrender"]["side"].get<int>()*128;context["target"]=next.battle_["target"];
        auto captives=t.value("captives",Json(std::vector<int>(24,255)));
        auto result=next.state_.clash_surrender(clash["runtime"]["units"],captives,context,next.random_);
        if(result.contains("error"))return result["error"];
        const auto error=next.state_.restore_tactical_board(next.battle_["target"]);if(!error.empty())return error;
        t["captives"]=std::move(captives);
        int selected=-1,defenders=0;bool commander=false;const auto &s=next.state_.sram();
        for(int i=0;i<11;++i)if(s[0xdc2+i*3]!=255){if(selected<0)selected=i;commander|=(s[0xdc4+i*3]&128)!=0;}
        for(int i=0;i<12;++i)defenders+=s[0xdaa+i*2]!=255;
        if(selected>=0){const int next_selected=next.first_tactical_slot();if(next_selected>=0)t["selected"]=next_selected;}
        result["can_continue"]=selected>=0&&defenders>0&&commander;
        result["kind"]="surrender";attack["result"]=std::move(result);
        clash["runtime"].update({{"phase",11},{"counter",0}});attack["stage"]="clash_result";
    }
    t["moves"].push_back({{"kind","advance_surrender"},{"cursor",cursor}});*this=std::move(next);return {};
}
std::string OriginalSession::finish_clash_result(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack")||battle_["tactics"]["attack"]["stage"]!="clash_result")return "当前没有交战结果";
    auto &t=battle_["tactics"];
    if(!t["attack"]["result"]["can_continue"].get<bool>())return "交锋已触发战役结果，请先确认战果或查看君主失败报告";
    t["last_clash"]=t["attack"]["result"];t.erase("attack");
    t["moves"].push_back({{"kind","finish_clash_result"}});return {};
}
std::string OriginalSession::prepare_deployment(){
    if((phase_!="expedition"&&phase_!="battle")||!battle_.is_object())return "当前没有待部署战斗";
    auto next=*this;
    try{
        const int source=battle_["source"],target=battle_["target"];const auto &s=state_.sram();
        if(battle_.contains("deployment")){
            if(!battle_["deployment"].value("handover",false))return "已进入布阵";
            int current=-1;for(int i=0;i<11;++i)if(s[0xdc2+i*3]!=255){current=i;break;}
            if(current<0)return "没有出征部队";
            next.state_.start_deployment_unit(target,current);
            next.battle_["deployment"]["current"]=current;next.battle_["deployment"]["handover"]=false;
        }else{
            const int owner=s[target*36]&7;
            const bool two_players=phase_=="expedition"&&(owner==(s[0xd89]&7)||((s[0xd8a]&128)&&owner==(s[0xd8a]&7)));
            const bool defending=phase_=="battle"||two_players;
            const std::vector<std::uint8_t> attackers(s.begin()+0xdc2,s.begin()+0xde3);
            const int current=next.state_.prepare_deployment(source,target,defending,two_players);
            Json defenders=Json::array();for(int i=0;i<12;++i)if(next.state_.sram()[0xdaa+i*2]!=255)defenders.push_back(next.state_.sram()[0xdaa+i*2]);
            if(phase_=="expedition")next.battle_["defenders"]=defenders;
            next.battle_["deployment"]={{"current",current}};
            if(two_players)next.battle_["deployment"].update({{"side","defender"},{"two_players",true},{"handover",false}});
            else if(defending){next.battle_["deployment"]["side"]="defender";next.battle_["deployment"]["attackers"]=attackers;}
        }
        *this=std::move(next);return {};
    }catch(const std::exception &e){return e.what();}
}
std::string OriginalSession::move_deployment(int direction){
    if((phase_!="expedition"&&phase_!="battle")||!battle_.contains("deployment")||battle_["deployment"]["current"]==-1)return "当前没有待部署部队";
    return state_.move_deployment_unit(battle_["target"],battle_["deployment"]["current"],direction,defending_deployment());
}
std::string OriginalSession::confirm_deployment(){
    if((phase_!="expedition"&&phase_!="battle")||!battle_.contains("deployment")||battle_["deployment"]["current"]==-1)return "当前没有待部署部队";
    auto next=*this;
    try{
        int current=next.battle_["deployment"]["current"],following=-1;
        const bool defending=defending_deployment();
        for(int i=current+1;i<(defending?12:11);++i)if(next.state_.sram()[(defending?0xdaa:0xdc2)+i*(defending?2:3)]!=255){following=i;break;}
        if(following>=0)next.state_.start_deployment_unit(next.battle_["target"],following,defending);
        else if(defending){
            if(next.battle_["deployment"].value("two_players",false)){
                next.battle_["deployment"]["side"]="attacker";next.battle_["deployment"]["handover"]=true;
            }else next.state_.finish_defense_deployment(next.battle_["target"]);
        }
        next.battle_["deployment"]["current"]=following;*this=std::move(next);return {};
    }catch(const std::exception &e){return e.what();}
}
Json OriginalSession::visit_city(int city){
    if(phase_!="player_commands"||!army_.is_null())return nullptr;
    if(!search_.is_null())return search_;
    if(!pending_.is_null())return nullptr;
    // Keep collection/result generation atomic if unsupported state is loaded.
    auto next=state_;auto cursor=random_;
    const int officer=next.collect_search(city);if(officer<0)return nullptr;
    bool exclude=false;Json found;
    for(;;) {
        const int kind=next.search_kind(officer,cursor,exclude);
        found=next.search_find(city,kind,cursor);
        if(!found.value("reroll",false))break;
        exclude=found.value("exclude_people",false);
    }
    found["city"]=city;found["officer"]=officer;
    state_=std::move(next);random_=cursor;search_=found;
    return search_;
}
Json OriginalSession::finish_search(bool accept){
    if(auto error=command_error();!error.empty())return {{"error",error}};
    if(search_.is_null())return {{"error","没有搜索报告"}};
    auto result=search_;const int kind=result["kind"];
    if(kind>=6 && kind<=8) {
        result["joined"]=(accept||kind==8) && state_.recruit_search(result["city"],result["candidate"],kind,result["value"],random_);
    }
    search_=nullptr;return result;
}
Json OriginalSession::save() const {
    Json result={{"format",track_argument_?"native-original-v3":"native-original-v2"},{"rom_crc32",OriginalRom::EXPECTED_CRC32},
        {"sram",state_.sram()},{"frame_counter",clock_},{"random_cursor",random_},{"pending_development",pending_},{"pending_search",search_},
        {"phase",phase_},{"ai",ai_},{"battle",battle_},{"ending",ending_},{"events",events_},{"pending_army",army_}};
    if(track_argument_)result["command_argument"]=persistent_argument_;
    return result;
}
std::string OriginalSession::recruit(int city,int hundreds){
    if(auto error=command_error();!error.empty())return error;
    if(!pending_.is_null()||!search_.is_null())return "请先处理当前方案或搜索报告";
    auto error=state_.recruit_reserves(city,hundreds);if(error.empty())army_=city;return error;
}
std::string OriginalSession::assign_troops(int officer,int hundreds){
    if(phase_!="player_commands"||army_.is_null())return "请先进入征兵编成";
    return state_.assign_troops(army_.get<int>(),officer,hundreds);
}
std::string OriginalSession::restore(const Json &data){
    try{
        if(!data.is_object()||(data.at("format")!="native-original-v1"&&data.at("format")!="native-original-v2"&&data.at("format")!="native-original-v3")||data.at("rom_crc32")!=OriginalRom::EXPECTED_CRC32)return "存档版本或参考游戏不匹配";
        const bool tracked=data.at("format")=="native-original-v3";
        const auto valid_argument=[](const Json &v){return v.is_number_integer()&&v>=0&&v<=255;};
        if(tracked&&(!data.contains("command_argument")||!valid_argument(data["command_argument"])))return "无效原版命令暂存";
        if(!tracked&&data.contains("command_argument"))return "旧版存档不能附带原版命令暂存";
        // Validate the immutable deployment boundary through the legacy path,
        // then replay commands (legacy field name "moves") to verify all effects.
        if(data.contains("battle")&&data["battle"].is_object()&&data["battle"].contains("tactics")){
            const auto &t=data["battle"]["tactics"];
            if(!t.is_object()||!t.at("deployment_sram").is_array()||t["deployment_sram"].size()!=8192||!t.at("moves").is_array()||t["moves"].size()>1024||!t.at("selected").is_number_integer())return "无效战术记录";
            std::vector<const Json *> segments;
            if(t.contains("history")){
                const auto &history=t["history"];
                if(!history.is_array()||history.empty()||history.size()>MAX_HISTORY_SEGMENTS)return "无效战斗历史分段";
                for(const auto &segment:history){
                    if(!segment.is_array()||segment.size()<HISTORY_SEGMENT_TARGET||segment.size()>1024)return "无效战斗历史分段";
                    segments.push_back(&segment);
                }
            }
            segments.push_back(&t["moves"]);
            auto base=data;base["sram"]=t["deployment_sram"];base["battle"].erase("tactics");
            if(tracked){
                if(!t.contains("entry_argument")||!valid_argument(t["entry_argument"]))return "无效战场初始命令暂存";
                base["command_argument"]=t["entry_argument"];
            }
            // Strategic AI retains its invasion-entry RNG while tactical events advance the live cursor.
            if(base.at("phase")=="battle")base["random_cursor"]=base.at("ai").at("random_cursor");
            auto next=*this;auto error=next.restore(base);if(!error.empty())return error;
            if(!(error=next.begin_tactics()).empty())return error;
            for(std::size_t segment=0;segment<segments.size();++segment){
            for(const auto &move:*segments[segment]){
                const auto kind=move.value("kind",std::string("move"));
                if(kind=="computer_attacker_retreat"){
                    if(!move.contains("cursor")||!move["cursor"].is_number_integer()||move["cursor"]<0||move["cursor"]>255)return "无效电脑撤军随机游标";
                    next.random_=move["cursor"].get<std::uint8_t>();error=next.resume_computer_attacker_retreat();
                }
                else if(kind=="computer_attack"){
                    for(const auto *key:{"cursor","frame"})if(!move.contains(key)||!move[key].is_number_integer()||move[key]<0||move[key]>255)return "无效电脑先攻时序";
                    next.random_=move["cursor"].get<std::uint8_t>();const auto clock=next.clock_;next.clock_=move["frame"].get<int>();
                    error=next.advance_computer_attack();next.clock_=clock;
                }
                else if(kind=="end_tactical_turn"){
                    if(!move.at("slot").is_number_integer()||move["slot"]<0||move["slot"]>(next.tactical_side()?10:11))return "无效回合交接武将";
                    error=next.select_tactical_unit(move["slot"]);if(error.empty())error=next.end_tactical_turn();
                }
                else if(kind=="prepare_tactical_strategy"){
                    for(const auto *key:{"slot","target_slot","strategy"})if(!move.contains(key)||!move[key].is_number_integer()||move[key]<0||move[key]>11)return "无效玩家施计选择";
                    error=next.prepare_tactical_strategy(move["slot"],move["target_slot"],move["strategy"]);
                }
                else if(kind=="cancel_tactical_strategy")error=next.cancel_tactical_strategy();
                else if(kind=="confirm_tactical_strategy"){
                    for(const auto *key:{"cursor","frame"})if(!move.contains(key)||!move[key].is_number_integer()||move[key]<0||move[key]>255)return "无效玩家施计时序";
                    next.random_=move["cursor"].get<std::uint8_t>();const auto clock=next.clock_;next.clock_=move["frame"].get<int>();
                    error=next.confirm_tactical_strategy();next.clock_=clock;
                }
                else if(kind=="finish_tactical_strategy")error=next.finish_tactical_strategy();
                else if(kind=="computer_motion"||kind=="computer_nearby"){
                    if(!move.contains("frame")||!move["frame"].is_number_integer()||move["frame"]<0||move["frame"]>255)return "无效电脑移动交战时序";
                    const auto clock=next.clock_;next.clock_=move["frame"].get<int>();error=kind=="computer_nearby"?next.continue_computer_nearby():next.continue_computer_motion();next.clock_=clock;
                }
                else if(kind=="computer_occupied_fort"||kind=="computer_occupied_fort_resume"){
                    for(const auto *key:{"cursor","frame"})if(!move.contains(key)||!move[key].is_number_integer()||move[key]<0||move[key]>255)return "无效占堡应对时序";
                    next.random_=move["cursor"].get<std::uint8_t>();const auto clock=next.clock_;next.clock_=move["frame"].get<int>();
                    error=next.continue_computer_occupied_fort(kind=="computer_occupied_fort_resume");next.clock_=clock;
                }
                else if(kind=="computer_flank")error=next.continue_computer_flank();
                else if(kind=="computer_scan")error=next.continue_computer_scan();
                else if(kind=="computer_exhausted_attack")error=next.finish_exhausted_computer_attack();
                else if(kind=="computer_reuse")error=next.plan_computer_tactics(true);
                else if(kind=="computer_strategy_retry"){
                    if(!move.contains("cursor")||!move["cursor"].is_number_integer()||move["cursor"]<0||move["cursor"]>255)return "无效补充计策随机游标";
                    next.random_=move["cursor"].get<std::uint8_t>();error=next.retry_computer_strategy();
                }
                else if(kind=="computer_role_return")error=next.continue_computer_role();
                else if(kind=="computer_role_after_handover")error=next.resume_computer_role_after_handover();
                else if(kind=="computer_empty_role_return")error=next.continue_empty_computer_role();
                else if(kind=="computer_fort")error=next.plan_computer_tactics();
                else if(kind=="execute_computer_strategy"){
                    for(const auto *key:{"cursor","frame"})if(!move.contains(key)||!move[key].is_number_integer()||move[key]<0||move[key]>255)return "无效电脑计策执行时序";
                    next.random_=move["cursor"].get<std::uint8_t>();const auto clock=next.clock_;next.clock_=move["frame"].get<int>();
                    error=next.execute_computer_strategy();next.clock_=clock;
                }
                else if(kind=="finish_computer_strategy")error=next.finish_computer_strategy();
                else if(kind=="computer_strategy"){
                    if(!move.contains("cursor")||!move["cursor"].is_number_integer()||move["cursor"]<0||move["cursor"]>255)return "无效电脑计策随机游标";
                    next.random_=move["cursor"].get<std::uint8_t>();error=next.evaluate_computer_strategy();
                }
                else if(kind=="computer_tactics"){
                    if(!move.contains("cursor")||!move["cursor"].is_number_integer()||move["cursor"]<0||move["cursor"]>255)return "无效电脑战术随机游标";
                    next.random_=move["cursor"].get<int>();error=next.advance_computer_tactics();
                }
                else if(kind=="advance_time_limit_result")error=next.advance_time_limit_result();
                else if(kind=="begin_human_failure")error=next.begin_human_failure();
                else if(kind=="finish_human_failure")error=next.finish_human_failure();
                else if(kind=="begin_ruler_result")error=next.begin_ruler_defeat_result();
                else if(kind=="advance_ruler_annexation"){
                    if(!move.contains("cursor")||!move["cursor"].is_number_integer()||move["cursor"]<0||move["cursor"]>255)return "无效领地结算随机游标";
                    next.random_=move["cursor"].get<int>();error=next.advance_ruler_annexation();
                }
                else if(kind=="begin_commander_result")error=next.begin_commander_defeat_result();
                else if(kind=="begin_defeat_result")error=next.begin_defender_defeat_result();
                else if(kind=="advance_withdrawal_result"){
                    if(!move.at("cursor").is_number_integer()||move["cursor"]<0||move["cursor"]>255)return "无效战后随机状态";
                    next.random_=move["cursor"].get<std::uint8_t>();error=next.advance_withdrawal_result();
                }
                else if(kind=="begin_retreat"){
                    if(!move.at("slot").is_number_integer()||move["slot"]<0||move["slot"]>(next.tactical_side()?10:11))return "无效撤退部队";
                    error=next.begin_tactical_retreat(move["slot"]);
                }
                else if(kind=="cancel_retreat")error=next.cancel_tactical_retreat();
                else if(kind=="confirm_retreat"){
                    if(!move.at("cursor").is_number_integer()||move["cursor"]<0||move["cursor"]>255)return "无效撤退随机状态";
                    next.random_=move["cursor"].get<std::uint8_t>();error=next.confirm_tactical_retreat();
                }
                else if(kind=="finish_retreat")error=next.finish_tactical_retreat();
                else if(kind=="close_scout")error=next.close_tactical_scout();
                else if(kind=="scout"){
                    if(!move.at("slot").is_number_integer()||move["slot"]<0||move["slot"]>10||!move.at("target_slot").is_number_integer()||move["target_slot"]<0||move["target_slot"]>11)return "无效侦察步骤";
                    error=next.scout_tactical(move["slot"],move["target_slot"]);
                }
                else if(kind=="begin_clash")error=next.begin_clash();
                else if(kind=="advance_clash"){
                    if(!move.at("cursor").is_number_integer()||move["cursor"]<0||move["cursor"]>255)return "无效交锋随机状态";
                    next.random_=move["cursor"].get<std::uint8_t>();error=next.advance_clash();
                }
                else if(kind=="resume_clash_strategy"||kind=="recover_clash_strategy"){
                    if(!move.at("cursor").is_number_integer()||move["cursor"]<0||move["cursor"]>255)return "无效电脑军令随机状态";
                    next.random_=move["cursor"].get<std::uint8_t>();error=kind=="resume_clash_strategy"?next.resume_clash_strategy():next.recover_clash_strategy();
                }
                else if(kind=="advance_clash_retreat"){
                    if(!move.at("cursor").is_number_integer()||move["cursor"]<0||move["cursor"]>255)return "无效交锋撤离随机状态";
                    next.random_=move["cursor"].get<std::uint8_t>();error=next.advance_clash_retreat();
                }
                else if(kind=="advance_clash_defeat")error=next.advance_clash_defeat();
                else if(kind=="begin_duel")error=next.begin_duel();
                else if(kind=="duel_command"){
                    if(!move.at("command").is_number_integer()||move["command"]<0||move["command"]>4||!move.at("cursor").is_number_integer()||move["cursor"]<0||move["cursor"]>255)return "无效单挑指令记录";
                    next.random_=move["cursor"].get<std::uint8_t>();error=next.choose_duel_command(move["command"]);
                }
                else if(kind=="answer_duel_surrender"){
                    if(!move.at("accept").is_boolean())return "无效单挑投降确认";
                    error=next.answer_duel_surrender(move["accept"]);
                }
                else if(kind=="advance_duel"){
                    if(!move.at("cursor").is_number_integer()||move["cursor"]<0||move["cursor"]>255||!move.at("frame").is_number_integer()||move["frame"]<0||move["frame"]>255)return "无效单挑推进记录";
                    next.random_=move["cursor"].get<std::uint8_t>();const auto clock=next.clock_;next.clock_=move["frame"].get<int>();
                    error=next.advance_duel();next.clock_=clock;
                }
                else if(kind=="request_surrender")error=next.request_clash_surrender();
                else if(kind=="answer_surrender"){
                    if(!move.at("accept").is_boolean())return "无效投降确认";
                    error=next.answer_clash_surrender(move["accept"]);
                }
                else if(kind=="advance_surrender"){
                    if(!move.at("cursor").is_number_integer()||move["cursor"]<0||move["cursor"]>255)return "无效投降随机状态";
                    next.random_=move["cursor"].get<std::uint8_t>();error=next.advance_clash_surrender();
                }
                else if(kind=="finish_clash_result")error=next.finish_clash_result();
                else if(kind=="clash_order"){
                    if(!move.at("side").is_number_integer()||move["side"]<0||move["side"]>1||!move.at("unit_kind").is_number_integer()||move["unit_kind"]<0||move["unit_kind"]>3)return "无效交战军令步骤";
                    error=next.cycle_clash_order(move["side"],move["unit_kind"]);
                }
                else if(kind=="cancel_attack")error=next.cancel_tactical_attack();
                else if(kind=="confirm_attack"){
                    if(!move.at("frame").is_number_integer()||move["frame"]<0||move["frame"]>255)return "无效交战帧计数";
                    const auto clock=next.clock_;next.clock_=move["frame"].get<int>();
                    error=next.confirm_tactical_attack();next.clock_=clock;
                }else{
                    if(!move.at("slot").is_number_integer()||!move.at("direction").is_number_integer()||move["slot"]<0||move["slot"]>(next.tactical_side()?10:11)||move["direction"]<0||move["direction"]>3)return "无效战术步骤";
                    if(kind=="formation")error=next.adjust_tactical_formation(move["slot"],move["direction"]);
                    else if(kind=="attack")error=next.attack_tactical(move["slot"],move["direction"]);
                    else if(kind=="move")error=next.move_tactical(move["slot"],move["direction"]);
                    else return "未知战术步骤";
                }
                if(!error.empty())return error;
            }
            if(segment+1<segments.size()&&!(error=next.archive_battle_history()).empty())return error;
            }
            if(t["selected"]<0||t["selected"]>(next.tactical_side()?10:11))return "无效战术武将";
            const bool clash_result=t.contains("attack")&&t["attack"]["stage"]=="clash_result";
            if(!t.contains("retreat")&&!t.contains("human_strategy")&&!clash_result&&!t.contains("turn_boundary")&&!(error=next.select_tactical_unit(t["selected"])).empty())return "无效战术武将";
            if(next.save()!=data)return "战术位置、机动力或行动记录不一致";
            *this=std::move(next);return {};
        }
        auto integer=[](const Json &v,int max){if(!v.is_number_integer())throw std::runtime_error("integer");auto n=v.get<std::int64_t>();if(n<0||n>max)throw std::runtime_error("range");return int(n);};
        const auto &raw=data.at("sram");if(!raw.is_array()||raw.size()!=8192)return "存档状态长度错误";
        std::vector<std::uint8_t> bytes;for(const auto &b:raw)bytes.push_back(integer(b,255));
        const int clock=integer(data.at("frame_counter"),65535),random=integer(data.at("random_cursor"),255);
        if(bytes[0xd88]>2||bytes[0xd87]<1||bytes[0xd87]>12||bytes[0xd8b]>5||(bytes[0xd89]&7)>5||(bytes[0xd8a]&128 && (bytes[0xd8a]&7)>5))return "无效日期、难度或君主";
        for(int c=0;c<30;++c)for(int i=0;i<12;++i){int id=bytes[c*36+16+i];if(id!=255&&id>=241)return "无效驻城武将";}
        for(int r=0;r<6;++r)if(bytes[0xd8f+r*4]>15)return "无效命令书数量";
        OriginalState candidate(rom_,std::move(bytes));auto pending=data.at("pending_development");
        if(!pending.is_null()){
            int city=integer(pending.at("city"),29),officer=integer(pending.at("officer"),240),kind=integer(pending.at("kind"),2),proposal=integer(pending.at("proposal"),17);
            const auto offers=candidate.development_options(city,officer,kind);bool valid=false;
            for(const auto &offer:offers)if(offer["index"]==proposal && offer["cost"]==pending.at("cost") && offer["dialogue_id"]==pending.at("dialogue_id"))valid=true;
            if(!valid)return "无效开发方案";
        }
        auto search=data.value("pending_search",Json(nullptr));
        if(!search.is_null()) {
            if(!pending.is_null())return "存档包含冲突的待处理指令";
            const int city=integer(search.at("city"),29),officer=integer(search.at("officer"),240),kind=integer(search.at("kind"),9);
            integer(search.at("dialogue_id"),255);integer(search.at("value"),65535);
            if((candidate.sram()[city*36]&7)!=candidate.sram()[0xd8b] || officer<6)return "无效搜索报告";
            if(kind>=6 && kind<=8) {
                const int id=integer(search.at("candidate"),240);
                if(id<6 || search["value"].get<int>()>255)return "无效搜索招募";
                for(int c=0;c<30;++c)for(int s=0;s<12;++s)if(candidate.sram()[c*36+16+s]==id)return "搜索招募武将已驻城";
            }else if(!search.at("candidate").is_null())return "无效搜索对象";
        }
        std::string phase="player_commands";Json ai=nullptr,battle=nullptr,ending=nullptr,events=Json::array();
        if(data.at("format")!="native-original-v1"){
            phase=data.at("phase").get<std::string>();ai=data.at("ai");battle=data.at("battle");ending=data.at("ending");events=data.at("events");
            if(phase!="player_commands"&&phase!="ai_turn"&&phase!="battle"&&phase!="expedition"&&phase!="ending")return "无效回合阶段";
            if(!events.is_array()||events.size()>64)return "无效回合记录";
            for(const auto &event:events){
                const auto kind=event.at("kind").get<std::string>();
                if(kind=="ai"){
                    integer(event.at("ruler"),5);integer(event.at("battle_result"),2);
                    integer(event.at("source"),255);integer(event.at("target"),255);
                }else if(kind=="month"){
                    integer(event.at("year"),65535);if(integer(event.at("month"),12)<1)return "无效回合月份";
                }else return "无效回合记录类型";
            }
            if(phase=="ai_turn"||phase=="battle"){
                if(candidate.human_turn()||!ai.is_object())return "无效电脑回合";
                integer(ai.at("phase"),2);integer(ai.at("budget"),255);integer(ai.at("plan"),255);integer(ai.at("round"),2);
                if(integer(ai.at("random_cursor"),255)!=random)return "随机状态不一致";
                if(!ai.at("done").is_boolean())return "无效电脑执行状态";
                if(ai.contains("a1"))integer(ai["a1"],255);
                if(ai.contains("work_gold"))integer(ai["work_gold"],0xffffff);
                if(phase=="ai_turn"){
                    if(ai["done"].get<bool>()||ai["phase"]==2||!battle.is_null())return "电脑回合已经结束";
                }else{
                    auto battle_ai=battle;if(battle_ai.is_object())battle_ai.erase("deployment");
                    if(battle_ai!=ai||!ai["done"].get<bool>()||ai["phase"]!=2||ai.at("battle_result")!=3)return "无效待处理战斗";
                    const int source=integer(ai.at("source"),29),target=integer(ai.at("target"),29);
                    const auto &s=candidate.sram();const int owner=s[target*36]&7;
                    if(source==target||(s[source*36]&7)!=s[0xd8b]||
                       !(owner==(s[0xd89]&7)||((s[0xd8a]&128)&&owner==(s[0xd8a]&7)))||
                       s[0xdaa+57]!=((s[0xd8b]<<4)|owner))return "战斗双方与城池不一致";
                    for(int off=0;off<57;off+=(off<24?2:3))if(s[0xdaa+off]!=255&&s[0xdaa+off]>=241)return "无效战斗武将";
                    if(battle.contains("deployment")){
                        const auto &deployment=battle.at("deployment");
                        if(deployment.at("side")!="defender"||deployment.value("two_players",false)||deployment.value("handover",false)||!deployment.at("current").is_number_integer())return "无效守城布阵进度";
                        const auto cursor=deployment.at("current").get<std::int64_t>();
                        const auto &raw_attackers=deployment.at("attackers");
                        if(!raw_attackers.is_array()||raw_attackers.size()!=33||cursor<-1||cursor>11)return "无效守城布阵记录";
                        std::vector<std::uint8_t> attackers;for(const auto &b:raw_attackers)attackers.push_back(integer(b,255));
                        if(!candidate.valid_defense_deployment(source,target,static_cast<int>(cursor),attackers))return "守城布阵、进攻名单或地形不一致";
                    }

                }
            }else if(phase=="expedition"){
                if(!candidate.human_turn()||!ai.is_null()||!battle.is_object()||battle.at("kind")!="player_expedition")return "无效玩家出征阶段";
                const int source=integer(battle.at("source"),29),target=integer(battle.at("target"),29),leader=integer(battle.at("leader"),240);
                const auto &s=candidate.sram();
                if(source==target||(s[source*36]&7)!=s[0xd8b]||(s[target*36]&7)==s[0xd8b]||s[0xdaa+57]!=((s[0xd8b]<<4)|(s[target*36]&7))||s[target*36+32]!=source||s[target*36+33]!=s[0xd8b])return "出征双方与城池不一致";
                if(!battle.at("officers").is_array()||battle["officers"].empty()||battle["officers"].size()>11)return "无效出征名单";
                std::set<int> selected,ledger;int cost=0,leaders=0;
                for(const auto &v:battle["officers"]){int id=integer(v,240);if(!selected.insert(id).second)return "重复出征武将";cost+=(s[0x438+id*8+6]&15)*20;}
                for(int i=0;i<11;++i){const int b=0xdaa+24+i*3,id=s[b];if(id==255)continue;if(id>=241||!ledger.insert(id).second||(s[b+2]&127)!=source||((s[b+2]&128)!=0)!=(id==leader))return "无效出征战斗记录";leaders+=id==leader;}
                if(ledger!=selected||leaders!=1||integer(battle.at("cost"),3300)!=cost)return "出征名单与战斗记录不一致";
                integer(battle.at("reference_adviser"),255);
                Json defenders=Json::array(),residents=Json::array();std::set<int> seen;
                for(int i=0;i<12;++i){
                    const int id=s[target*36+16+i];
                    if(id!=255&&seen.insert(id).second)residents.push_back(id);
                    if(!battle.contains("deployment")&&s[0xdaa+i*2+1]!=255)return "守军尚未进入布阵阶段";
                    if(s[0xdaa+i*2]!=255)defenders.push_back(s[0xdaa+i*2]);
                }
                if(battle.contains("defenders")){
                    if(!battle["defenders"].is_array()||battle["defenders"]!=defenders||(!battle.contains("deployment")&&defenders!=residents))return "守军与驻城名单不一致";
                }else if(!defenders.empty())return "旧版出征记录包含未知守军";
                if(battle.contains("deployment")){
                    const auto &deployment=battle["deployment"];
                    if(!deployment.is_object()||!deployment.at("current").is_number_integer())return "无效布阵进度";
                    const auto cursor=deployment.at("current").get<std::int64_t>();
                    const int owner=s[target*36]&7;
                    const bool two_players=owner==(s[0xd89]&7)||((s[0xd8a]&128)&&owner==(s[0xd8a]&7));
                    if(!battle.contains("defenders"))return "缺少守军名单";
                    if(two_players){
                        if(deployment.at("two_players")!=true||!deployment.at("handover").is_boolean())return "无效双人交接记录";
                        const auto side=deployment.at("side").get<std::string>();const bool defending=side=="defender";
                        if((side!="defender"&&side!="attacker")||cursor<-1||cursor>=(defending?12:11)||
                           !candidate.valid_pvp_deployment(source,target,static_cast<int>(cursor),defending,deployment["handover"]))return "双人布阵位置、顺序或交接状态不一致";
                    }else if(deployment.value("two_players",false)||deployment.value("handover",false)||
                             deployment.value("side",std::string("attacker"))!="attacker"||cursor<-1||cursor>10||
                             !candidate.valid_deployment(source,target,static_cast<int>(cursor)))return "布阵位置、地形或进度不一致";
                }else for(int i=0;i<11;++i)if(s[0xdc3+i*3]!=255)return "未布阵的出征包含部署位置";
                for(int c=0;c<30;++c)for(int slot=0;slot<12;++slot)if(selected.count(s[c*36+16+slot]))return "出征武将仍驻城";
            }else if(!ai.is_null()||!battle.is_null())return "非电脑阶段包含未处理战斗";
        }
        if(phase=="player_commands"&&!candidate.human_turn())return "当前君主不是玩家";
        if(phase!="player_commands"&&(!pending.is_null()||!search.is_null()))return "当前阶段不能保留玩家指令";
        if(phase=="ending"){
            if(ending.is_null()||ending!=candidate.ending())return "结局与战局不符";
        }else if(!ending.is_null())return "未结束的战局包含结局";
        auto army=data.value("pending_army",Json(nullptr));
        if(!army.is_null()){
            const int city=integer(army,29);
            if(phase!="player_commands"||!pending.is_null()||!search.is_null()||(candidate.sram()[city*36]&7)!=candidate.sram()[0xd8b])return "无效征兵编成状态";
        }
        computer_scratch21_=-1;computer_argument_=-1;handover_argument_=-1;
        track_argument_=tracked;persistent_argument_=tracked?data["command_argument"].get<int>():-1;
        state_=std::move(candidate);pending_=std::move(pending);search_=std::move(search);clock_=clock;random_=random;army_=std::move(army);
        phase_=phase;ai_=std::move(ai);battle_=std::move(battle);ending_=std::move(ending);events_=std::move(events);return {};
    }catch(const std::exception&){return "存档内容不完整或已损坏";}
}
}
