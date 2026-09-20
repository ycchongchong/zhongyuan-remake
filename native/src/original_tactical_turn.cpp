#include "zhongyuan/original_state.hpp"
#include <algorithm>
namespace zhongyuan {
namespace {
bool valid_turn(const Json &c){
    if(!c.is_object())return false;
    for(const auto *key:{"side","points","round","carry","phase","counter","reason"})
        if(!c.contains(key)||!c[key].is_number_integer()||c[key]<0||c[key]>255)return false;
    if(c["side"]!=0&&c["side"]!=128)return false;
    if(!c.contains("status")||!c["status"].is_array()||c["status"].size()!=24)return false;
    for(const auto &v:c["status"])if(!v.is_number_integer()||v<0||v>255)return false;
    return true;
}
}
// C6EC-C723, after accepting the player's end-turn menu item.
Json OriginalState::tactical_end_turn(const Json &context) const {
    if(!valid_turn(context)||context["phase"]!=5||context["counter"]!=8||context["points"]>40)return {{"error","无效战术结束状态"}};
    auto next=context;const int saved=std::min(10,context["points"].get<int>()),carry=context["carry"];
    next["carry"]=context["side"]==128?(carry&15)|(saved<<4):(carry&240)|saved;
    next["points"]=0;next["phase"]=5;next["counter"]=1;return next;
}
// C489-C544. Presentation/input calls are folded; no random draw or SRAM write.
Json OriginalState::tactical_handover(const Json &context) const {
    if(!valid_turn(context)||context["phase"]!=5||context["counter"]!=1||context["points"]!=0)return {{"error","请先结束当前战术行动"}};
    auto next=context;const int defender=bytes_[0xde3]&7,attacker=bytes_[0xde3]>>4;
    if(defender>=6||attacker>=6)return {{"error","无效战斗势力"}};
    int count[2]={};bool commander=false;
    // BEFE/BF2E count eleven slots on BOTH sides, even with twelve defenders.
    for(int i=0;i<11;++i){count[0]+=bytes_[0xdaa+i*2]!=255;count[1]+=bytes_[0xdc2+i*3]!=255;
        commander|=bytes_[0xdc2+i*3]!=255&&(bytes_[0xdc4+i*3]&128);}
    int reason=0;
    if(bytes_[0x438+defender*8]&32)reason=0x83;
    else if(bytes_[0x438+attacker*8]&32)reason=0x94;
    else if(!count[0])reason=3;
    else if(!commander)reason=0x14;
    if(reason){next["phase"]=14;next["counter"]=0;next["reason"]=reason;return next;}
    const int side=context["side"];
    if(!side){
        const int round=(context["round"].get<int>()+1)&255;next["round"]=round;
        if(round>=11){next["phase"]=15;next["counter"]=0;return next;}
        // C0FE repeats BIT $01 (mask 0C), even before subtracting 10 and 40.
        // It touches only the first 20 of 24 bytes. Preserve these ROM quirks.
        for(int i=0;i<20;++i){int value=next["status"][i];
            if(value&3)value=(value-1)&255;
            for(int amount:{4,16,64})if(value&12)value=(value-amount)&255;
            next["status"][i]=value;
        }
    }
    const int following=side^128,carry=context["carry"],bonus=following?carry>>4:carry&15;
    next["side"]=following;next["points"]=rom_.tactical_mobility(count[following?1:0])+bonus;
    next["carry"]=following?carry&15:carry&240;next["counter"]=2;return next;
}
}
