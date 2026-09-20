#include "zhongyuan/original_state.hpp"
namespace zhongyuan {
// A2D7-A3C5 attacking branch. AE25 selects the defending anchor and clears
// stale defender roles while scanning. All writes commit only on valid input.
Json OriginalState::tactical_ai_attacker_plan(int target,int previous_slot,bool reuse_unit){
    if(target<0||target>=30||previous_slot<0||previous_slot>=11)return {{"error","无效进攻军选队参数"}};
    for(int side:{0,128})for(int slot=0;slot<(side?11:12);++slot){
        const int id=bytes_[(side?0xdc2:0xdaa)+slot*(side?3:2)];
        if(id>=241&&id!=255)return {{"error","无效战场军团名册"}};
    }
    int slot=previous_slot;
    if(!reuse_unit||bytes_[0xdc2+slot*3]==255){
        auto scan=tactical_ai_scan(128,slot,255);if(scan.contains("error"))return scan;
        if(scan["kind"]!="select")return {{"error","没有可选择的进攻部队"}};
        slot=scan["slot"];
    }
    const int current=bytes_[0xdc3+slot*3],fort=rom_.deployment_tables(target)["defenders"][0];
    if(current<16||current>=160||(bytes_[0xe1a+current]&63)!=(48|slot)||fort<16||fort>=160)return {{"error","无效进攻部队或城堡位置"}};
    auto next=*this;int anchor=-1;
    // ADE5 also scans our own commander before AE25 selects the enemy anchor.
    for(int i=10;i>=0;--i){
        if(bytes_[0xdc2+3*i]==255)next.bytes_[0xe04+2*i]=255;
        else if(bytes_[0xdc4+3*i]&128)break;
    }
    for(int i=11;i>=0;--i){
        const int id=bytes_[0xdaa+2*i];
        if(id==255){next.bytes_[0xdec+2*i]=255;continue;}
        if(id<6){const int cell=bytes_[0xdab+2*i];if(cell>=16&&cell<160)anchor=cell;break;}
    }
    // If no valid ruler, lower the loyalty threshold 60,50,...,0. Strict
    // score improvement is not used: equal strengths prefer the earlier slot.
    for(int threshold=60;anchor<0&&threshold>=0;threshold-=10){
        int best=0,chosen=-1;
        for(int i=11;i>=0;--i){
            const int id=bytes_[0xdaa+2*i],cell=bytes_[0xdab+2*i];
            if(id==255||cell<16||cell>=160)continue;
            const int b=0x438+id*8;if(bytes_[b+5]<threshold)continue;
            const int product=(bytes_[b+3]>>2)*((bytes_[b+6]&15)+1),score=product>=256?254:product;
            if(score>=best){best=score;chosen=cell;}
        }
        if(best&&chosen>=0)anchor=chosen;
    }
    if(anchor<0)anchor=fort;
    Json result={{"kind","role_plan"},{"slot",slot},{"position",current},{"fort",fort},{"enemy_commander",anchor},{"previous_slot",previous_slot}};
    if(reuse_unit)result["reuse_unit"]=true;
    *this=std::move(next);return result;
}
}
