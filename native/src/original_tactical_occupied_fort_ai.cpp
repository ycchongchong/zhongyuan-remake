#include "zhongyuan/original_state.hpp"
#include <array>
namespace zhongyuan {
// AAD7-ACDE. DED is a direction/duration byte in this branch, not a cell.
// $21 is carried across AI helpers. -1 means unknown: accept only decisions
// whose SRAM, RNG and command are identical for zero/nonzero scratch values.
Json OriginalState::tactical_ai_occupied_fort(int target,int slot,int points,const Json &status,std::uint8_t &cursor,int scratch21){
    const auto entry=tactical_ai_motion(target,slot,points,status);
    if(entry.contains("error"))return entry;
    if(entry["kind"]!="pending"||entry["branch"]!="occupied_fort"||scratch21< -1||scratch21>255)return {{"error","当前不在敌军占堡后的守军行动入口"}};
    if(scratch21<0){
        auto zero=*this,nonzero=*this;auto c0=cursor,c1=cursor;
        auto a=zero.tactical_ai_occupied_fort(target,slot,points,status,c0,0),b=nonzero.tactical_ai_occupied_fort(target,slot,points,status,c1,1);
        const int s0=a.value("scratch21",-1),s1=b.value("scratch21",-1);a.erase("scratch21");b.erase("scratch21");
        if(a!=b||zero.sram()!=nonzero.sram()||c0!=c1)return {{"error","当前行动依赖尚未恢复的原版电脑临时状态，已保留进度"}};
        if(a.contains("error"))return a;
        bytes_=zero.bytes_;cursor=c0;a["scratch21"]=s0==s1?s0:-1;return a;
    }
    const int current=entry["position"],fort=entry["fort"],cache=0xded+2*slot;
    const int officer=bytes_[0xdaa+2*slot],b=0x438+8*officer,flags=status[slot];
    Json result=entry;result["command"]=-1;result["argument"]=-1;
    auto random=[&](){return development_["random_sequence"][cursor++].get<int>();};
    auto duration=[&](bool rotate){int n;do{n=random();if(rotate)n=(n<<1)&240;}while(n<48||n>=176);return n&240;};
    const std::array<int,4> delta={-1,-16,1,16},direction={2,0,3,1};
    auto valid=[&](int d){return d==0?(current&15)>0:d==1?current>=32:d==2?(current&15)<15:current<144;};
    auto cell=[&](int d){return (current+delta[d])&255;};
    auto cost=[&](int d){return rom_.tactical_ai_step_cost(bytes_[b+6],bytes_[0xf1a+cell(d)]);};
    auto movable=[&](int mask){for(int d=0;d<4;++d)if(mask&(1<<d)){
        if((bytes_[0xe1a+cell(d)]&16)||(flags&3)||((flags&12)&&bytes_[0xf1a+current]==1)||points<cost(d))mask&=~(1<<d);
    }return mask;};
    auto flip=[&](int mask){const int x=current&15;
        return ((mask&1)?(x==15?1:4):(x==0?4:1))|((mask&2)?(current>=144?2:8):(current<32?8:2));};
    auto strength=[&](int id){const int o=0x438+8*id,n=(bytes_[o+3]>>2)*((bytes_[o+6]&15)+1);return n>255?254:n;};
    auto finish=[&](const char *kind){result["kind"]=kind;result["cached_target"]=bytes_[cache];result["scratch21"]=scratch21;return result;};
    auto command=[&](const char *kind,int d){result["direction"]=direction[d];result["destination"]=cell(d);result["command"]=(slot<<4)|(std::string(kind)=="attack"?1:0);result["argument"]=direction[d];return finish(kind);};
    if(bytes_[cache]==255){
        const int mask=fort>=current?((current&15)<(fort&15)?3:6):((fort&15)>=(current&15)?9:12);
        bytes_[cache]=duration(true)|mask;
    }
    // AB29 explicitly discards the freshly generated duration before edges.
    int mask=bytes_[cache]&15;
    if((current&15)==0)mask=(mask&10)|4;else if((current&15)==15)mask=(mask&10)|1;
    if(current>=144)mask=(mask&5)|2;else if(current<32)mask=(mask&5)|8;
    bytes_[cache]=mask;
    for(int d=0;d<4;++d)if(!valid(d))mask&=~(1<<d);
    if(!mask)return finish("scan");
    int best=-1,score=255;
    for(int d=0;d<4;++d)if(mask&(1<<d)){
        const int token=bytes_[0xe1a+cell(d)];if((token&48)!=48)continue;
        const int value=strength(bytes_[0xdc2+3*(token&15)]);if(value<score){score=value;best=d;}
    }
    if(best>=0){
        const int profile=((bytes_[b+1]<60?2:0)+(strength(officer)<score?1:0))*2;
        int x=profile;result["strategy_threshold"]=rom_.tactical_occupied_fort_threshold(profile,false);
        if(random()<result["strategy_threshold"].get<int>()){
            const auto strategy=tactical_ai_strategy(target,slot,points,status,cursor,true);
            if(strategy.contains("error"))return strategy;
            if(strategy["kind"]=="strategy"){
                for(const auto &[key,value]:strategy.items())result[key]=value;
                scratch21=strategy["strategy"];return finish("strategy");
            }
            // B634 changes X even when no mobility; a full B69B search
            // finishes with X=48 and $21=47. AC19 does NOT restore profile.
            x=points<4?0:48;
            if(points>=4)scratch21=47;
        }
        if(points>=8){
            result["attack_gate_index"]=x;result["attack_threshold"]=rom_.tactical_occupied_fort_threshold(x,true);
            if(random()<result["attack_threshold"].get<int>())return command("attack",best);
        }
    }
    mask=movable(bytes_[cache]&15);
    if(!mask){
        mask=flip(mask);bytes_[cache]=duration(false)|mask;mask=movable(mask);
        if(!mask)return finish("scan");
    }
    int low=bytes_[cache]&15,remaining=(bytes_[cache]&240)-16;
    if(remaining<=0){if(!scratch21)low=flip(mask);remaining=duration(false);}
    bytes_[cache]=remaining|low;
    if((mask&(mask-1))==0){for(int d=0;d<4;++d)if(mask&(1<<d))return command("move",d);}
    best=-1;score=255;
    for(int d=0;d<4;++d)if((mask&(1<<d))&&valid(d)&&cost(d)<score){best=d;score=cost(d);}
    return best<0?finish("scan"):command("move",best);
}
}
