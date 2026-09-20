#include "zhongyuan/original_state.hpp"
#include <algorithm>
namespace zhongyuan {
// A1D9-A2D6: assess retreat and update a defender role before A2D7 begins
// unit/path planning. This explicit boundary must not be treated as an end turn.
Json OriginalState::tactical_ai_assessment(int side,int round,int points,std::uint8_t &cursor){
    if((side!=0&&side!=128)||round<0||round>255||points<0||points>40||(bytes_[0xde3]&15)>=6||(bytes_[0xde3]>>4)>=6)
        return {{"error","无效电脑战术评估状态"}};
    for(int team:{0,128})for(int slot=0;slot<(team?11:12);++slot){
        const int id=bytes_[(team?0xdc2:0xdaa)+slot*(team?3:2)];
        if(id>=241&&id!=255)return {{"error","无效电脑战术军团"}};
    }
    const int owner=side?bytes_[0xde3]>>4:bytes_[0xde3]&15;
    int cities=0;for(int city=0;city<30;++city)cities+=(bytes_[city*36]&7)==owner;
    Json result={{"kind","plan"},{"slot",-1},{"score",-1},{"role_slot",-1},{"side",side}};
    if(cities>=2&&points>0&&(!side||round>=4)){
        // AD03: quantized hp * martial * troops. When total >= 256 the
        // original division uses only its high byte, not an exact percentage.
        unsigned strength[2]={};
        for(int team:{0,1})for(int slot=0;slot<(team?11:12);++slot){
            const int id=bytes_[(team?0xdc2:0xdaa)+slot*(team?3:2)];if(id==255)continue;
            const int b=0x438+id*8;
            strength[team]+=std::max(1,bytes_[b+1]>>4)*std::max(1,bytes_[b+3]>>4)*std::max(1,bytes_[b+6]&15);
        }
        const unsigned total=(strength[0]+strength[1])&65535,enemy=strength[side?0:1];
        const unsigned divisor=(total>>8)?total>>8:total,numerator=(total>>8)?enemy:enemy<<8;
        const int score=std::min(255u,divisor?numerator/divisor:65535u);result["score"]=score;
        if(!side&&bytes_[0xd88]){
            // The ruler search checks slots 0..10 and requests slot ZERO,
            // where automatic deployment normally places the ruler.
            for(int slot=10;slot>=0;--slot)if(bytes_[0xdaa+slot*2]<6){
                result["kind"]="retreat";result["slot"]=0;return result;
            }
        }
        // A221 tests the sign of 06A7 after the defender-only branch: its
        // nominal whole-army probability path is unreachable for legal sides.
        if(score>=120){
            const int base=side?0xdc2:0xdaa,stride=side?3:2;
            for(int slot=side?10:11;slot>=0;--slot){
                const int id=bytes_[base+slot*stride];if(id==255)continue;const int b=0x438+id*8;
                if(bytes_[b+1]>=40||(bytes_[b+6]&15)>=3)continue;
                if(development_["random_sequence"][cursor++].get<int>()>=204)continue;
                result["kind"]="retreat";result["slot"]=slot;return result;
            }
        }
    }
    if(!side){
        int count=0;for(int slot=11;slot>=0;--slot)if(bytes_[0xdaa+slot*2]!=255&&bytes_[0xdec+slot*2]==2)++count;
        if(count<4)for(int slot=11;slot>=0;--slot){
            if(bytes_[0xdaa+slot*2]==255||bytes_[0xdec+slot*2]==2||bytes_[0xdec+slot*2]==15)continue;
            bytes_[0xdec+slot*2]=2;result["role_slot"]=slot;break;
        }
    }
    return result;
}
// A2D7-A3C4, defender side: normal descending scan or negative-A0
// same-unit reuse, followed by commander/fort setup and vacant-fort move.
// Stop at A3C5 if role-specific planning is required; never skip that unit.
Json OriginalState::tactical_ai_fort(int target,int previous_slot,int points,const Json &status,bool reuse_unit){
    if(target<0||target>=30||previous_slot<0||previous_slot>=12||points<0||points>40||!status.is_array()||status.size()!=24)
        return {{"error","无效电脑城堡补位状态"}};
    for(const auto &v:status)if(!v.is_number_integer()||v<0||v>255)return {{"error","无效战术状态效果"}};
    int slot=reuse_unit?(previous_slot+1)%12:previous_slot,selected=-1;
    for(int i=0;i<12;++i){slot=(slot+11)%12;const int id=bytes_[0xdaa+slot*2];
        if(id>=241&&id!=255)return {{"error","无效守军名册"}};
        if(id!=255){selected=slot;break;}}
    if(selected<0)return {{"error","没有可选择的守军"}};
    const int current=bytes_[0xdab+selected*2];if(current<16||current>=160)return {{"error","无效守军位置"}};
    int commander=-1;
    for(int i=10;i>=0;--i){const int id=bytes_[0xdc2+i*3];
        if(id>=241&&id!=255)return {{"error","无效进攻军名册"}};
        if(id!=255&&(bytes_[0xdc4+i*3]&128)){commander=i;break;}}
    if(commander<0||bytes_[0xdc3+commander*3]<16||bytes_[0xdc3+commander*3]>=160)return {{"error","没有有效进攻主将"}};
    const int fort=rom_.deployment_tables(target)["defenders"][0];
    if(fort<16||fort>=160)return {{"error","无效原版城堡位置"}};
    // ADF3 clears stale attacker roles only while scanning down to the leader.
    for(int i=10;i>commander;--i)if(bytes_[0xdc2+i*3]==255)bytes_[0xe04+i*2]=255;
    Json result={{"kind","role_plan"},{"slot",selected},{"position",current},{"fort",fort},
        {"enemy_commander",bytes_[0xdc3+commander*3]},{"direction",-1},{"previous_slot",previous_slot}};
    if(reuse_unit)result["reuse_unit"]=true;
    const int flags=status[selected];
    if((flags&3)||((flags&12)&&bytes_[0xf1a+current]==1)||(bytes_[0xe1a+fort]&16))return result;
    const int dx=std::abs((fort&15)-(current&15)),dy=std::abs((fort>>4)-(current>>4));
    if(dx+dy>=2)return result;
    const int troops=bytes_[0x438+bytes_[0xdaa+selected*2]*8+6]&15;
    if(points<=(troops<4?2:troops<7?4:6))return result;
    for(int i=0;i<12;++i)if(bytes_[0xdec+i*2]==12)bytes_[0xded+i*2]=255;
    bytes_[0xdec+selected*2]=15;
    // Original direction bytes: 0 up, 1 down, 2 left, 3 right.
    result["kind"]="move";result["direction"]=(dx?2:0)+(fort>=current?1:0);
    return result;
}

}
