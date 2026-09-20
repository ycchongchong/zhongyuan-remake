#include "zhongyuan/original_session.hpp"
namespace zhongyuan {
std::string OriginalSession::advance_time_limit_result(){
    if((phase_!="expedition"&&phase_!="battle")||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有限时战果";
    auto &t=battle_["tactics"];
    if(t.value("turn_boundary",std::string{})!="time_limit"||t.value("round",0)!=11||tactical_side()!=0||t["points"]!=0||
       t.contains("attack")||t.contains("retreat")||t.contains("scout"))return "尚未到达战斗限时结果";
    if(t["moves"].size()>=1024)return "当前行动记录已满";
    if(!t.contains("time_limit")){
        // Reserve all three notices and the complete forced-withdrawal path
        // before starting: no save may be stranded halfway through a result.
        int needed=6;for(int i=0;i<11;++i)needed+=state_.sram()[0xdc2+i*3]!=255;
        if(t.contains("captives"))for(const auto &id:t["captives"])needed+=id!=255;
        if(t["moves"].size()+needed>1024)return "剩余记录空间不足以完成限时结算";
    }
    const auto &s=state_.sram();
    auto player=[&](int faction){return faction==(s[0xd89]&7)?1:((s[0xd8a]&128)&&faction==(s[0xd8a]&7)?2:0);};
    const auto previous=t.value("time_limit",Json{{"stage",0},{"speaker",255}});
    auto result=state_.time_limit_step(previous["stage"],(player(s[0xde3]>>4)<<4)|player(s[0xde3]&7),previous["speaker"]);
    if(result.contains("error"))return result["error"];
    if(result["phase"]==13){t["turn_boundary"]="battle_result";t["turn_reason"]=36;}
    t["time_limit"]=std::move(result);t["moves"].push_back({{"kind","advance_time_limit_result"}});return {};
}
int OriginalSession::pending_clash_result_reason() const {
    if((phase_!="expedition"&&phase_!="battle")||!battle_.is_object()||!battle_.contains("tactics"))return 0;
    const auto &t=battle_["tactics"];
    if(!t.contains("attack")||t["attack"]["stage"]!="clash_result"||t["attack"]["result"]["can_continue"]==true||t["attack"]["result"].value("phase",0)!=11)return 0;
    Json context={{"side",tactical_side()},{"points",0},{"round",t.value("round",0)},{"carry",t.value("carry",0)},
        {"phase",5},{"counter",1},{"reason",0},{"status",t.value("status",Json(std::vector<int>(24,0)))}};
    const auto result=state_.tactical_handover(context);
    return !result.contains("error")&&result["phase"]==14?result["reason"].get<int>():0;
}
bool OriginalSession::can_settle_defender_defeat() const {return pending_clash_result_reason()==3;}
bool OriginalSession::can_settle_commander_defeat() const {return pending_clash_result_reason()==20;}
bool OriginalSession::can_settle_ruler_defeat() const {
    const int reason=pending_clash_result_reason();if(reason!=131&&reason!=148)return false;
    const auto &s=state_.sram();const int loser=reason==131?s[0xde3]&7:s[0xde3]>>4;
    return loser!=(s[0xd89]&7)&&!((s[0xd8a]&128)&&loser==(s[0xd8a]&7));
}
bool OriginalSession::can_report_human_failure() const {
    if((phase_!="expedition"&&phase_!="battle")||!battle_.is_object()||!battle_.contains("tactics"))return false;
    const auto &t=battle_["tactics"];
    if(t.contains("human_failure")||!t.contains("attack")||t["attack"]["stage"]!="clash_result")return false;
    const auto &result=t["attack"]["result"];if(result.value("phase",0)!=15)return false;
    const int loser=result.value("officer",255),factions=state_.sram()[0xde3];
    if(loser!=(factions&7)&&loser!=(factions>>4))return false;
    return !state_.human_failure_step(loser,loser==(factions&7)?factions>>4:factions&7,0).contains("error");
}
std::string OriginalSession::begin_human_failure(){
    if(!can_report_human_failure())return "当前没有玩家君主失败报告";
    auto &t=battle_["tactics"];const int loser=t["attack"]["result"]["officer"],factions=state_.sram()[0xde3];
    auto result=state_.human_failure_step(loser,loser==(factions&7)?factions>>4:factions&7,0);
    int needed=1;
    if(result["can_continue"]){
        needed+=1+30+(loser==(factions&7)?5:4);
        for(int i=0;i<11;++i)needed+=state_.sram()[0xdc2+i*3]!=255;
        if(t.contains("captives"))for(const auto &id:t["captives"])needed+=id!=255;
    }
    if(t["moves"].size()+needed>1024)return "剩余记录空间不足以完成君主失败结算";
    t["human_failure"]=std::move(result);t["moves"].push_back({{"kind","begin_human_failure"}});return {};
}
std::string OriginalSession::finish_human_failure(){
    if((phase_!="expedition"&&phase_!="battle")||!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("human_failure"))return "请先查看玩家君主失败报告";
    const auto &old=battle_["tactics"];const auto &report=old["human_failure"];
    auto result=state_.human_failure_step(report["loser"],report["winner"],3);
    if(result.contains("error"))return result["error"];
    if(result["phase"]!=11)return "所有玩家均已败北，战局结束";
    if(old["moves"].size()>=1024)return "当前行动记录已满";
    auto next=*this;auto &t=next.battle_["tactics"];
    if(auto error=next.state_.restore_tactical_board(next.battle_["target"]);!error.empty())return error;
    Json context={{"side",next.tactical_side()},{"points",0},{"round",t.value("round",0)},{"carry",t.value("carry",0)},
        {"phase",5},{"counter",1},{"reason",0},{"status",t.value("status",Json(std::vector<int>(24,0)))}};
    const auto boundary=next.state_.tactical_handover(context);
    if(boundary.contains("error")||boundary["phase"]!=14||(boundary["reason"]!=131&&boundary["reason"]!=148))return "无效君主战后结算阶段";
    t["last_clash"]=t["attack"]["result"];t.erase("attack");t.erase("human_failure");
    t["last_human_failure"]=std::move(result);t["turn_boundary"]="battle_result";t["turn_reason"]=boundary["reason"];
    t["moves"].push_back({{"kind","finish_human_failure"}});*this=std::move(next);return {};
}
std::string OriginalSession::begin_ruler_defeat_result(){
    if(!can_settle_ruler_defeat())return "当前不是可结算的电脑君主败北战果";
    const int reason=pending_clash_result_reason();auto &t=battle_["tactics"];
    if(t["moves"].size()>=1024)return "当前行动记录已满";
    t["last_clash"]=t["attack"]["result"];t.erase("attack");t["turn_boundary"]="battle_result";t["turn_reason"]=reason;
    t["moves"].push_back({{"kind","begin_ruler_result"}});return {};
}
std::string OriginalSession::advance_ruler_annexation(){
    if((phase_!="expedition"&&phase_!="battle")||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有待接收的败方领地";
    const auto &old=battle_["tactics"];const int reason=old.value("turn_reason",0);
    if(old.value("turn_boundary",std::string{})!="battle_result"||(reason!=131&&reason!=148)||!old.contains("settlement")||
       old["settlement"]["stage"]!=(reason==131?29:4)||has_battle_captives())return "请先完成战场军团、俘虏与城池结算";
    const int city=old.contains("annexation")?old["annexation"]["next_city"].get<int>():0;
    if(city>=30)return "败方领地已处理，请确认返回战略地图";
    if(old["moves"].size()+30-city>1024)return "剩余记录空间不足以完成领地接收";
    auto next=*this;auto &t=next.battle_["tactics"];const auto factions=next.state_.sram()[0xde3];
    const int loser=reason==131?factions&7:factions>>4,winner=reason==131?factions>>4:factions&7,cursor=next.random_;
    const auto result=next.state_.annex_ruler_city(city,loser,winner,next.random_);if(result.contains("error"))return result["error"];
    if(!t.contains("annexation"))t["annexation"]={{"loser",loser},{"winner",winner},{"outcomes",Json::array()}};
    t["annexation"]["next_city"]=result["next_city"];
    if(result["changed"])t["annexation"]["outcomes"].push_back(result);
    t["moves"].push_back({{"kind","advance_ruler_annexation"},{"cursor",cursor}});*this=std::move(next);return {};
}
std::string OriginalSession::begin_commander_defeat_result(){
    if(!can_settle_commander_defeat())return "当前不是可结算的进攻主将离场战果";
    auto &t=battle_["tactics"];if(t["moves"].size()>=1024)return "当前行动记录已满";
    t["last_clash"]=t["attack"]["result"];t.erase("attack");t["turn_boundary"]="battle_result";t["turn_reason"]=20;
    t["moves"].push_back({{"kind","begin_commander_result"}});return {};
}
std::string OriginalSession::begin_defender_defeat_result(){
    if(!can_settle_defender_defeat())return "当前不是可结算的守军全灭战果";
    auto &t=battle_["tactics"];if(t["moves"].size()>=1024)return "当前行动记录已满";
    t["last_clash"]=t["attack"]["result"];t.erase("attack");t["turn_boundary"]="battle_result";t["turn_reason"]=3;
    t["moves"].push_back({{"kind","begin_defeat_result"}});return {};
}
std::string OriginalSession::advance_withdrawal_result(){
    if((phase_!="expedition"&&phase_!="battle")||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有撤离战果";
    const auto &old=battle_["tactics"];
    const int reason=old.value("turn_reason",0),branch=reason&127,start=branch==20?0:branch==36?14:branch==3?24:9,finish=branch==20?4:branch==36?17:branch==3?29:13;
    if(old.value("turn_boundary",std::string{})!="battle_result"||(reason!=2&&reason!=3&&reason!=20&&reason!=36&&reason!=131&&reason!=148)||old.contains("attack")||old.contains("retreat")||old.contains("scout"))return "请先完成已支持的战果确认";
    if(old.contains("settlement")&&old["settlement"]["stage"]==finish)return "战果已结算，请确认返回战略地图";
    if(old["moves"].size()>=1024)return "当前行动记录已满";
    if(!old.contains("settlement")){
        // Reserve every remaining officer and empty-list transition before any
        // transfer, so the action log cannot strand a half-settled battle.
        int needed=(branch==36?3:branch==3?5:4)+((reason&128)?30:0);for(int i=0;i<11;++i)needed+=state_.sram()[0xdc2+i*3]!=255;
        if(old.contains("captives"))for(const auto &id:old["captives"])needed+=id!=255;
        if(old["moves"].size()+needed>1024)return "剩余记录空间不足以完成战后结算";
    }
    auto next=*this;auto &t=next.battle_["tactics"];const int cursor=next.random_;
    if(!t.contains("settlement"))t["settlement"]={{"stage",start},{"outcomes",Json::array()}};
    auto captives=t.value("captives",Json(std::vector<int>(24,255)));auto &settlement=t["settlement"];
    const int stage=settlement["stage"];
    auto result=next.state_.withdrawal_result_step(next.battle_["target"],next.battle_["source"],stage,captives,next.random_);
    if(result.contains("error"))return result["error"];
    t["captives"]=std::move(captives);settlement["stage"]=result["stage"];
    if(result["officer"]!=255){result["step"]=stage;settlement["outcomes"].push_back(result);}
    if(stage==12||stage==28)settlement["city_result"]=result;
    t["moves"].push_back({{"kind","advance_withdrawal_result"},{"cursor",cursor}});
    *this=std::move(next);return {};
}
std::string OriginalSession::finish_withdrawal_result(){
    if((phase_!="expedition"&&phase_!="battle")||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有待确认战果";
    const auto &t=battle_["tactics"];
    const int reason=t.value("turn_reason",0),branch=reason&127,finish=branch==20?4:branch==36?17:branch==3?29:13;
    if(t.value("turn_boundary",std::string{})!="battle_result"||(reason!=2&&reason!=3&&reason!=20&&reason!=36&&reason!=131&&reason!=148)||!t.contains("settlement")||t["settlement"]["stage"]!=finish||has_battle_captives())return "战后军团、俘虏和城池尚未全部结算";
    if((reason&128)&&(!t.contains("annexation")||t["annexation"]["next_city"]!=30))return "败方领地尚未全部处理";
    auto next=*this;
    const int target=next.battle_["target"],source=next.battle_["source"],ruler=next.state_.sram()[0xd8b];
    const bool invasion=next.phase_=="battle";
    if(invasion&&next.state_.sram()[0xd8f+4*ruler]!=0)return "电脑入侵前的命令书尚未清零";
    const int outcome=(next.state_.sram()[target*36]&7)==ruler?2:1;
    next.state_.close_battle(target);next.battle_=nullptr;
    if(invasion){
        // D8F4 returns to strategy; the exhausted invading ruler is then
        // skipped by A086 and the following ruler receives fresh orders.
        next.events_.push_back({{"kind","ai"},{"ruler",ruler},{"battle_result",outcome},{"source",source},{"target",target}});
        try{next.rotate_turn();}catch(const std::exception &e){return e.what();}
        if(next.events_.size()>64)next.events_.erase(next.events_.begin(),next.events_.end()-64);
    }else next.enter_turn();
    *this=std::move(next);return {};
}
}
