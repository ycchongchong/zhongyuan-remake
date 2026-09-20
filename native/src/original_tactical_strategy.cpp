#include "zhongyuan/original_state.hpp"
#include <algorithm>
#include <cstdlib>
namespace zhongyuan {
// CE1E-CF2E: logical resolution after the separate CD76 mobility charge.
// Animation, dialogue and the frame-driven RNG between these entries are not
// synthesized. Both armies use the original ledger/occupancy representation.
Json OriginalState::resolve_tactical_strategy(int city,int side,int slot,int target_slot,int strategy,int frame,const Json &status,std::uint8_t &cursor){
    if(city<0||city>=30||(side!=0&&side!=128)||slot<0||slot>=(side?11:12)||target_slot<0||target_slot>=(side?12:11)||strategy<0||strategy>=8||frame<0||frame>255||!status.is_array()||status.size()!=24)return {{"error","无效战术计策执行参数"}};
    for(const auto &v:status)if(!v.is_number_integer()||v<0||v>255)return {{"error","无效战术状态效果"}};
    const int own=(side?0xdc2+slot*3:0xdaa+slot*2),other=(side?0xdaa+target_slot*2:0xdc2+target_slot*3);
    const int caster=bytes_[own],victim=bytes_[other],position=bytes_[own+1],cell=bytes_[other+1];
    if(caster>=241||victim>=241||position<16||position>=160||cell<0||cell>=160)return {{"error","计策部队或位置无效"}};
    const int token=bytes_[0xe1a+cell];
    if((token&63)!=((side?16:48)|target_slot))return {{"error","计策目标占用不一致"}};
    auto data=bytes_;auto effects=status;auto rng=cursor;const auto tables=rom_.tactical_strategy_tables();
    auto random=[&](){return development_["random_sequence"][rng++].get<int>();};
    auto percent=[&](){int v;do{v=random();}while(v>=100);return v;};
    Json result={{"success",false},{"strategy",strategy},{"slot",slot},{"target_slot",target_slot},{"target_cell",cell},{"officer",victim},{"cost",tables["cost"][strategy]},{"troops_lost",0},{"hp_lost",0},{"converted_slot",-1},{"status",effects}};
    auto finish=[&](bool success,const char *reason){result["success"]=success;result["reason"]=reason;result["status"]=effects;bytes_=std::move(data);cursor=rng;return result;};
    const int intelligence=data[0x438+caster*8+2];
    if(percent()>=intelligence)return finish(false,"intelligence");
    const int distance=std::min(5,std::max(std::abs((position&15)-(cell&15)),std::abs((position>>4)-(cell>>4))));
    if((random()&7)>=tables["distance"][distance].get<int>())return finish(false,"distance");
    auto remove_troop=[&](int officer){
        const int b=0x438+officer*8;if(!(data[b+6]&15))return;
        if(data[b+7]&240)data[b+7]-=16;else if(data[b+7]&15)--data[b+7];else data[b+4]-=16;
        data[b+6]=(data[b+6]&240)|((data[b+4]>>4)+(data[b+7]>>4)+(data[b+7]&15));
    };
    auto repaint=[&](int army,int unit){
        const int b=army?0xdc2+unit*3:0xdaa+unit*2;
        data[0xe1a+data[b+1]]=(army?48:16)|unit|((army&&(data[b+2]&128))?64:0);
    };
    const int b=0x438+victim*8;
    if(strategy==0||strategy==2||strategy==3){
        const int amount=std::min(data[b+6]&15,tables[strategy==0?"damage":"heavy_damage"][frame&7].get<int>());
        if(!amount)return finish(false,"no_troops");
        for(int i=0;i<amount;++i)remove_troop(victim);
        result["troops_lost"]=amount;repaint(side^128,target_slot);
    }else if(strategy==1){
        if(data[b+1]<30)return finish(false,"low_hp");
        const int amount=std::min<int>(data[b+1],(frame&15)+5);data[b+1]-=amount;result["hp_lost"]=amount;
    }else if(strategy==4||strategy==5){
        const int index=target_slot+(side?0:12),flags=effects[index];
        effects[index]=strategy==4?(flags&252)|3:(flags&243)|12;
    }else if(strategy==6){
        // D0BD deliberately does not clamp a nonzero shortfall. Its DEC/BEQ
        // branch returns without damage for insufficient troops.
        const int amount=tables["damage"][frame&7];int total=0;
        auto damage=[&](int encoded){
            if((encoded&48)!=(token&48))return true;
            const int army=(encoded&32)?128:0,unit=encoded&15;
            if(unit>=(army?11:12))return false;
            const int id=data[army?0xdc2+unit*3:0xdaa+unit*2];if(id>=241)return false;
            if((data[0x438+id*8+6]&15)<amount)return true;
            for(int i=0;i<amount;++i)remove_troop(id);
            total+=amount;return true;
        };
        for(int offset:{-16,16,1,-1})if(!damage(data[0xe1a+cell+offset]))return Json{{"error","计策连带目标无效"}};
        if(!total)return finish(false,"no_adjacent_losses");
        if(!damage(data[0xe1a+cell]))return Json{{"error","计策中心目标无效"}};
        result["troops_lost"]=total;repaint(side^128,target_slot);
    }else{
        if(victim<6)return finish(false,"ruler");
        const int loyalty=data[b+5];if(loyalty>=90)return finish(false,"loyalty");
        int chance=loyalty<31?80:loyalty<41?60:loyalty<51?40:loyalty<61?20:loyalty<71?10:0;
        if(intelligence>=95)chance+=(100-chance)/2;else if(intelligence>=90)chance+=(100-chance)/3;
        if(percent()>=chance)return finish(false,"conversion_roll");
        int empty=-1;for(int i=0;i<(side?11:12);++i)if(data[side?0xdc2+i*3:0xdaa+i*2]==255){empty=i;break;}
        if(empty<0)return finish(false,"army_full");
        int resident=-1;for(int i=0;i<12;++i)if(data[city*36+16+i]==(side?victim:255)){resident=i;break;}
        if(resident<0)return {{"error","计策转属城池名单不一致"}};
        const int destination=side?0xdc2+empty*3:0xdaa+empty*2;
        data[destination]=victim;data[destination+1]=cell;data[other]=255;data[other+1]=255;
        if(side)data[destination+2]=data[own+2]&127; // D202 copies the caster's flags.
        data[city*36+16+resident]=side?255:victim;repaint(side,empty);
        if(loyalty>=80)data[b+5]=20+(random()&3);
        else if(loyalty>=60)data[b+5]=25+(random()&7);
        else if(loyalty>=50)data[b+5]=67; // Literal LDA $E9D4 in this ROM.
        result["converted_slot"]=empty;
    }
    const int losses=result["troops_lost"];
    if(losses){const int address=0xde4+(side?2:0);const auto sum=static_cast<std::uint16_t>(data[address]+256*data[address+1]+losses);data[address]=sum&255;data[address+1]=sum>>8;}
    return finish(true,"applied");
}
}
