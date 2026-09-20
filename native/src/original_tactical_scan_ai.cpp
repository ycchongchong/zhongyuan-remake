#include "zhongyuan/original_state.hpp"
namespace zhongyuan {
// A2DF-A330: continue the existing scan, without resetting marker $98 or
// repeating retreat/role assessment. FF means no scan origin yet (A0 reuse).
Json OriginalState::tactical_ai_scan(int side,int previous,int marker) const {
    const int count=side?11:12,base=side?0xdc2:0xdaa,stride=side?3:2;
    if((side!=0&&side!=128)||previous<0||previous>=count||(marker!=255&&(marker<0||marker>=count)))return {{"error","无效电脑部队轮询状态"}};
    for(int i=0;i<count;++i)if(bytes_[base+i*stride]>=241&&bytes_[base+i*stride]!=255)return {{"error","无效轮询军团名册"}};
    int slot=previous,origin=marker;
    // An initial FF marker can require count+1 visits if every slot is empty.
    for(int visited=0;visited<=count;++visited){
        slot=side?(slot+1)%count:(slot+count-1)%count;
        if(origin==255)origin=slot;
        else if(origin==slot)return {{"kind","end_turn"},{"slot",slot},{"marker",origin},{"previous_slot",previous},{"side",side},{"command",4}};
        if(bytes_[base+slot*stride]!=255)return {{"kind","select"},{"slot",slot},{"marker",origin},{"previous_slot",previous},{"side",side}};
    }
    return {{"error","电脑部队轮询未收敛"}};
}
}
