#include "zhongyuan/original_session.hpp"
#include <algorithm>
namespace zhongyuan {
// A separate event resumes an existing held assessment. Older computer_attack
// events keep their original meaning and do not retroactively perform a retreat.
std::string OriginalSession::resume_computer_attacker_retreat(){
    if(phase_!="battle"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有进攻军撤退决定";
    const auto &old=battle_["tactics"];
    if(tactical_side()!=128||old.value("turn_boundary",std::string{})!="computer"||!old.contains("attacker_ai")||old["attacker_ai"].value("stage",std::string{})!="held_retreat")return "请先完成电脑进攻军撤退评估";
    for(const auto *key:{"attack","scout","retreat","human_strategy"})if(old.contains(key))return "请先处理当前战术结果";
    if(old["points"]<1||old["moves"].size()>1000)return "撤军机动力或剩余记录空间不足";
    const int slot=old["attacker_ai"]["assessment"]["slot"];
    if(slot<0||slot>=11||state_.sram()[0xdc2+slot*3]>=241)return "无效电脑撤退武将";
    auto next=*this;auto &t=next.battle_["tactics"];const int cursor=next.random_;
    // DEB8-DECC synthesizes 0x30|slot without the commander's 0x40
    // marker. Therefore even a computer commander takes D354's single-unit
    // branch, not the human whole-army retreat at D3E7.
    Json outcomes=Json::array();
    auto result=next.state_.retreat_attacker(slot);if(result.contains("error"))return result["error"];
    outcomes.push_back(std::move(result));
    bool remaining=false;for(int i=0;i<11;++i)remaining|=next.state_.sram()[0xdc2+i*3]!=255;
    t["points"]=t["points"].get<int>()-1;t["selected"]=slot;
    t["retreat"]={{"slot",slot},{"commander",false},{"computer",true},{"stage","result"},{"outcomes",std::move(outcomes)},{"ended",!remaining}};
    t["attacker_ai"]["stage"]="retreat_result";
    next.computer_scratch21_=-1;next.computer_argument_=-1;
    t["moves"].push_back({{"kind","computer_attacker_retreat"},{"cursor",cursor}});*this=std::move(next);return {};
}
std::string OriginalSession::advance_computer_attack(){
    if(phase_!="battle"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有电脑先攻战场";
    const auto &old=battle_["tactics"];const auto &raw=state_.sram();const int owner=raw[0xde3]>>4;
    if(tactical_side()!=128||old.value("turn_boundary",std::string{})!="computer"||owner==(raw[0xd89]&7)||((raw[0xd8a]&128)&&owner==(raw[0xd8a]&7)))return "当前不是电脑进攻方行动";
    for(const auto *key:{"attack","scout","retreat","human_strategy"})if(old.contains(key))return "请先处理当前战术结果";
    if(old["moves"].size()>1000)return "剩余记录空间不足以继续电脑先攻";
    auto next=*this;auto &t=next.battle_["tactics"];const int cursor=next.random_,frame=next.clock_&255;
    if(!t.contains("attacker_ai"))t["attacker_ai"]={{"stage","assess"}};
    auto &ai=t["attacker_ai"];const std::string stage=ai["stage"];
    auto boundary=[&](bool end_turn)->std::string{
        Json context={{"side",128},{"points",0},{"round",t["round"]},{"carry",t["carry"]},{"phase",5},{"counter",1},{"reason",0},{"status",t["status"]}};
        auto result=next.state_.tactical_handover(context);if(result.contains("error"))return result["error"];
        if(result["phase"]==14){t["turn_boundary"]="battle_result";t["turn_reason"]=result["reason"];ai["stage"]="finished";}
        else if(end_turn){
            for(const auto *key:{"side","round","carry","status"})t[key]=result[key];
            t["points"]=std::min(40,result["points"].get<int>());t.erase("turn_boundary");
            const int selected=next.first_tactical_slot();if(selected<0)return "守城方没有可行动部队";
            t["selected"]=selected;ai["stage"]="finished";
        }else ai["stage"]="assess";
        next.computer_scratch21_=-1;next.computer_argument_=-1;return {};
    };
    if(stage=="assess"||stage=="finished"){
        if(t["points"]==0){if(auto error=boundary(true);!error.empty())return error;}
        else{
            auto result=next.state_.tactical_ai_assessment(128,t["round"],t["points"],next.random_);
            if(result.contains("error"))return result["error"];
            ai={{"stage",result["kind"]=="retreat"?"held_retreat":"select"},{"assessment",std::move(result)}};
        }
    }else if(stage=="select"){
        const bool reuse=t.value("attacker_initial_reuse",false);const int previous=t.value("computer_cursor",0);
        auto plan=next.state_.tactical_ai_attacker_plan(next.battle_["target"],previous,reuse);if(plan.contains("error"))return plan["error"];
        ai["marker"]=reuse?255:(previous+1)%11;ai["plan"]=std::move(plan);ai["stage"]="strategy_check";
        t["selected"]=ai["plan"]["slot"];t.erase("attacker_initial_reuse");
    }else if(stage=="strategy_check"){
        auto decision=next.state_.tactical_ai_strategy(next.battle_["target"],ai["plan"]["slot"],t["points"],t["status"],next.random_,false,nullptr,128,t["round"]);
        if(decision.contains("error"))return decision["error"];
        ai["stage"]=decision["kind"]=="strategy"?"strategy_execute":"motion";ai["strategy"]=std::move(decision);
    }else if(stage=="strategy_execute"){
        const auto &decision=ai["strategy"];const int cost=rom_.tactical_strategy_tables()["cost"][decision["strategy"].get<int>()];
        if(t["points"].get<int>()<cost)return "电脑施计机动力不足";
        auto result=next.state_.resolve_tactical_strategy(next.battle_["target"],128,decision["slot"],decision["target_slot"],decision["strategy"],frame,t["status"],next.random_);
        if(result.contains("error"))return result["error"];
        result["points_before"]=t["points"];t["points"]=t["points"].get<int>()-cost;result["points_after"]=t["points"];t["status"]=result["status"];
        ai["result"]=std::move(result);ai["stage"]="strategy_result";t["computer_cursor"]=decision["slot"];
        next.computer_scratch21_=-1;next.computer_argument_=-1;
    }else if(stage=="strategy_result"){
        if(auto error=boundary(t["points"]==0);!error.empty())return error;
    }else if(stage=="motion"){
        const int slot=ai["plan"]["slot"];
        auto decision=next.state_.tactical_ai_motion(next.battle_["target"],slot,t["points"],t["status"],true,nullptr,128,ai["plan"]["enemy_commander"]);
        if(decision.contains("error"))return decision["error"];
        if(decision["kind"]=="scan"){ai["motion"]=std::move(decision);ai["stage"]="scan";}
        else if(decision["kind"]=="move"||decision["kind"]=="attack"){
            const int directions[]={3,2,1,0},direction=directions[decision["direction"].get<int>()];int points=t["points"];
            if(decision["kind"]=="move"){
                if(auto error=next.state_.tactical_step(slot,direction,points,128);!error.empty())return error;
            }else if(points<3){
                int trial=3;if(next.state_.tactical_attack(slot,direction,trial,128).contains("error"))return "无效电脑攻击目标";
                decision["kind"]="exhausted_attack";if(auto error=boundary(true);!error.empty())return error;
            }else{
                auto attack=next.state_.tactical_attack(slot,direction,points,128);if(attack.contains("error"))return attack["error"];
                attack["clash"]=next.state_.prepare_clash(attack,true,frame,false);if(attack["clash"].contains("error"))return attack["clash"]["error"];
                attack["stage"]="clash_ready";attack["computer"]=true;t["attack"]=std::move(attack);
            }
            decision["points_before"]=old["points"];
            if(decision["kind"]!="exhausted_attack"){
                t["points"]=points;t["selected"]=slot;ai["stage"]="assess";
            }
            decision["points_after"]=t["points"];t["computer_cursor"]=slot;ai["motion"]=std::move(decision);
            next.computer_scratch21_=-1;next.computer_argument_=-1;
        }else return "电脑进攻军遇到尚未还原的寻路分支";
    }else if(stage=="scan"){
        auto scan=next.state_.tactical_ai_scan(128,ai["plan"]["slot"],ai["marker"]);if(scan.contains("error"))return scan["error"];
        if(scan["kind"]=="end_turn"){
            ai["scan"]=scan;if(auto error=boundary(true);!error.empty())return error;
        }else{
            auto plan=next.state_.tactical_ai_attacker_plan(next.battle_["target"],scan["slot"],true);if(plan.contains("error"))return plan["error"];
            ai["marker"]=scan["marker"];ai["scan"]=scan;ai["plan"]=std::move(plan);ai["stage"]="strategy_check";ai.erase("motion");ai.erase("strategy");t["selected"]=scan["slot"];
        }
    }else if(stage=="held_retreat")return "电脑进攻军已决定撤军，请执行撤退决定";
    else return "无效电脑先攻阶段";
    t["moves"].push_back({{"kind","computer_attack"},{"cursor",cursor},{"frame",frame}});*this=std::move(next);return {};
}
}
