#include "zhongyuan/original_state.hpp"
#include <algorithm>
#include <array>
namespace zhongyuan {
Json OriginalState::execute_command(int city,const std::string &kind,const Json &args,std::uint8_t &cursor,std::uint8_t frame){
    // Validate first, then commit a copy: failed orders never spend resources or RNG.
    try {
        if(city<0||city>=30)throw std::invalid_argument("无效城池");
        const int base=city*36,ruler=bytes_[0xd8b];
        if((bytes_[base]&7)!=ruler)throw std::invalid_argument("只能从己方城池执行指令");
        if(!bytes_[0xd8f+4*ruler]&&!(kind=="scout"&&args.value("target",-1)==city))throw std::invalid_argument("命令书已用完");
        const auto number=[&](const char *key,int max,int fallback=-1){
            if(!args.contains(key))return fallback;
            const auto &v=args.at(key);if(!v.is_number_integer()||v.get<std::int64_t>()<0||v.get<std::int64_t>()>max)throw std::invalid_argument("无效指令参数");return v.get<int>();
        };
        auto next=bytes_;auto random_cursor=cursor;const auto &tables=commands_;
        const auto read=[&](int b,int n){std::uint32_t v=0;for(int i=0;i<n;++i)v|=std::uint32_t(next.at(b+i))<<(i*8);return v;};
        const auto write=[&](int b,int n,std::uint32_t value){for(int i=0;i<n;++i)next.at(b+i)=value>>(i*8);};
        const auto debit=[&](int cost){if(read(base+1,3)<std::uint32_t(cost))throw std::invalid_argument("黄金不足");write(base+1,3,read(base+1,3)-cost);};
        const auto random=[&](){const int v=development_["random_sequence"][random_cursor];++random_cursor;return v&3;};
        const int quantity=number("quantity",999999,0),officer=number("officer",240),target=number("target",29);
        const int ob=0x438+officer*8;
        Json result={{"kind",kind},{"city",city}};
        const auto require_officer=[&](){if(officer<0||!resident(city,officer))throw std::invalid_argument("请选择本城武将");};
        const auto service=[&](int type){bool available=false;for(int i=0;i<tables["shop_count_minus_two"][city].get<int>()+2;++i)available|=((tables["shop_types"][city].get<int>()>>(i*2))&3)==type;if(!available)throw std::invalid_argument("本城没有此商店");};
        if(kind=="scout"){
            if(target<0)throw std::invalid_argument("请选择侦查城池");
            const auto terrain=rom_.battlefield(target)["terrain"].get<std::vector<std::uint8_t>>();
            std::copy(terrain.begin(),terrain.end(),next.begin()+0xe1a);std::copy(terrain.begin(),terrain.end(),next.begin()+0xf1a);
            const auto source_position=rom_.city(city)["map_position"],target_position=rom_.city(target)["map_position"];
            next[0xda5]=source_position[0].get<int>()+8;next[0xda7]=source_position[1].get<int>()+8;
            next[0xda6]=target_position[0].get<int>()+8;next[0xda8]=target_position[1].get<int>()+8;
            result["target"]=target;
        }else if(kind=="transport"||kind=="gift"||kind=="sell"){
            if(!args.contains("items")||!args["items"].is_array()||args["items"].size()!=5)throw std::invalid_argument("请选择五类物资数量");
            std::array<int,5> amount{};constexpr int offsets[]={1,28,30,34,35},widths[]={3,2,1,1,1};bool any=false;
            for(int i=0;i<5;++i){const auto &v=args["items"][i];if(!v.is_number_integer()||v.get<std::int64_t>()<0||v.get<std::int64_t>()>int(read(base+offsets[i],widths[i])))throw std::invalid_argument("物资不足或数量无效");amount[i]=v.get<int>();any|=amount[i]>0;}
            if(!any)throw std::invalid_argument("请选择非零物资数量");
            if(kind!="transport"&&(amount[0]||amount[1]))throw std::invalid_argument("赠送与出售只接受宝石、手镯和指环");
            int outcome=0;
            if(kind=="sell")service(3);
            else{
                if(target<0||target==city)throw std::invalid_argument("请选择其他相邻城池");
                bool adjacent=false;for(const auto &edge:roads_)adjacent|=(edge[0]==city&&edge[1]==target)||(edge[1]==city&&edge[0]==target);
                if(!adjacent)throw std::invalid_argument("目标城池不相邻");
                if(kind=="transport"&&(next[target*36]&7)!=ruler)throw std::invalid_argument("运输目标必须是己方城池");
                if(kind=="gift"){
                    if((next[target*36]&7)==ruler||(next[target*36]&7)>=6)throw std::invalid_argument("请选择其他势力的城池");
                    const auto weight=std::uint8_t(amount[2]*3+amount[3]*2+amount[4]);
                    outcome=tables["gift_results"][(weight<10?(weight/2)*8:40)+(frame&7)];result["outcome"]=outcome;
                    // C4B5 compares the entire road byte, including its duration bits.
                    if(outcome!=2){int slot=-1;const int road=0xc8d+target*8;for(int i=0;i<8;++i)if(next[road+i]==city){slot=i;break;}if(slot<0)for(int i=0;i<8;++i)if(next[road+i]==255){slot=i;break;}if(slot<0)throw std::invalid_argument("目标城池的外交记录已满");next[road+slot]=(outcome==1?0x60:((frame&0x60)|0x80))|city;}
                }
            }
            if(kind!="gift"||outcome!=2)for(int i=0;i<5;++i){write(base+offsets[i],widths[i],read(base+offsets[i],widths[i])-amount[i]);if(kind!="sell")write(target*36+offsets[i],widths[i],read(target*36+offsets[i],widths[i])+amount[i]);}
            if(kind=="sell"){const int gold=(amount[2]*3+amount[3]*2+amount[4])*30;write(base+1,3,read(base+1,3)+gold);result["gold"]=gold;}
        }else if(kind=="buy"){
            service(0);if(!quantity)throw std::invalid_argument("请输入购买数量");debit(quantity*5);
            // CD60 only adds the low quantity byte to the 16-bit stock.
            write(base+28,2,read(base+28,2)+(quantity&255));result["cost"]=quantity*5;
        }else if(kind=="award_gold"||kind=="award_weapons"){
            require_officer();if(!quantity)throw std::invalid_argument("请输入赏赐数量");
            const bool weapon=kind=="award_weapons";const int stat=weapon?3:5;
            if(weapon){if(read(base+28,2)<std::uint32_t(quantity))throw std::invalid_argument("武器不足");write(base+28,2,read(base+28,2)-quantity);}else debit(quantity);
            const int reference=tables["award_reference"][officer][weapon?1:0];
            const int band=weapon?(reference<31?0:reference<50?1:reference<70?2:3):(reference<49?0:reference<80?1:reference<112?2:3);
            int delta=std::uint8_t((quantity&255)/tables["award_divisors"][band].get<int>()+random()+tables["award_bonus"][band].get<int>());
            const auto sum=std::uint8_t(next[ob+stat]+delta);if(sum>=99){delta=std::uint8_t(99-next[ob+stat]);next[ob+stat]=99;}else next[ob+stat]=sum;
            result["delta"]=delta;
        }else if(kind=="award_people"){
            if(!quantity)throw std::invalid_argument("请输入赏赐数量");debit(quantity);
            const int control=next[base+14];if(control>100)throw std::invalid_argument("统治度状态无效");
            int delta=std::uint8_t(((quantity&255)*rom_.officer(ruler)["virtue"].get<int>()*tables["control_multiplier"][control/10].get<int>()/3000)+1);
            const auto sum=std::uint8_t(control+delta);if(sum>=100){delta=100-control;next[base+14]=100;}else next[base+14]=sum;result["delta"]=delta;
        }else if(kind=="study"){
            service(1);require_officer();if(read(base+1,3)<30)throw std::invalid_argument("进入学问所至少需要三十金");
            const int knowledge=next[ob+2],cost=knowledge<31?30:knowledge<61?20:10;debit(cost);
            int delta=random()+(knowledge<31||knowledge>=61?6:10);const auto sum=std::uint8_t(knowledge+delta);if(sum>=99){delta=std::uint8_t(99-knowledge);next[ob+2]=99;}else next[ob+2]=sum;result["delta"]=delta;result["cost"]=cost;
        }else if(kind=="heal"){
            service(2);require_officer();debit(50);
            const int maximum=rom_.officer(officer)["stamina"],current=next[ob+1];int delta=(frame&7)+40;
            const auto sum=std::uint8_t(current+delta);if(sum>=maximum){delta=std::uint8_t(maximum-current);next[ob+1]=maximum;}else next[ob+1]=sum;result["delta"]=delta;result["cost"]=50;
        }else throw std::invalid_argument("尚未还原此指令");
        if(kind!="scout"||target!=city){++next[0xd8c];--next[0xd8f+4*ruler];}bytes_=std::move(next);cursor=random_cursor;return result;
    }catch(const std::exception &e){return {{"error",e.what()}};}
}
}
