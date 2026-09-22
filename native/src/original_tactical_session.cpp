#include "zhongyuan/original_session.hpp"
#include <algorithm>
namespace zhongyuan {
int OriginalSession::tactical_side() const {return battle_.is_object()&&battle_.contains("tactics")?battle_["tactics"].value("side",128):128;}
int OriginalSession::first_tactical_slot() const {
    const int side=tactical_side();for(int i=0;i<(side?11:12);++i)if(state_.sram()[side?0xdc2+i*3:0xdaa+i*2]<241)return i;return -1;
}
std::string OriginalSession::tactical_action_error() const {
    if((phase_!="expedition"&&phase_!="battle")||!battle_.is_object()||!battle_.contains("tactics"))return "请先进入战术行动";
    const auto &t=battle_["tactics"];
    if(t.contains("human_strategy"))return "请先确认、取消计策或确认施计结果";
    if(t.contains("turn_boundary"))return t["turn_boundary"]=="computer"?"轮到电脑战术行动，请先推进电脑行动":"战斗已进入战后阶段，请先完成结果确认";
    const auto &s=state_.sram();const int owner=tactical_side()?s[0xde3]>>4:s[0xde3]&7;
    if(owner!=(s[0xd89]&7)&&!((s[0xd8a]&128)&&owner==(s[0xd8a]&7)))return "当前轮到电脑行动";
    return {};
}
std::string OriginalSession::end_tactical_turn(){
    if(auto error=tactical_action_error();!error.empty())return error;
    const auto &old=battle_["tactics"];
    if(old.contains("attack")||old.contains("scout")||old.contains("retreat"))return "请先完成当前攻击、侦察或撤退";
    if(old["moves"].size()>=1022)return "当前行动记录已满";
    auto next=*this;auto &t=next.battle_["tactics"];
    Json context={{"side",tactical_side()},{"points",t["points"]},{"round",t.value("round",0)},{"carry",t.value("carry",0)},
        {"phase",5},{"counter",8},{"reason",0},{"status",t.value("status",Json(std::vector<int>(24,0)))}};
    auto result=next.state_.tactical_end_turn(context);if(result.contains("error"))return result["error"];
    result=next.state_.tactical_handover(result);if(result.contains("error"))return result["error"];
    for(const auto *key:{"side","round","carry","status"})t[key]=result[key];
    // C010 clamps the displayed, usable budget to 40 after the turn banner.
    t["points"]=std::min(40,result["points"].get<int>());
    if(result["phase"]==15)t["turn_boundary"]="time_limit";
    else if(result["phase"]==14){t["turn_boundary"]="battle_result";t["turn_reason"]=result["reason"];}
    else{
        const auto &s=next.state_.sram();const int owner=result["side"]==128?s[0xde3]>>4:s[0xde3]&7;
        const bool human=owner==(s[0xd89]&7)||((s[0xd8a]&128)&&owner==(s[0xd8a]&7));
        if(!human)t["turn_boundary"]="computer";
        const int selected=next.first_tactical_slot();if(selected<0)return "下一方没有可行动部队";t["selected"]=selected;
    }
    next.handover_argument_=next.computer_argument_;
    next.computer_scratch21_=-1;next.computer_argument_=-1;
    t["moves"].push_back({{"kind","end_tactical_turn"},{"slot",old["selected"]}});*this=std::move(next);return {};
}
std::string OriginalSession::advance_computer_tactics(){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有电脑战术回合";
    const auto &old=battle_["tactics"];
    if(old.value("turn_boundary",std::string{})!="computer"||old.contains("attack")||old.contains("scout")||old.contains("retreat"))return "请先完成当前战术结果";
    const int side=tactical_side();const auto &raw=state_.sram();const int owner=side?raw[0xde3]>>4:raw[0xde3]&7;
    if(owner==(raw[0xd89]&7)||((raw[0xd8a]&128)&&owner==(raw[0xd8a]&7)))return "当前轮到玩家行动";
    if(old.contains("computer")&&old["computer"]["kind"]=="plan")return "电脑移动、攻击与计策决策尚未接入，当前评估已保存";
    int needed=6;for(int i=0;i<11;++i)needed+=raw[0xdc2+i*3]!=255;
    if(old.contains("captives"))for(const auto &id:old["captives"])needed+=id!=255;
    if(old["moves"].size()+needed>1024)return "剩余记录空间不足以完成电脑撤军与战后结算";
    auto next=*this;auto &t=next.battle_["tactics"];const int cursor=next.random_;
    next.computer_scratch21_=-1; // A new assessment follows an untracked command/presentation boundary.
    if(t["points"]==0){
        Json context={{"side",side},{"points",0},{"round",t.value("round",0)},{"carry",t.value("carry",0)},
            {"phase",5},{"counter",1},{"reason",0},{"status",t.value("status",Json(std::vector<int>(24,0)))}};
        const auto result=next.state_.tactical_handover(context);if(result.contains("error"))return result["error"];
        for(const auto *key:{"side","round","carry","status"})t[key]=result[key];
        t["points"]=std::min(40,result["points"].get<int>());t.erase("computer");
        if(result["phase"]==15)t["turn_boundary"]="time_limit";
        else if(result["phase"]==14){t["turn_boundary"]="battle_result";t["turn_reason"]=result["reason"];}
        else{t.erase("turn_boundary");const int selected=next.first_tactical_slot();if(selected<0)return "下一方没有可行动部队";t["selected"]=selected;}
    }else{
        if(side!=0)return "电脑先攻流程尚未接入";
        auto assessment=next.state_.tactical_ai_assessment(side,t.value("round",0),t["points"],next.random_);
        if(assessment.contains("error"))return assessment["error"];
        if(assessment.value("score",-1)>=0){
            for(int i=10;i>=0;--i){const int id=next.state_.sram()[0xdc2+3*i];if(id<241){next.computer_scratch21_=(0x6438+8*id)>>8;break;}}
        }
        if(assessment["kind"]=="retreat"){
            const int slot=assessment["slot"];
            auto result=next.state_.retreat_defender(slot,next.battle_["target"],false,next.random_);
            if(result.contains("error"))return result["error"];
            if(!result["departed"].get<bool>())return "电脑撤退未能离场";
            next.computer_scratch21_=-1;
            t["points"]=t["points"].get<int>()-1;t["selected"]=slot;
            bool remaining=false;for(int i=0;i<11;++i)remaining|=next.state_.sram()[0xdaa+i*2]!=255;
            t["retreat"]={{"slot",slot},{"commander",false},{"defending",true},{"computer",true},{"stage","result"},
                {"outcomes",Json::array({result})},{"ended",!remaining}};
        }
        t["computer"]=std::move(assessment);t.erase("computer_plan");t.erase("computer_motion");t.erase("computer_scan");
    }
    t["moves"].push_back({{"kind","computer_tactics"},{"cursor",cursor}});*this=std::move(next);return {};
}

std::string OriginalSession::plan_computer_tactics(bool reuse_unit){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有电脑战术计划";
    const auto &old=battle_["tactics"];
    if(tactical_side()!=0||old.value("turn_boundary",std::string{})!="computer"||old.contains("retreat")||old.contains("attack")||old.contains("scout")||
       !old.contains("computer")||old["computer"]["kind"]!="plan"||old.contains("computer_plan"))return "请先完成电脑撤军评估或当前行动";
    if(reuse_unit!=old.contains("computer_reuse"))return "请选择正确的电脑部队续行入口";
    if(old["computer"].value("role_slot",-1)>=0)return "守军角色调整后的命令衔接尚未完成";
    if(old["moves"].size()>=1024)return "当前行动记录已满";
    auto next=*this;auto &t=next.battle_["tactics"];
    auto plan=next.state_.tactical_ai_fort(next.battle_["target"],t.value("computer_cursor",0),t["points"],t.value("status",Json(std::vector<int>(24,0))),reuse_unit);
    if(plan.contains("error"))return plan["error"];
    if(plan["kind"]=="move"){
        next.computer_scratch21_=-1;next.computer_argument_=plan["direction"];next.remember_argument(plan["direction"]);
        const int directions[]={3,2,1,0};int points=t["points"];
        const auto error=next.state_.tactical_step(plan["slot"],directions[plan["direction"].get<int>()],points,0);
        if(!error.empty()){
            if(error!="剩余机动力不足")return error;
            // C886-C889 returns to assessment with unchanged mobility/position.
            // The earlier fort-role assignment and emitted argument survive.
            plan["blocked"]=true;
        }
        plan["points_before"]=t["points"];plan["points_after"]=points;
        plan["destination"]=next.state_.sram()[0xdab+plan["slot"].get<int>()*2];
        t["points"]=points;t["selected"]=plan["slot"];t["computer_cursor"]=plan["slot"];t["computer"]["kind"]="moved";
    }
    if(reuse_unit)t.erase("computer_reuse");
    t["computer_plan"]=std::move(plan);t["moves"].push_back({{"kind",reuse_unit?"computer_reuse":"computer_fort"}});*this=std::move(next);return {};
}

std::string OriginalSession::evaluate_computer_strategy(){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有电脑计策评估";
    const auto &old=battle_["tactics"];
    if(tactical_side()!=0||old.value("turn_boundary",std::string{})!="computer"||old.contains("attack")||old.contains("scout")||old.contains("retreat")||
       !old.contains("computer")||old["computer"]["kind"]!="plan"||!old.contains("computer_plan")||old["computer_plan"]["kind"]!="role_plan"||old.contains("computer_strategy"))return "请先完成电脑部队选择；计策决定不能重复评估";
    if(old["moves"].size()>=1024)return "当前行动记录已满";
    auto next=*this;auto &t=next.battle_["tactics"];const int cursor=next.random_;
    auto result=next.state_.tactical_ai_strategy(next.battle_["target"],t["computer_plan"]["slot"],t["points"],t.value("status",Json(std::vector<int>(24,0))),next.random_,false,&next.computer_scratch21_);
    if(result.contains("error"))return result["error"];
    t["selected"]=result["slot"];if(result["kind"]=="strategy")next.remember_argument(result["argument"]);
    t["computer_strategy"]=std::move(result);
    t["moves"].push_back({{"kind","computer_strategy"},{"cursor",cursor}});*this=std::move(next);return {};
}

std::string OriginalSession::execute_computer_strategy(){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有待执行电脑计策";
    const auto &old=battle_["tactics"];
    if(tactical_side()!=0||old.value("turn_boundary",std::string{})!="computer"||old.contains("strategy_result")||old.contains("attack")||old.contains("retreat")||old.contains("scout")||!old.contains("computer_strategy")||old["computer_strategy"]["kind"]!="strategy")return "请先选定电脑计策并完成当前结果";
    int needed=8;const auto &raw=state_.sram();for(int i=0;i<11;++i)needed+=raw[0xdc2+i*3]!=255;
    if(old.contains("captives"))for(const auto &id:old["captives"])needed+=id!=255;
    if(old["moves"].size()+needed>1024)return "剩余记录空间不足以结算计策及可能的战果";
    const auto &decision=old["computer_strategy"];const int cost=rom_.tactical_strategy_tables()["cost"][decision["strategy"].get<int>()];
    if(old["points"].get<int>()<cost)return "计策机动力不足";
    auto next=*this;auto &t=next.battle_["tactics"];const int cursor=next.random_,frame=next.clock_&255;
    auto result=next.state_.resolve_tactical_strategy(next.battle_["target"],0,decision["slot"],decision["target_slot"],decision["strategy"],frame,t.value("status",Json(std::vector<int>(24,0))),next.random_);
    if(result.contains("error"))return result["error"];
    next.computer_scratch21_=-1;next.computer_argument_=-1;
    result["points_before"]=t["points"];t["points"]=t["points"].get<int>()-cost;result["points_after"]=t["points"];
    t["status"]=result["status"];t["strategy_result"]=std::move(result);t["moves"].push_back({{"kind","execute_computer_strategy"},{"cursor",cursor},{"frame",frame}});
    *this=std::move(next);return {};
}

std::string OriginalSession::finish_computer_strategy(){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有电脑计策结果";
    const auto &old=battle_["tactics"];
    if(old.value("turn_boundary",std::string{})!="computer"||!old.contains("strategy_result")||old["moves"].size()>=1024)return "请先执行电脑计策";
    auto next=*this;auto &t=next.battle_["tactics"];
    Json context={{"side",0},{"points",0},{"round",t.value("round",0)},{"carry",t.value("carry",0)},{"phase",5},{"counter",1},{"reason",0},{"status",t["status"]}};
    const auto boundary=next.state_.tactical_handover(context);if(boundary.contains("error"))return boundary["error"];
    t["computer_cursor"]=t["strategy_result"]["slot"];t["last_strategy"]=t["strategy_result"];t.erase("strategy_result");t.erase("computer_strategy");t.erase("computer_plan");t["computer"]["kind"]="acted";
    if(boundary["phase"]==14){t["turn_boundary"]="battle_result";t["turn_reason"]=boundary["reason"];}
    else if(t["points"]==0){
        for(const auto *key:{"side","round","carry","status"})t[key]=boundary[key];
        t["points"]=std::min(40,boundary["points"].get<int>());t.erase("computer");
        if(boundary["phase"]==15)t["turn_boundary"]="time_limit";
        else{t.erase("turn_boundary");const int selected=next.first_tactical_slot();if(selected<0)return "下一方没有可行动部队";t["selected"]=selected;}
    }
    t["moves"].push_back({{"kind","finish_computer_strategy"}});*this=std::move(next);return {};
}

std::string OriginalSession::continue_computer_motion(){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有待续电脑行动";
    const auto &old=battle_["tactics"];
    if(tactical_side()!=0||old.value("turn_boundary",std::string{})!="computer"||old.contains("attack")||old.contains("retreat")||old.contains("scout")||old.contains("strategy_result")||old.contains("computer_motion")||
       !old.contains("computer")||old["computer"]["kind"]!="plan"||!old.contains("computer_strategy")||old["computer_strategy"]["kind"]!="no_strategy")return "请先完成电脑计策评估；不能重复执行行动";
    if(old["moves"].size()>1000)return "剩余记录空间不足以衔接电脑交战";
    auto next=*this;
    auto decision=next.state_.tactical_ai_motion(battle_["target"],old["computer_strategy"]["slot"],old["points"],old["status"],false,&next.computer_scratch21_);
    const auto error=next.apply_computer_motion(std::move(decision),"computer_motion");if(!error.empty())return error;
    *this=std::move(next);return {};
}

std::string OriginalSession::continue_computer_nearby(){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有电脑近敌搜索";
    const auto &old=battle_["tactics"];
    if(tactical_side()!=0||old.value("turn_boundary",std::string{})!="computer"||old.contains("attack")||old.contains("retreat")||old.contains("scout")||old.contains("strategy_result")||
       !old.contains("computer")||old["computer"]["kind"]!="plan"||!old.contains("computer_strategy")||old["computer_strategy"]["kind"]!="no_strategy"||
       !old.contains("computer_motion")||old["computer_motion"]["kind"]!="pending"||old["computer_motion"]["branch"]!="role")return "请先进入电脑附近敌军搜索边界";
    if(old["moves"].size()>1000)return "剩余记录空间不足以衔接电脑交战";
    const int slot=old["computer_motion"]["slot"],role=state_.sram()[0xdec+2*slot];
    if(!(role&1)||role==15)return "当前守军不使用附近敌军搜索";
    auto next=*this;
    auto decision=next.state_.tactical_ai_motion(battle_["target"],slot,old["points"],old["status"],true,&next.computer_scratch21_);
    const auto error=next.apply_computer_motion(std::move(decision),"computer_nearby");if(!error.empty())return error;
    *this=std::move(next);return {};
}

std::string OriginalSession::continue_computer_occupied_fort(bool recover_scratch){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有敌军占堡后的电脑行动";
    const auto &old=battle_["tactics"];
    if(tactical_side()!=0||old.value("turn_boundary",std::string{})!="computer"||old.contains("attack")||old.contains("retreat")||old.contains("scout")||old.contains("strategy_result")||old.contains("computer_motion")||
       !old.contains("computer")||old["computer"]["kind"]!="plan"||!old.contains("computer_plan")||!old.contains("computer_strategy")||old["computer_strategy"]["kind"]!="role_reassignment")return "请先进入敌军占领城堡后的行动边界";
    if(old["moves"].size()>1000)return "剩余记录空间不足以衔接电脑交战";
    auto next=*this;const int cursor=next.random_;
    // Preserve the original event's limited inference exactly. The new
    // resume event uses the private tracker reconstructed through all prior
    // decision events; no temporary byte is accepted from the save payload.
    int scratch=-1;
    if(!old.contains("computer_scan")&&old["computer"].value("score",-1)>=0){
        for(int i=10;i>=0;--i){const int id=next.state_.sram()[0xdc2+3*i];if(id<241){scratch=(0x6438+8*id)>>8;break;}}
    }
    if(recover_scratch)scratch=next.computer_scratch21_;
    const char *event=recover_scratch?"computer_occupied_fort_resume":"computer_occupied_fort";
    auto decision=next.state_.tactical_ai_occupied_fort(next.battle_["target"],old["computer_strategy"]["slot"],old["points"],old["status"],next.random_,scratch);
    if(decision.contains("error"))return decision["error"];
    if(decision.value("scratch21",-1)>=0)next.computer_scratch21_=decision["scratch21"];
    if(decision["kind"]=="strategy"){
        next.remember_argument(decision["argument"]);
        auto &t=next.battle_["tactics"];t["computer_strategy"]=std::move(decision);
        t["moves"].push_back({{"kind",event},{"cursor",cursor},{"frame",next.clock_&255}});
    }else{
        const auto error=next.apply_computer_motion(std::move(decision),event);if(!error.empty())return error;
        auto &t=next.battle_["tactics"];t["moves"].back()["cursor"]=cursor;
        if(t.contains("computer_strategy"))t["computer_strategy"]["kind"]="no_strategy";
    }
    *this=std::move(next);return {};
}

std::string OriginalSession::apply_computer_motion(Json decision,const char *event){
    auto next=*this;auto &t=next.battle_["tactics"];const int frame=next.clock_&255;
    if(decision.contains("error"))return decision["error"];
    if(decision["kind"]=="move"||decision["kind"]=="attack"){
        next.computer_scratch21_=-1;next.computer_argument_=decision["kind"]=="move"?decision["direction"].get<int>():-1;
        next.remember_argument(decision["direction"]);
        const int directions[]={3,2,1,0};const int slot=decision["slot"],direction=directions[decision["direction"].get<int>()];int points=t["points"];
        if(decision["kind"]=="move"){
            const auto error=next.state_.tactical_step(slot,direction,points,0);if(!error.empty())return error;
        }else{
            auto attack=next.state_.tactical_attack(slot,direction,points,0);if(attack.contains("error"))return attack["error"];
            const auto &raw=next.state_.sram();const int owner=raw[0xde3]>>4;
            const bool human=(raw[0xd89]&7)==owner||((raw[0xd8a]&128)&&(raw[0xd8a]&7)==owner);
            attack["clash"]=next.state_.prepare_clash(attack,human,frame,false);
            if(attack["clash"].contains("error"))return attack["clash"]["error"];
            attack["stage"]="clash_ready";attack["computer"]=true;t["attack"]=std::move(attack);
        }
        decision["points_before"]=t["points"];decision["points_after"]=points;
        t["points"]=points;t["selected"]=slot;t["computer_cursor"]=slot;t["computer"]["kind"]="acted";
        t.erase("computer_strategy");t.erase("computer_plan");
    }
    t["computer_motion"]=std::move(decision);t["moves"].push_back({{"kind",event},{"frame",frame}});*this=std::move(next);return {};
}

std::string OriginalSession::continue_computer_flank(){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有电脑绕行计划";
    const auto &old=battle_["tactics"];
    if(tactical_side()!=0||old.value("turn_boundary",std::string{})!="computer"||old.contains("attack")||old.contains("retreat")||old.contains("scout")||old.contains("strategy_result")||
       !old.contains("computer_motion")||old["computer_motion"]["kind"]!="pending"||(old["computer_motion"]["branch"]!="fort_flank"&&old["computer_motion"]["branch"]!="cached_target")||
       !old.contains("computer_strategy")||old["computer_strategy"]["kind"]!="no_strategy"||!old.contains("computer")||old["computer"]["kind"]!="plan")return "请先进入电脑绕行或缓存目标边界";
    if(old["moves"].size()>1000)return "剩余记录空间不足以继续电脑绕行";
    auto next=*this;auto &t=next.battle_["tactics"];
    auto result=next.state_.tactical_ai_flank(next.battle_["target"],t["computer_motion"]["slot"],t["points"],t["status"]);
    if(result.contains("error"))return result["error"];
    if(result["kind"]=="move"){
        next.computer_scratch21_=-1;next.computer_argument_=result["direction"];next.remember_argument(result["direction"]);
        const int directions[]={3,2,1,0};const int slot=result["slot"];int points=t["points"];
        const auto error=next.state_.tactical_step(slot,directions[result["direction"].get<int>()],points,0);if(!error.empty())return error;
        result["points_before"]=t["points"];result["points_after"]=points;t["points"]=points;t["selected"]=slot;t["computer_cursor"]=slot;
        t["computer_reuse"]=slot;t["computer"]["kind"]="acted";t.erase("computer_strategy");t.erase("computer_plan");
    }
    t["computer_motion"]=std::move(result);t["moves"].push_back({{"kind","computer_flank"}});*this=std::move(next);return {};
}

std::string OriginalSession::retry_computer_strategy(){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有电脑补充计策评估";
    const auto &old=battle_["tactics"];
    if(tactical_side()!=0||old.value("turn_boundary",std::string{})!="computer"||old.contains("attack")||old.contains("retreat")||old.contains("scout")||old.contains("strategy_result")||
       !old.contains("computer_motion")||old["computer_motion"]["kind"]!="retry_strategy"||!old.contains("computer_strategy")||old["computer_strategy"]["kind"]!="no_strategy"||
       !old.contains("computer")||old["computer"]["kind"]!="plan")return "当前没有原版要求的补充计策评估";
    if(old["moves"].size()>1000)return "剩余记录空间不足以执行补充计策";
    auto next=*this;auto &t=next.battle_["tactics"];const int cursor=next.random_;
    auto result=next.state_.tactical_ai_strategy(next.battle_["target"],t["computer_motion"]["slot"],t["points"],t["status"],next.random_,true,&next.computer_scratch21_);
    if(result.contains("error"))return result["error"];
    if(result["kind"]=="strategy")t.erase("computer_motion");
    else{t["computer_motion"]["kind"]="scan";t["computer_motion"]["branch"]="strategy_retry";}
    if(result["kind"]=="strategy")next.remember_argument(result["argument"]);
    t["computer_strategy"]=std::move(result);t["moves"].push_back({{"kind","computer_strategy_retry"},{"cursor",cursor}});*this=std::move(next);return {};
}

std::string OriginalSession::continue_computer_scan(){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有电脑部队轮询";
    const auto &old=battle_["tactics"];
    if(tactical_side()!=0||old.value("turn_boundary",std::string{})!="computer"||old.contains("attack")||old.contains("retreat")||old.contains("scout")||old.contains("strategy_result")||
       !old.contains("computer")||old["computer"]["kind"]!="plan"||!old.contains("computer_plan")||
       !old.contains("computer_motion")||old["computer_motion"]["kind"]!="scan")return "请先完成当前部队的计策和行动检查";
    if(old["moves"].size()>1000)return "剩余记录空间不足以完成电脑轮询及交接";
    // Old v2 saves did not store $98. Reconstruct the first candidate, which
    // can be an empty slot, or FF when A0 selected the same unit directly.
    int marker=old.contains("computer_scan")?old["computer_scan"]["marker"].get<int>():
        old["computer_plan"].value("reuse_unit",false)?255:(old["computer_plan"]["previous_slot"].get<int>()+11)%12;
    auto next=*this;auto &t=next.battle_["tactics"];
    auto scan=next.state_.tactical_ai_scan(0,t["computer_motion"]["slot"],marker);
    if(scan.contains("error"))return scan["error"];
    if(scan["kind"]=="select"){
        // A326 resumes at the selected occupied slot without A2D7 resetting
        // the origin and without rerunning the A1D9 retreat assessment.
        auto plan=next.state_.tactical_ai_fort(next.battle_["target"],scan["slot"],t["points"],t["status"],true);
        if(plan.contains("error"))return plan["error"];
        plan.erase("reuse_unit");plan["previous_slot"]=scan["previous_slot"];
        if(plan["kind"]=="move"){
            next.computer_scratch21_=-1;next.computer_argument_=plan["direction"];next.remember_argument(plan["direction"]);
            const int directions[]={3,2,1,0};int points=t["points"];
            const auto error=next.state_.tactical_step(plan["slot"],directions[plan["direction"].get<int>()],points,0);
            if(!error.empty()){
                if(error!="剩余机动力不足")return error;
                plan["blocked"]=true;
            }
            plan["points_before"]=t["points"];plan["points_after"]=points;plan["destination"]=next.state_.sram()[0xdab+plan["slot"].get<int>()*2];
            t["points"]=points;t["computer_cursor"]=plan["slot"];t["computer"]["kind"]="moved";
        }
        t["selected"]=plan["slot"];t["computer_plan"]=std::move(plan);t["computer_scan"]=std::move(scan);
        t.erase("computer_motion");t.erase("computer_strategy");
    }else{
        next.computer_scratch21_=-1;
        // DEAC-DEB5 computer command 4 clears mobility and jumps to C761, bypassing
        // the human C6EC end-turn carry calculation. Keep existing carry intact.
        Json context={{"side",0},{"points",0},{"round",t.value("round",0)},{"carry",t.value("carry",0)},
            {"phase",5},{"counter",1},{"reason",0},{"status",t["status"]}};
        auto result=next.state_.tactical_handover(context);if(result.contains("error"))return result["error"];
        scan["points_before"]=t["points"];scan["points_after"]=std::min(40,result["points"].get<int>());
        t["computer_cursor"]=scan["slot"];t["last_computer_scan"]=std::move(scan);
        for(const auto *key:{"side","round","carry","status"})t[key]=result[key];
        t["points"]=std::min(40,result["points"].get<int>());
        for(const auto *key:{"computer","computer_plan","computer_strategy","computer_motion","computer_scan","computer_reuse"})t.erase(key);
        if(result["phase"]==15)t["turn_boundary"]="time_limit";
        else if(result["phase"]==14){t["turn_boundary"]="battle_result";t["turn_reason"]=result["reason"];}
        else{
            const auto &s=next.state_.sram();const int owner=result["side"]==128?s[0xde3]>>4:s[0xde3]&7;
            const bool human=owner==(s[0xd89]&7)||((s[0xd8a]&128)&&owner==(s[0xd8a]&7));
            if(human)t.erase("turn_boundary");else t["turn_boundary"]="computer";
            const int selected=next.first_tactical_slot();if(selected<0)return "下一方没有可行动部队";t["selected"]=selected;
        }
    }
    t["moves"].push_back({{"kind","computer_scan"}});*this=std::move(next);return {};
}

// D5D3/DE86 mode6 -> DF27-DF36: a computer attack with fewer than
// three points ends its turn directly. Preserve the legacy attempted-action
// event semantics and replay this newly supported continuation separately.
std::string OriginalSession::finish_exhausted_computer_attack(){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有待处理电脑攻击";
    const auto &old=battle_["tactics"];
    if(tactical_side()!=0||old.value("turn_boundary",std::string{})!="computer"||old.contains("attack")||old.contains("scout")||old.contains("retreat")||old.contains("strategy_result")||
       !old.contains("computer")||old["computer"]["kind"]!="plan"||!old.contains("computer_strategy")||old["computer_strategy"]["kind"]!="no_strategy"||old["points"]<1||old["points"]>=3)
        return "当前不是机动力不足的电脑攻击";
    const bool nearby=old.contains("computer_motion");
    if(nearby&&(old["computer_motion"]["kind"]!="pending"||old["computer_motion"]["branch"]!="role"))return "请先完成当前电脑行动";
    if(old["moves"].size()>1000)return "剩余记录空间不足以交接电脑回合";
    auto decision=state_.tactical_ai_motion(battle_["target"],old["computer_strategy"]["slot"],old["points"],old["status"],nearby);
    if(decision.value("kind",std::string{})!="attack")return "当前没有因机动力不足而中止的攻击";
    // Verify the exact target independently; malformed/missing targets must
    // not be turned into a successful end-turn operation.
    const int directions[]={3,2,1,0};int trial_points=3;
    if(state_.tactical_attack(decision["slot"],directions[decision["direction"].get<int>()],trial_points,0).contains("error"))return "电脑攻击目标无效";
    auto next=*this;auto &t=next.battle_["tactics"];
    Json context={{"side",0},{"points",0},{"round",t.value("round",0)},{"carry",t.value("carry",0)},
        {"phase",5},{"counter",1},{"reason",0},{"status",t["status"]}};
    const auto result=next.state_.tactical_handover(context);if(result.contains("error"))return result["error"];
    next.remember_argument(decision["direction"]);
    decision["kind"]="exhausted_attack";decision["points_before"]=t["points"];decision["points_after"]=std::min(40,result["points"].get<int>());
    t["computer_cursor"]=decision["slot"];t["last_exhausted_attack"]=std::move(decision);
    for(const auto *key:{"side","round","carry","status"})t[key]=result[key];
    t["points"]=std::min(40,result["points"].get<int>());
    for(const auto *key:{"computer","computer_plan","computer_strategy","computer_motion","computer_scan","computer_reuse"})t.erase(key);
    if(result["phase"]==15)t["turn_boundary"]="time_limit";
    else if(result["phase"]==14){t["turn_boundary"]="battle_result";t["turn_reason"]=result["reason"];}
    else{
        const auto &s=next.state_.sram();const int owner=result["side"]==128?s[0xde3]>>4:s[0xde3]&7;
        const bool human=owner==(s[0xd89]&7)||((s[0xd8a]&128)&&owner==(s[0xd8a]&7));
        if(human)t.erase("turn_boundary");else t["turn_boundary"]="computer";
        const int selected=next.first_tactical_slot();if(selected<0)return "下一方没有可行动部队";t["selected"]=selected;
    }
    t["moves"].push_back({{"kind","computer_exhausted_attack"}});*this=std::move(next);return {};
}

}
