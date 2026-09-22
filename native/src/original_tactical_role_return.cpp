#include "zhongyuan/original_session.hpp"
namespace zhongyuan {
// A2D6 -> A1D3 leaves command 06A7=0 and preserves 06A8. The tactical
// dispatcher therefore requests slot-zero movement using the inherited low
// two direction bits. C7EF returns to C761 even if the move is rejected.
Json OriginalState::tactical_role_return(int argument,int points,const Json &status){
    if(argument < -1||argument>255||points<1||points>40||!status.is_array()||status.size()!=24)
        return {{"error","无效角色调整续行参数"}};
    for(const auto &v:status)if(!v.is_number_integer()||v<0||v>255)return {{"error","无效战术状态效果"}};
    if(bytes_[0xdaa]>=241||bytes_[0xdab]<16||bytes_[0xdab]>=160)
        return {{"error","守军第 1 队已离场，原版残留命令的空槽行为尚未接入"}};
    if(argument==-1){
        Json common;std::vector<std::uint8_t> raw;
        for(int direction=0;direction<4;++direction){
            auto trial=*this;auto result=trial.tactical_role_return(direction,points,status);
            if(result.contains("error"))return result;
            result["argument"]=-1;result["direction"]=-1;
            if(direction==0){common=result;raw=trial.sram();}
            else if(result!=common||trial.sram()!=raw)return {{"error","角色调整沿用的方向尚未确定，请保留当前进度"}};
        }
        bytes_=std::move(raw);return common;
    }
    const int before=points,position=bytes_[0xdab],directions[]={3,2,1,0};
    bool moved=false;
    if((status[0].get<int>()&15)==0)moved=tactical_step(0,directions[argument&3],points,0).empty();
    return {{"kind",(status[0].get<int>()&15)?"waiting":moved?"move":"blocked"},{"branch","role_return"},{"slot",0},
        {"argument",argument},{"direction",argument&3},{"position",position},{"destination",bytes_[0xdab]},
        {"points_before",before},{"points_after",points}};
}
// C7EF still reads the absent officer FF through EE45 (6C36), and uses
// C83A/C8A9/C8B7/C8CA's direction-specific bounds, not a general map bound.
// On success only the ledger position changes: BA32 sees officer FF and
// returns before painting occupancy. Keep this quirk out of tactical_step.
Json OriginalState::tactical_empty_role_return(int argument,int points,const Json &status){
    if(argument < -1||argument>255||points<1||points>40||bytes_[0xdaa]!=255||!status.is_array()||status.size()!=24)
        return {{"error","无效空槽位续行参数"}};
    for(const auto &v:status)if(!v.is_number_integer()||v<0||v>255)return {{"error","无效战术状态效果"}};
    if(argument==-1){
        Json common;std::vector<std::uint8_t> raw;
        for(int direction=0;direction<4;++direction){
            auto trial=*this;auto result=trial.tactical_empty_role_return(direction,points,status);
            if(result.contains("error"))return result;
            result["argument"]=-1;result["direction"]=-1;
            if(direction==0){common=result;raw=trial.sram();}
            else if(result!=common||trial.sram()!=raw)return {{"error","空槽位沿用的方向尚未确定，请保留当前进度"}};
        }
        bytes_=std::move(raw);return common;
    }
    const int before=points,position=bytes_[0xdab],direction=argument&3;
    const int delta[]={-16,16,-1,1};const int destination=(position+delta[direction])&255;
    const bool allowed=direction==0?(position&240)>=32:direction==1?(position&240)<144:direction==2?(position&15)>0:(position&15)<15;
    bool moved=false;
    if((status[0].get<int>()&15)==0&&allowed&&!(bytes_[0xe1a+destination]&16)){
        const int cost=rom_.tactical_step_cost(bytes_[0xc36],bytes_[0xe1a+destination]);
        if(points>=cost){points-=cost;bytes_[0xdab]=destination;moved=true;}
    }
    return {{"kind",(status[0].get<int>()&15)?"waiting":moved?"move":"blocked"},{"branch","empty_role_return"},{"slot",0},{"empty_slot",true},
        {"argument",argument},{"direction",direction},{"position",position},{"destination",bytes_[0xdab]},
        {"points_before",before},{"points_after",points}};
}
std::string OriginalSession::continue_computer_role(){return continue_computer_role_impl(false);}
std::string OriginalSession::continue_empty_computer_role(){return continue_computer_role_impl(true);}
std::string OriginalSession::continue_computer_role_impl(bool empty_slot){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有守军角色调整";
    const auto &old=battle_["tactics"];
    if(tactical_side()!=0||old.value("turn_boundary",std::string{})!="computer"||old.contains("attack")||old.contains("retreat")||old.contains("scout")||old.contains("strategy_result")||old.contains("computer_plan")||
       !old.contains("computer")||old["computer"]["kind"]!="plan"||old["computer"].value("role_slot",-1)<0)return "请先完成守军角色调整评估";
    if(old["moves"].size()>=1024)return "当前行动记录已满";
    auto next=*this;auto &t=next.battle_["tactics"];
    const int argument=next.track_argument_?next.persistent_argument_:next.computer_argument_;
    auto result=empty_slot?next.state_.tactical_empty_role_return(argument,t["points"],t["status"]):next.state_.tactical_role_return(argument,t["points"],t["status"]);
    if(result.contains("error"))return result["error"];
    if(result["kind"]=="waiting")return "守军第 1 队受状态限制，原版仍停留在移动等待阶段";
    next.computer_scratch21_=-1;
    // 9B is the AI scan cursor, independent of the stale command's slot zero.
    t["selected"]=0;t["points"]=result["points_after"];t["computer"]["kind"]="acted";
    t["computer_motion"]=std::move(result);
    t["moves"].push_back({{"kind",empty_slot?"computer_empty_role_return":"computer_role_return"}});
    *this=std::move(next);return {};
}
bool OriginalSession::can_continue_computer_role() const {
    auto trial=*this;
    return trial.perform_battle_action([](auto &game){
        auto error=game.continue_computer_role();
        if(error.empty())return error;
        error=game.continue_empty_computer_role();
        return error.empty()?error:game.resume_computer_role_after_handover();
    }).empty();
}
// End-turn leaves the known AI argument in 06A8. Recover the replay-derived
// pre-handover value, or the separately verified underfunded-attack history.
// Other unknown histories remain held rather than inventing a direction.
// A separate event preserves the meaning of older role-return save events.
std::string OriginalSession::resume_computer_role_after_handover(){
    if(phase_!="expedition"||!battle_.is_object()||!battle_.contains("tactics"))return "当前没有待恢复的守军角色调整";
    const auto &old=battle_["tactics"];
    if(old.value("turn_boundary",std::string{})!="computer"||!old.contains("computer")||old["computer"].value("role_slot",-1)<0)
        return "当前没有可核对的回合交接方向";
    std::vector<const Json *> events;
    if(old.contains("history"))for(const auto &segment:old["history"])for(const auto &event:segment)events.push_back(&event);
    for(const auto &event:old["moves"])events.push_back(&event);
    if(events.empty()||events.back()->value("kind",std::string{})!="computer_tactics")return "请先完成当前守军角色评估";
    events.pop_back(); // The current role assessment preserves 06A8.
    auto resume=[&](int direction){
        auto next=*this;next.computer_argument_=direction;
        auto error=next.continue_computer_role_impl(next.state_.sram()[0xdaa]==255);
        if(!error.empty())return error;
        next.battle_["tactics"]["moves"].back()["kind"]="computer_role_after_handover";
        *this=std::move(next);return std::string{};
    };
    // Keep the legacy clearing behavior, but recover a known replay-derived
    // argument when the immediately preceding event is this turn's handover.
    if(!events.empty()&&events.back()->value("kind",std::string{})=="end_tactical_turn"&&handover_argument_>=0)
        return resume(handover_argument_);
    if(!old.contains("last_exhausted_attack"))return "上次电脑指令没有可核对的方向";
    const auto &last=old["last_exhausted_attack"];
    const int direction=last.value("direction",-1),points=last.value("points_before",-1);
    if(direction<0||direction>3||points<1||points>2)return "上次电脑攻击记录无效";
    bool found=false;int handovers=0;
    for(auto it=events.rbegin();it!=events.rend();++it){
        const auto kind=(**it).value("kind",std::string{});
        if(kind=="computer_exhausted_attack"){found=true;break;}
        if(kind=="end_tactical_turn"){++handovers;continue;}
        if(kind.empty()||kind=="attack"||kind=="confirm_attack"||kind=="begin_clash"||kind=="clash_order"||
           kind=="advance_clash"||kind=="advance_surrender"||kind=="advance_clash_defeat"||kind=="finish_clash_result")continue;
        return "该交接路径的原版方向暂存尚未核对";
    }
    if(!found||handovers!=1)return "没有找到完整的电脑攻击至玩家回合交接记录";
    return resume(direction);
}
}
