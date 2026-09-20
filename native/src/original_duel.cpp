#include "zhongyuan/original_state.hpp"
#include <algorithm>

namespace zhongyuan {
namespace {
bool fields(const Json &v,std::initializer_list<const char *> names){
    if(!v.is_object())return false;
    for(auto name:names)if(!v.contains(name)||!v[name].is_number_integer()||v[name]<0||v[name]>255)return false;
    return true;
}
bool valid_units(const Json &units){
    if(!units.is_array()||units.size()!=66)return false;
    for(const auto &v:units)if(!v.is_number_integer()||v<0||v>255)return false;
    return true;
}
}
// 8F86-90FE after the 96-count attack animation. The original animation
// consumes its own random values; this kernel starts at the damage boundary.
Json OriginalState::duel_strike(Json &units,const Json &context,std::uint8_t &cursor) const {
    if(!valid_units(units)||!fields(context,{"first","second","side","command","flags","last_damage"})||
       context["first"]>=241||context["second"]>=241||context["first"]==context["second"]||
       (context["side"]!=0&&context["side"]!=128)||context["command"]>2)return {{"error","无效单挑攻击"}};
    const int side=context["side"],own=side?34:1,enemy=side?1:34;
    const int strength=bytes_[0x438+context[side?"second":"first"].get<int>()*8+3];
    const int opponent=bytes_[0x438+context[side?"first":"second"].get<int>()*8+3];
    auto next=units;auto next_cursor=cursor;int flags=context["flags"],damage=context["last_damage"],target=enemy;
    const int command=context["command"];std::string outcome="hit";
    auto random=[&](){return development_["random_sequence"][next_cursor++].get<int>();};
    if(command==0){
        // 90AD branches directly to the store for the left duelist. The
        // apparent ORA #2 is unreachable; preserve the observed asymmetry.
        if(side)flags|=1;
        damage=(strength/3)/4+1;
    }else if(command==1){
        int threshold=strength>=90?1:strength>=80?2:5;
        const int bit=side?1:2;
        if(flags&bit){flags&=~bit;threshold+=5;}
        if((random()&15)<threshold)outcome="miss";
        else damage=strength/4+1;
    }else if(random()&3){
        target=own;damage=opponent/2+opponent/4;outcome="counter";
    }else damage=strength/2+strength/4+1;
    if(outcome!="miss"){
        damage=std::min(damage,next[target].get<int>());next[target]=next[target].get<int>()-damage;
    }
    units=std::move(next);cursor=next_cursor;
    return {{"flags",flags},{"last_damage",damage},{"outcome",outcome},{"target_side",target==1?0:128}};
}
// 9325-93A2, after the human-controller check; frame selects a packed nibble.
Json OriginalState::duel_ai(const Json &units,const Json &context,std::uint8_t &cursor) const {
    if(!valid_units(units)||!fields(context,{"first","second","side","frame"})||
       context["first"]>=241||context["second"]>=241||context["first"]==context["second"]||
       (context["side"]!=0&&context["side"]!=128)||units[1]>100||units[34]>100)return {{"error","无效单挑电脑状态"}};
    const int side=context["side"],officer=context[side?"second":"first"];
    auto next_cursor=cursor;
    int command=rom_.duel_ai_choice(units[side?34:1],units[side?1:34],development_["random_sequence"][next_cursor++],context["frame"]);
    if(officer<6&&command==4)command=3;
    cursor=next_cursor;return {{"command",command}};
}
// 91BC-91F0 or 92F5-930F after the original result acknowledgements.
Json OriginalState::duel_finish(Json &units,Json &captives,const Json &context,std::uint8_t &cursor){
    if(!valid_units(units)||!fields(context,{"first","second","side","orientation","tactical_side","first_token","second_token","target"})||
       context["first"]>=241||context["second"]>=241||context["first"]==context["second"]||
       (context["side"]!=0&&context["side"]!=128)||(context["tactical_side"]!=0&&context["tactical_side"]!=128)||!context.contains("kind")||!context["kind"].is_string())return {{"error","无效单挑结算"}};
    const std::string kind=context.value("kind",std::string{});const int side=context["side"];
    auto next=*this;auto next_units=units,next_captives=captives,next_context=context;auto next_cursor=cursor;
    next_context["active"]=side;Json result;
    if(kind=="defeat"){
        if(units[side?0:33]!=255||units[side?1:34]!=255)return {{"error","单挑战败武将尚未离场"}};
        result=next.clash_general_defeat(context["first"],context["second"],side,context["orientation"],context["tactical_side"]);
        if(result.contains("error"))return result;
        result=next.finish_clash_defeat(next_units,next_context);
    }else if(kind=="surrender"){
        if(context[side?"second":"first"]<6)return {{"error","君主不能投降"}};
        const int stats_side=side^((context["orientation"].get<int>()&128)?0:128)^(context["tactical_side"].get<int>()?0:128);
        ++next.bytes_[0xdea+((stats_side&128)?1:0)];
        result=next.clash_surrender(next_units,next_captives,next_context,next_cursor);
    }else return {{"error","未知单挑结算"}};
    if(result.contains("error"))return result;
    *this=std::move(next);units=std::move(next_units);captives=std::move(next_captives);cursor=next_cursor;return result;
}
}
