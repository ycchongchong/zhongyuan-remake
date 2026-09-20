#include "zhongyuan/original_session.hpp"
#include <algorithm>
namespace zhongyuan {
Json OriginalState::player_tactical_strategies(int side,int slot) const {
    if((side!=0&&side!=128)||slot<0||slot>=(side?11:12))return {{"error","请选择施计部队"}};
    const int base=side?0xdc2+slot*3:0xdaa+slot*2,id=bytes_[base];
    if(id>=241||bytes_[base+1]<16||bytes_[base+1]>=160)return {{"error","施计部队无效"}};
    const int intelligence=bytes_[0x438+id*8+2],tier=intelligence<40?0:intelligence<60?2:intelligence<80?4:7;
    const auto tables=rom_.player_tactical_strategy_tables();Json choices=Json::array();
    for(int page=0;page<2;++page){const int group=tables["tiers"][tier+page];if(group==255)break;
        for(const auto &strategy:tables["groups"][group])choices.push_back({{"strategy",strategy},{"cost",tables["cost"][strategy.get<int>()]},{"page",page}});
    }
    return {{"slot",slot},{"side",side},{"intelligence",intelligence},{"choices",choices}};
}
Json OriginalState::quote_tactical_strategy(int side,int slot,int target_slot,int strategy,int points) const {
    auto result=player_tactical_strategies(side,slot);if(result.contains("error"))return result;
    if(strategy<0||strategy>=8||points<0||points>40||target_slot<0||target_slot>=(side?12:11))return {{"error","无效施计目标或参数"}};
    bool available=false;for(const auto &v:result["choices"])available|=v["strategy"]==strategy;
    if(!available)return {{"error","该武将智力尚不能使用此计"}};
    const auto tables=rom_.player_tactical_strategy_tables();const int cost=tables["cost"][strategy];
    if(points<cost)return {{"error","施计机动力不足"}};
    const int base=side?0xdaa+target_slot*2:0xdc2+target_slot*3,id=bytes_[base],cell=bytes_[base+1];
    if(id>=241||cell<16||cell>=160||(bytes_[0xe1a+cell]&63)!=((side?16:48)|target_slot))return {{"error","请选择有效敌军部队"}};
    const int terrain=bytes_[0xf1a+cell];bool allowed=false;
    for(int i=0;i<8;++i){const int value=tables["terrain"][strategy*8+i];if(value==255||value==terrain){allowed=true;break;}}
    if(!allowed)return {{"error","目标地形不能使用此计"}};
    return {{"slot",slot},{"target_slot",target_slot},{"strategy",strategy},{"side",side},{"cost",cost},
        {"target_cell",cell},{"target_terrain",terrain},{"intelligence",result["intelligence"]}};
}
Json OriginalSession::player_tactical_strategies(int slot) const {
    if(auto error=tactical_action_error();!error.empty())return {{"error",error}};
    return state_.player_tactical_strategies(tactical_side(),slot);
}
std::string OriginalSession::prepare_tactical_strategy(int slot,int target_slot,int strategy){
    if(auto error=tactical_action_error();!error.empty())return error;
    const auto &old=battle_["tactics"];
    if(old.contains("attack")||old.contains("scout")||old.contains("retreat"))return "请先处理当前战术指令";
    if(old["moves"].size()>1020)return "剩余记录空间不足以完成施计";
    auto proposal=state_.quote_tactical_strategy(tactical_side(),slot,target_slot,strategy,old["points"]);
    if(proposal.contains("error"))return proposal["error"];
    proposal["stage"]="confirm";auto &t=battle_["tactics"];t["selected"]=slot;t["human_strategy"]=std::move(proposal);
    t["moves"].push_back({{"kind","prepare_tactical_strategy"},{"slot",slot},{"target_slot",target_slot},{"strategy",strategy}});return {};
}
std::string OriginalSession::cancel_tactical_strategy(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("human_strategy")||battle_["tactics"]["human_strategy"]["stage"]!="confirm")return "当前没有待取消的计策";
    auto &t=battle_["tactics"];if(t["moves"].size()>=1024)return "当前行动记录已满";
    t.erase("human_strategy");t["moves"].push_back({{"kind","cancel_tactical_strategy"}});return {};
}
std::string OriginalSession::confirm_tactical_strategy(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("human_strategy")||battle_["tactics"]["human_strategy"]["stage"]!="confirm")return "当前没有待确认的计策";
    const auto &old=battle_["tactics"];if(old["moves"].size()>1022)return "剩余记录空间不足以确认施计结果";
    const auto proposal=old["human_strategy"];auto next=*this;auto &t=next.battle_["tactics"];const int cursor=next.random_,frame=next.clock_&255;
    auto quote=next.state_.quote_tactical_strategy(next.tactical_side(),proposal["slot"],proposal["target_slot"],proposal["strategy"],t["points"]);
    if(quote.contains("error"))return quote["error"];
    auto result=next.state_.resolve_tactical_strategy(next.battle_["target"],next.tactical_side(),proposal["slot"],proposal["target_slot"],proposal["strategy"],frame,t.value("status",Json(std::vector<int>(24,0))),next.random_);
    if(result.contains("error"))return result["error"];
    result["points_before"]=t["points"];t["points"]=t["points"].get<int>()-quote["cost"].get<int>();result["points_after"]=t["points"];
    t["status"]=result["status"];t["human_strategy"]["stage"]="result";t["human_strategy"]["result"]=std::move(result);
    next.computer_argument_=-1;next.computer_scratch21_=-1;
    t["moves"].push_back({{"kind","confirm_tactical_strategy"},{"cursor",cursor},{"frame",frame}});*this=std::move(next);return {};
}
std::string OriginalSession::finish_tactical_strategy(){
    if(!battle_.is_object()||!battle_.contains("tactics")||!battle_["tactics"].contains("human_strategy")||battle_["tactics"]["human_strategy"]["stage"]!="result")return "请先执行计策";
    if(battle_["tactics"]["moves"].size()>=1024)return "当前行动记录已满";
    auto next=*this;auto &t=next.battle_["tactics"];
    Json context={{"side",next.tactical_side()},{"points",0},{"round",t.value("round",0)},{"carry",t.value("carry",0)},{"phase",5},{"counter",1},{"reason",0},{"status",t["status"]}};
    const auto boundary=next.state_.tactical_handover(context);if(boundary.contains("error"))return boundary["error"];
    t["last_human_strategy"]=t["human_strategy"];t.erase("human_strategy");
    if(boundary["phase"]==14){t["turn_boundary"]="battle_result";t["turn_reason"]=boundary["reason"];}
    else if(t["points"]==0){
        for(const auto *key:{"side","round","carry","status"})t[key]=boundary[key];t["points"]=std::min(40,boundary["points"].get<int>());
        if(boundary["phase"]==15)t["turn_boundary"]="time_limit";
        else{const auto &raw=next.state_.sram();const int owner=next.tactical_side()?raw[0xde3]>>4:raw[0xde3]&7;
            if(owner!=(raw[0xd89]&7)&&!((raw[0xd8a]&128)&&owner==(raw[0xd8a]&7)))t["turn_boundary"]="computer";
            const int selected=next.first_tactical_slot();if(selected<0)return "下一方没有可行动部队";t["selected"]=selected;
        }
    }
    t["moves"].push_back({{"kind","finish_tactical_strategy"}});*this=std::move(next);return {};
}
}
