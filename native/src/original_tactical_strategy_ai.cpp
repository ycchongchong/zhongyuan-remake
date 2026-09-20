#include "zhongyuan/original_state.hpp"
#include <algorithm>
#include <cstdlib>
namespace zhongyuan {
// Defender A3E8 role entry through B614/B61D strategy selection. No effects
// or subsequent path planning are executed here. Preserve original RNG order.
// direct=true is the A949 retry: B61D without role setup/probability gating.
Json OriginalState::tactical_ai_strategy(int target,int slot,int points,const Json &status,std::uint8_t &cursor,bool direct,int *scratch21,int side,int round){
    if((side!=0&&side!=128)||round<0||round>255||target<0||target>=30||slot<0||slot>=(side?11:12)||points<0||points>40||!status.is_array()||status.size()!=24)
        return {{"error","无效电脑计策评估状态"}};
    for(const auto &v:status)if(!v.is_number_integer()||v<0||v>255)return {{"error","无效战术状态效果"}};
    const int base=side?0xdc2:0xdaa,stride=side?3:2,enemy_base=side?0xdaa:0xdc2,enemy_stride=side?2:3;
    const int friendly=side?48:16,hostile=side?16:48;
    const int id=bytes_[base+slot*stride],current=bytes_[base+1+slot*stride];
    if(id>=241||current<16||current>=160||(bytes_[0xe1a+current]&63)!=(friendly|slot))return {{"error","无效施计守军位置"}};
    for(int cell=0;cell<256;++cell){const int token=bytes_[0xe1a+cell];
        if((token&48)==hostile&&((token&15)>=(side?12:11)||bytes_[enemy_base+(token&15)*enemy_stride]>=241))return {{"error","无效敌军占用记录"}};
    }
    const int role=bytes_[(side?0xe04:0xdec)+slot*2],fort=rom_.deployment_tables(target)["defenders"][0];
    Json result={{"kind","no_strategy"},{"slot",slot},{"role",role},{"threshold",-1},{"mask",0},{"strategy",-1},{"target_slot",-1},{"target_cell",-1},{"command",-1},{"argument",-1}};
    if(!side&&!direct&&role!=15&&(role&3)==2){
        // A758: a hostile occupation of the fort diverts to AAD7 before RNG.
        if((bytes_[0xe1a+fort]&48)==48){result["kind"]="role_reassignment";return result;}
        const int previous=bytes_[0xded+slot*2];
        if(std::abs((fort&15)-(previous&15))>=2||std::abs((fort>>4)-(previous>>4))>=2)bytes_[0xded+slot*2]=255;
    }
    auto random=[&](){return development_["random_sequence"][cursor++].get<int>();};
    const int intelligence=bytes_[0x438+id*8+2];
    if(side&&!direct&&round>=9)return result; // A3CF skips strategy after round index eight.
    if(!direct&&(side||role!=15)){
        // B8E1 uses floor(intelligence*256/100), saturates to 255, then
        // rounds the scaled probability before adding 25.
        const int factor=side?(round==8?25:round==7?51:102):((role&1)?102:76);
        const int threshold=25+(std::min(255,intelligence*256/100)*factor+128)/256;
        result["threshold"]=threshold;
        if(random()>=threshold)return result;
    }
    int mask=points<4?0:points<5?0x31:points<6?0x33:points<9?0x3f:points<10?0x7f:0xff;
    mask&=intelligence<40?0x13:intelligence<60?0x1f:intelligence<80?0x3d:0xff;
    result["mask"]=mask;if(!mask)return result;
    auto enemy=[&](int cell){return (bytes_[0xe1a+cell]&48)==hostile;};
    for(int index=0;index<48;++index){
        if(scratch21)*scratch21=index; // B7FB saves every search index, even invalid cells.
        const auto delta=rom_.tactical_strategy_offset(index);
        const int y=(current>>4)+delta[0],x=(current&15)+delta[1];
        // B803 permits row zero on an upward offset; do not normalize it.
        if(y<0||y>=10||x<0||x>=16)continue;
        const int cell=y*16+x;if(!enemy(cell)||cell==fort)continue;
        const int terrain=bytes_[0xf1a+cell];
        const int terrain_mask=terrain==0?0x80:terrain==1?0xa4:terrain<4?0xda:terrain==4?0xdb:terrain==5?0x81:terrain==6?0x80:0;
        int choices=mask&terrain_mask;
        const int target_slot=bytes_[0xe1a+cell]&15,officer=bytes_[enemy_base+target_slot*enemy_stride],b=0x438+officer*8;
        if((bytes_[b+6]&15)<4)choices&=0xb2;
        if(bytes_[b+1]<50)choices&=0xfd;
        if(bytes_[b+5]>=50)choices&=0x7f;
        if(choices&64){
            bool adjacent=(cell>=32&&enemy(cell-16))||(x<15&&enemy(cell+1))||(cell+16<160&&enemy(cell+16));
            // B761 jumps directly to B771 on the left border, retaining bit64.
            if(!adjacent&&x>0&&!enemy(cell-1))choices&=0xbf;
        }
        const int flags=status[(side?0:12)+target_slot];
        if(flags&3)choices&=0xef;
        // The original draws even when bit32 is already absent or choices=0.
        if((flags&12)||random()>=76)choices&=0xdf;
        if(!choices)continue;
        int strategy=-1;for(int bit:{7,6,3,2,1,0,4,5})if(choices&(1<<bit)){strategy=bit;break;}
        if(scratch21)*scratch21=strategy; // B7D8 replaces the saved search index.
        result["kind"]="strategy";result["strategy"]=strategy;result["target_slot"]=target_slot;result["target_cell"]=cell;
        result["command"]=slot*16+2;result["argument"]=target_slot*8+strategy;result["candidate_mask"]=choices;result["search_index"]=index;
        return result;
    }
    return result;
}
}
