#include "zhongyuan/original_state.hpp"
#include <array>
#include <cstdlib>
namespace zhongyuan {
// A8D6-AAA3 and AAA4-AAD6, entered AFTER the first strategy attempt.
// Stop before A949's second B61D attempt; its RNG is a separate event.
Json OriginalState::tactical_ai_flank(int target,int slot,int points,const Json &status){
    const auto entry=tactical_ai_motion(target,slot,points,status);
    if(entry.contains("error"))return entry;
    if(entry["kind"]!="pending"||(entry["branch"]!="fort_flank"&&entry["branch"]!="cached_target"))return {{"error","当前不在守军绕行或缓存目标入口"}};
    Json result=entry;result["repeat_unit"]=false;
    const int current=entry["position"],fort=entry["fort"],cache=0xded+2*slot;
    const std::array<int,4> delta={-16,1,16,-1},directions={0,3,1,2}; // U,R,D,L
    auto valid=[](int cell,int d){return d==0?cell>=32:d==1?(cell&15)<15:d==2?cell<144:(cell&15)>0;};
    auto friendly=[&](int cell){return (bytes_[0xe1a+cell]&48)==16;};
    auto finish=[&](const char *kind){result["kind"]=kind;result["cached_target"]=bytes_[cache];return result;};
    int goal=bytes_[cache];
    if(goal!=255){
        result["target"]=goal;
        if(goal==current||friendly(goal)){bytes_[cache]=255;return finish("scan");}
        // Enemy arrival clears the cache but the old destination is still
        // used for this step, including the negative A0 continuation marker.
        if(bytes_[0xe1a+goal]&16)bytes_[cache]=255;
    }else{
        const bool horizontal=(current&15)!=(fort&15);
        bool candidate=false;for(int d:horizontal?std::array<int,2>{0,2}:std::array<int,2>{3,1})
            candidate|=valid(fort,d)&&!friendly(fort+delta[d]);
        if(!candidate)return finish("retry_strategy");
        std::array<int,4> distances={255,255,255,255};
        for(int d=0;d<4;++d){
            int cell=fort;for(int n=1;valid(cell,d);++n){cell+=delta[d];const int token=bytes_[0xe1a+cell];
                if(!(token&16))continue;
                if((token&32)==0&&(token&15)==slot)continue; // B1CB ignores this unit.
                if(token&32)distances[d]=n;
                break;
            }
            if(distances[d]==1)distances[d]=255;
        }
        const int difference=std::abs(current-fort);
        distances[current<fort?(difference<16?1:2):(difference<16?3:0)]=255;
        int best=-1,score=255;for(int d=0;d<4;++d)if(distances[d]<score){score=distances[d];best=d;}
        if(best<0)return finish("scan");
        goal=fort+delta[best];result["target"]=goal;
        if(goal==current)return finish("retry_strategy");
        bytes_[cache]=goal;
    }
    const int flags=status[slot],officer=bytes_[0x438+8*bytes_[0xdaa+2*slot]+6];
    // AA6E/B02C choose the first legal-cost direction in L,U,R,D order.
    for(int d:{3,0,1,2}){
        const bool toward=d==3?(goal&15)<(current&15):d==0?(goal>>4)<(current>>4):d==1?(goal&15)>(current&15):(goal>>4)>(current>>4);
        if(!toward)continue;
        const int cell=(current+delta[d])&255;
        if((bytes_[0xe1a+cell]&16)||(flags&3)||((flags&12)&&bytes_[0xf1a+current]==1)||points<rom_.tactical_ai_step_cost(officer,bytes_[0xf1a+cell]))continue;
        result["direction"]=directions[d];result["destination"]=cell;result["command"]=slot<<4;result["argument"]=directions[d];result["repeat_unit"]=true;
        return finish("move");
    }
    return finish("scan");
}
}
