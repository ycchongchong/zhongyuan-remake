#include "zhongyuan/original_session.hpp"

namespace zhongyuan {
std::string OriginalSession::begin_duel(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack"))return "当前没有单挑";
    const auto &a=battle_["tactics"]["attack"];
    if(a["stage"]!="clash_boundary"||a.value("boundary",std::string{})!="duel")return "尚未进入单挑";
    const auto &r=a["clash"]["runtime"];
    if(r["global_phase"]!=13||(r["active"]!=0&&r["active"]!=128)||r["units"][0]==255||r["units"][33]==255)return "无效单挑入口";
    if(battle_["tactics"]["moves"].size()>1016)return "当前行动记录已满，请保存进度";
    auto next=*this;auto &t=next.battle_["tactics"];auto &attack=t["attack"];
    attack["duel"]={{"side",r["active"]},{"flags",0},{"last_damage",0},{"round",0}};
    attack.erase("boundary");attack["stage"]="duel_orders";attack["clash"]["runtime"]["global_phase"]=14;
    next.random_=0; // E572 scene-load reset; presentation frame sampling is separate.
    t["moves"].push_back({{"kind","begin_duel"}});*this=std::move(next);return {};
}
// Called only on a candidate session after validating the public entry point.
std::string OriginalSession::apply_duel_command(int command){
    auto &a=battle_["tactics"]["attack"];auto &duel=a["duel"];auto &clash=a["clash"];
    const int side=duel["side"],officer=clash[side?"second":"first"];
    if(command<0||command>4||(command==4&&officer<6))return "无效单挑指令；君主不能投降";
    duel["command"]=command;
    if(command<3){
        auto context=clash;context.update(duel);
        auto result=state_.duel_strike(clash["runtime"]["units"],context,random_);
        if(result.contains("error"))return result["error"];
        duel["flags"]=result["flags"];duel["last_damage"]=result["last_damage"];duel["exchange"]=std::move(result);
        a["stage"]="duel_exchange";
    }else if(command==3)a["stage"]="duel_retreat";
    else a["stage"]=clash["players"][side?1:0]!=0?"duel_surrender_confirm":"duel_surrender_notice";
    return {};
}
std::string OriginalSession::choose_duel_command(int command){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack"))return "当前没有单挑指令";
    const auto &a=battle_["tactics"]["attack"];
    if(a["stage"]!="duel_orders")return "请先确认当前单挑结果";
    const int side=a["duel"]["side"];
    if(a["clash"]["players"][side?1:0]==0)return "当前由电脑选择单挑指令";
    if(battle_["tactics"]["moves"].size()>1018)return "当前行动记录已满，请保存进度";
    auto next=*this;const int cursor=next.random_;const auto error=next.apply_duel_command(command);
    if(!error.empty())return error;
    next.battle_["tactics"]["moves"].push_back({{"kind","duel_command"},{"command",command},{"cursor",cursor}});*this=std::move(next);return {};
}
std::string OriginalSession::answer_duel_surrender(bool accept){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack")||battle_["tactics"]["attack"]["stage"]!="duel_surrender_confirm")return "当前没有单挑投降确认";
    if(battle_["tactics"]["moves"].size()>=1024)return "当前行动记录已满";
    battle_["tactics"]["attack"]["stage"]=accept?"duel_surrender_notice":"duel_orders";
    battle_["tactics"]["moves"].push_back({{"kind","answer_duel_surrender"},{"accept",accept}});return {};
}
std::string OriginalSession::advance_duel(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("attack"))return "当前没有单挑";
    const auto stage=battle_["tactics"]["attack"]["stage"];
    if(stage!="duel_orders"&&stage!="duel_exchange"&&stage!="duel_defeat"&&stage!="duel_retreat"&&stage!="duel_surrender_notice"&&stage!="duel_surrender_accepted")return "请先处理当前单挑选择";
    if(battle_["tactics"]["moves"].size()>=1024||(stage=="duel_orders"&&battle_["tactics"]["moves"].size()>1018))return "当前行动记录已满，请保存进度";
    auto next=*this;auto &t=next.battle_["tactics"];auto &a=t["attack"];auto &clash=a["clash"];auto &runtime=clash["runtime"];auto &duel=a["duel"];
    const int side=duel["side"],cursor=next.random_,frame=static_cast<std::uint8_t>(next.clock_);
    if(stage=="duel_orders"){
        if(clash["players"][side?1:0]!=0)return "请选择本方单挑指令";
        auto context=clash;context.update({{"side",side},{"frame",frame}});
        const auto choice=next.state_.duel_ai(runtime["units"],context,next.random_);if(choice.contains("error"))return choice["error"];
        const auto error=next.apply_duel_command(choice["command"]);if(!error.empty())return error;
    }else if(stage=="duel_exchange"){
        duel["side"]=side^128;duel["round"]=duel["round"].get<int>()+1;
        auto &u=runtime["units"];
        if(u[1]==0||u[34]==0){
            const bool left=u[1]==0;u[left?0:33]=255;u[left?1:34]=255;
            duel["side"]=left?128:0;duel["flags"]=left?1:0;duel["officer"]=clash[left?"first":"second"];
            a["stage"]="duel_defeat";
        }else a["stage"]="duel_orders";
    }else if(stage=="duel_retreat"){
        // 9233 -> global scene 9, then 9AC0 resumes existing unit records.
        runtime["orders"][side?7:3]=1;
        runtime.update({{"global_phase",10},{"phase",1},{"counter",15},{"sequential",1},{"direction",139}});
        runtime["countdown"]=((clash["tactical_side"].get<int>()?clash["human_mask"].get<int>()>>4:clash["human_mask"].get<int>())&15)?7:8;
        duel["closed"]="retreat";a["duel_return"]=true;a["stage"]="clash_orders";next.random_=0;
    }else if(stage=="duel_surrender_notice")a["stage"]="duel_surrender_accepted";
    else{
        const bool surrendered=stage=="duel_surrender_accepted";
        auto context=clash;context.update({{"kind",surrendered?"surrender":"defeat"},{"side",side},{"target",next.battle_["target"]}});
        auto captives=t.value("captives",Json(std::vector<int>(24,255)));
        auto result=next.state_.duel_finish(runtime["units"],captives,context,next.random_);if(result.contains("error"))return result["error"];
        if(surrendered){t["captives"]=std::move(captives);runtime["active"]=side;}
        const bool restored_map=result["phase"]==11;const int officer=result["officer"];
        if(restored_map){const auto error=next.state_.restore_tactical_board(next.battle_["target"]);if(!error.empty())return error;}
        int selected=-1,defenders=0;bool commander=false;const auto &s=next.state_.sram();
        for(int i=0;i<11;++i)if(s[0xdc2+i*3]!=255){if(selected<0)selected=i;commander|=(s[0xdc4+i*3]&128)!=0;}
        for(int i=0;i<12;++i)defenders+=s[0xdaa+i*2]!=255;
        if(selected>=0){const int next_selected=next.first_tactical_slot();if(next_selected>=0)t["selected"]=next_selected;}
        result.update({{"kind","duel"},{"outcome",surrendered?"surrender":"defeat"},{"map_restored",restored_map},
            {"can_continue",restored_map&&selected>=0&&defenders>0&&commander&&officer>=6}});
        runtime["global_phase"]=result["phase"];a["result"]=std::move(result);a["stage"]="clash_result";
    }
    t["moves"].push_back({{"kind","advance_duel"},{"cursor",cursor},{"frame",frame}});*this=std::move(next);return {};
}
}
