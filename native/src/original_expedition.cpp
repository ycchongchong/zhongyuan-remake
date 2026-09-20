#include "zhongyuan/original_state.hpp"
#include <algorithm>
#include <set>
namespace zhongyuan {
Json OriginalState::collect_defenders(int target) {
    if(target<0||target>=30)throw std::out_of_range("Defending city");
    auto next=bytes_;
    for(int slot=0;slot<12;++slot){
        const int id=next[target*36+16+slot];if(id==255)continue;
        if(id>=241)throw std::invalid_argument("Invalid defending officer");
        bool present=false;int empty=-1;
        for(int i=0;i<12;++i){
            const int entry=next[0xdaa+i*2];
            if(entry==id)present=true;
            if(entry==255&&empty<0)empty=i;
        }
        if(present)continue;
        // Original input guarantees space. Reject malformed ledgers atomically
        // instead of letting B73D walk beyond the twelve defending records.
        if(empty<0)throw std::invalid_argument("Defender ledger is full");
        next[0xdaa+empty*2]=id;
    }
    Json result=Json::array();
    for(int i=0;i<12;++i)if(next[0xdaa+i*2]!=255)result.push_back(next[0xdaa+i*2]);
    bytes_=std::move(next);return result;
}
Json OriginalState::expedition_quote(int source,int target,const std::vector<int> &officers,int leader) const {
    const auto fail=[](const char *message){return Json{{"error",message}};};
    if(source<0||source>=30||target<0||target>=30||source==target)return fail("请选择其他相邻城池");
    const int ruler=bytes_[0xd8b],base=source*36;
    if((bytes_[base]&7)!=ruler)return fail("只能从己方城池出征");
    if((bytes_[target*36]&7)==ruler)return fail("不能攻打己方城池");
    if(!bytes_[0xd8f+4*ruler])return fail("命令书已用完");
    bool adjacent=false;for(const auto &edge:roads_)adjacent|=(edge[0]==source&&edge[1]==target)||(edge[1]==source&&edge[0]==target);
    if(!adjacent)return fail("目标城池不相邻");
    for(int i=0;i<8;++i)if((bytes_[0xc8d+source*8+i]&31)==target)return fail("当前外交约定禁止攻打该城");
    const std::set<int> selected(officers.begin(),officers.end());
    if(selected.empty()||selected.size()!=officers.size()||selected.size()>11)return fail("请选择不重复的出征武将");
    if(!selected.count(leader))return fail("主将必须属于出征部队");
    int count=0,cost=0;
    for(int i=0;i<12;++i)count+=bytes_[base+16+i]!=255;
    if(int(selected.size())>=count)return fail("出发城必须留守至少一名武将");
    for(int id:selected){if(id<0||id>=241||!resident(source,id))return fail("请选择出发城的驻城武将");cost+=(bytes_[0x438+id*8+6]&15)*20;}
    // B830-B865: twenty gold per hundred soldiers, including a zero-cost sortie.
    const int gold=bytes_[base+1]+(bytes_[base+2]<<8)+(bytes_[base+3]<<16);
    if(gold<cost)return fail("黄金不足，出征每百名士兵需要二十金");
    return {{"kind","player_expedition"},{"source",source},{"target",target},{"officers",officers},{"leader",leader},{"cost",cost}};
}
Json OriginalState::dispatch_expedition(int source,int target,const std::vector<int> &officers,int leader){
    auto result=expedition_quote(source,target,officers,leader);if(result.contains("error"))return result;
    const std::set<int> selected(officers.begin(),officers.end());auto next=bytes_;
    const int base=source*36,ruler=next[0xd8b],cost=result["cost"];
    const int gold=next[base+1]+(next[base+2]<<8)+(next[base+3]<<16);
    for(int i=0;i<3;++i)next[base+1+i]=(gold-cost)>>(i*8);
    // B799 clears the shared ledger; B980 removes selected residents in reverse
    // ordinal order and BE50 marks the chosen commander in the source-city byte.
    std::fill(next.begin()+0xdaa,next.begin()+0xdaa+65,255);
    int entry=0,adviser=255,comparison=255;
    for(int slot=11;slot>=0;--slot){
        const int id=next[base+16+slot];if(!selected.count(id))continue;
        next[base+16+slot]=255;
        const int b=0xdaa+24+entry*3;next[b]=id;next[b+2]=source|(id==leader?128:0);++entry;
        // Preserve B9A6-B9C3's asymmetric first-record assignment.
        const int knowledge=next[0x438+id*8+2];
        if(comparison==255){adviser=knowledge;comparison=id;}
        else if(knowledge>=comparison){adviser=id;comparison=knowledge;}
    }
    next[0xdaa+57]=(ruler<<4)|(next[target*36]&7);
    next[target*36+32]=source;next[target*36+33]=ruler;
    std::fill(next.begin()+0xdaa+58,next.begin()+0xdaa+66,0);
    const int control=next[base+14]+(next[base+15]<<8),reduced=std::max(0,control-5);
    next[base+14]=reduced&255;next[base+15]=reduced>>8;
    ++next[0xd8c];--next[0xd8f+4*ruler];bytes_=std::move(next);
    result["reference_adviser"]=adviser;return result;
}
}
