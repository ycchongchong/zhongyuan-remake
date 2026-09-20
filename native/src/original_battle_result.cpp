#include "zhongyuan/original_state.hpp"
#include <algorithm>
namespace zhongyuan {
// AFA8: the failure screen holds until a living human player can continue.
// Both A and B are accepted from either enabled controller; no RNG/SRAM writes.
Json OriginalState::human_failure_step(int loser,int winner,int inputs) const {
    const int first=bytes_[0xd89]&7,second=(bytes_[0xd8a]&128)?bytes_[0xd8a]&7:-1;
    if(first>=6||second>=6||first==second||loser<0||loser>=6||winner<0||winner>=6||loser==winner||
       (loser!=first&&loser!=second)||!(bytes_[0x438+loser*8]&32)||inputs<0||inputs>3)
        return {{"error","无效玩家君主败北记录"}};
    const bool continuing=!(bytes_[0x438+first*8]&32)||(second>=0&&!(bytes_[0x438+second*8]&32));
    return {{"loser",loser},{"winner",winner},{"can_continue",continuing},
        {"phase",continuing&&(inputs&(second>=0?3:1))?11:16}};
}
// DA30: after the ruler-defeat battle report, visit every city once.
Json OriginalState::annex_ruler_city(int city,int loser,int winner,std::uint8_t &cursor){
    if(city<0||city>=30||loser<0||loser>=6||winner<0||winner>=6||loser==winner||bytes_[0xd8b]>=6)
        return {{"error","无效君主败北领地记录"}};
    const int b=city*36;const bool changed=(bytes_[b]&7)==loser;
    if(changed)for(int i=0;i<12;++i){const int id=bytes_[b+16+i];if(id>=241&&id!=255)return {{"error","无效败方驻将名册"}};}
    const auto before=std::vector<std::uint8_t>(bytes_.begin()+b,bytes_.begin()+b+36);
    if(changed){
        bytes_[b]=(bytes_[b]&248)|winner;
        for(int i=0;i<12;++i){
            const int id=bytes_[b+16+i];if(id==255)continue;auto &loyalty=bytes_[0x438+id*8+5];
            if(loyalty>=80)loyalty=20+(development_["random_sequence"][cursor++].get<int>()&3);
            else if(loyalty>=60)loyalty=25+(development_["random_sequence"][cursor++].get<int>()&7);
            else if(loyalty>=50)loyalty=67;
        }
        auto scale=[&](int offset,int width,int mask,int base){
            const int factor=base+(development_["random_sequence"][cursor++].get<int>()&mask);
            std::uint32_t value=0;for(int i=0;i<width;++i)value|=std::uint32_t(bytes_[b+offset+i])<<(8*i);
            value=width==3?(value/100)*factor:(value*factor)/100;
            for(int i=0;i<width;++i)bytes_[b+offset+i]=(value>>(8*i))&255;
        };
        scale(1,3,15,42);scale(4,2,15,42);scale(6,2,15,42);scale(8,3,7,70);scale(14,2,15,42);
    }
    // DAC7 clears only the defeated current ruler's remaining command books.
    if(city==29&&bytes_[0xd8b]==loser)bytes_[0xd8f+loser*4]=0;
    return {{"city",city},{"next_city",city+1},{"stage",city==29?34:33},{"changed",changed},
        {"city_before",before},{"city_after",std::vector<std::uint8_t>(bytes_.begin()+b,bytes_.begin()+b+36)}};
}
// DD02/DD8A/DD98: time-limit announcement, acknowledgement, then forced
// attacker withdrawal. Text/portrait waits are folded at explicit UI steps.
Json OriginalState::time_limit_step(int stage,int human_mask,int speaker) const {
    if(stage<0||stage>2||human_mask<0||human_mask>255||(human_mask&15)>2||(human_mask>>4)>2||
       speaker<0||(speaker>=241&&speaker!=255)||(bytes_[0xde3]&15)>=6||(bytes_[0xde3]>>4)>=6)
        return {{"error","无效限时战果记录"}};
    const bool attacking=(human_mask&48)!=0;
    if(stage==0){
        int best=-1;speaker=255;
        // DD50 includes twelve defending slots and eleven attacking slots.
        // Equal martial ability selects the later occupied slot, including zero.
        for(int i=0;i<(attacking?11:12);++i){
            const int id=bytes_[attacking?0xdc2+i*3:0xdaa+i*2];
            if(id==255)continue;
            if(id>=241)return {{"error","无效限时军团名册"}};
            const int martial=bytes_[0x438+id*8+3];
            if(martial>=best){best=martial;speaker=id;}
        }
    }
    if(speaker==255)return {{"error","限时报告没有可出面的武将"}};
    return {{"phase",stage==2?13:15},{"stage",stage==2?14:stage+1},{"reason",stage==2?36:0},
        {"speaker",speaker},{"faction",attacking?bytes_[0xde3]>>4:bytes_[0xde3]&15},{"side",attacking?128:0}};
}
// Original result steps: phase 13 counters 9..12 (defender withdrawal),
// 14..16 (attacker withdrawal), phase 14 counters 0..3 (commander lost)
// and 24..28 (defender army lost).
Json OriginalState::withdrawal_result_step(int target,int source,int stage,Json &captives,std::uint8_t &cursor){
    if(target<0||target>=30||source<0||source>=30||source==target||!((stage>=0&&stage<=3)||(stage>=9&&stage<=12)||(stage>=14&&stage<=16)||(stage>=24&&stage<=28))||
       !captives.is_array()||captives.size()!=24||(bytes_[0xde3]&15)>=6||(bytes_[0xde3]>>4)>=6)
        return {{"error","无效撤离战果记录"}};
    for(const auto &id:captives)if(!id.is_number_integer()||id<0||(id>=241&&id!=255))return {{"error","无效战后俘虏名册"}};
    auto vacancy=[&](int city){for(int i=0;i<12;++i)if(bytes_[city*36+16+i]==255)return i;return -1;};
    auto reduce_loyalty=[&](int id){
        auto &loyalty=bytes_[0x438+id*8+5];
        if(loyalty>=80)loyalty=20+(development_["random_sequence"][cursor++].get<int>()&3);
        else if(loyalty>=60)loyalty=25+(development_["random_sequence"][cursor++].get<int>()&7);
        else if(loyalty>=50)loyalty=67;
    };
    if(stage==0)return {{"stage",1},{"officer",255}};
    if(stage==3||stage==14){
        auto result=retreat_army_step(cursor);if(result.contains("error"))return result;
        if(result["done"])return {{"stage",stage+1},{"officer",255}};
        result["stage"]=stage;return result;
    }
    if(stage==24){
        for(int i=0;i<12;++i){const int id=bytes_[target*36+16+i];if(id>=241&&id!=255)return {{"error","无效战后驻城武将"}};}
        for(int i=0;i<12;++i){const int id=bytes_[target*36+16+i];if(id!=255)reduce_loyalty(id);}
        return {{"stage",25},{"officer",255}};
    }
    if(stage==12||stage==28){
        const int b=target*36;
        const auto before=std::vector<std::uint8_t>(bytes_.begin()+b,bytes_.begin()+b+36);
        bytes_[b]=(bytes_[b]&248)|(bytes_[0xde3]>>4);
        auto scale=[&](int offset,int width,int mask,int base){
            const auto factor=base+(development_["random_sequence"][cursor++].get<int>()&mask);
            std::uint32_t value=0;for(int i=0;i<width;++i)value|=std::uint32_t(bytes_[b+offset+i])<<(i*8);
            // F8AF divides 24-bit values before multiplying. F878 reverses
            // the order for 16-bit values; preserve the two truncation rules.
            value=width==3?(value/100)*factor:(value*factor)/100;
            for(int i=0;i<width;++i)bytes_[b+offset+i]=(value>>(i*8))&255;
        };
        scale(1,3,15,42);scale(4,2,15,42);scale(6,2,15,42);scale(8,3,7,70);scale(14,2,15,42);
        return {{"stage",stage+1},{"officer",255},{"city",target},{"city_before",before},
            {"city_after",std::vector<std::uint8_t>(bytes_.begin()+b,bytes_.begin()+b+36)}};
    }
    const bool army=stage==9||stage==25;
    const bool upper=stage==2||stage==11||stage==16||stage==26;
    int chosen=-1,best=-1,id=255;
    for(int i=(upper?12:0);i<(army?11:upper?24:12);++i){
        const int current=army?bytes_[0xdc2+i*3]:captives[i].get<int>();
        if(current==255)continue;
        if(current>=241||(army&&bytes_[0xdc3+i*3]>=160))return {{"error","无效入城军团记录"}};
        const int loyalty=bytes_[0x438+current*8+5];
        if(loyalty>=best){best=loyalty;chosen=i;id=current;}
    }
    if(chosen<0)return {{"stage",stage+1},{"officer",255}};
    int city=stage==16?source:target,slot=-1;
    if(stage==10){
        city=-1;int count=13;
        for(int c=0;c<30;++c){
            if(c==target||(bytes_[c*36]&7)!=(bytes_[0xde3]&15))continue;
            int n=0;for(int i=0;i<12;++i)n+=bytes_[c*36+16+i]!=255;
            if(n<count){city=c;count=n;}
        }
    }
    if(city>=0)slot=vacancy(city);
    if((stage==11||stage==26||stage==27)&&slot<0){city=source;slot=vacancy(city);}
    const bool returned=slot>=0||(army&&id<6);
    if(returned)bytes_[city*36+16+(slot<0?0:slot)]=id;
    else {reduce_loyalty(id);bytes_[0x438+id*8]|=128;}
    if(army){
        const int b=0xdc2+chosen*3,position=bytes_[b+1];
        bytes_[0xe1a+position]=bytes_[0xf1a+position];std::fill(bytes_.begin()+b,bytes_.begin()+b+3,255);
    }else captives[chosen]=255;
    return {{"stage",stage},{"officer",id},{"city",city},{"returned",returned},{"slot",chosen}};
}
}
