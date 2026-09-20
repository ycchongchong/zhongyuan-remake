#include "zhongyuan/original_state.hpp"
#include <algorithm>
#include <set>
namespace zhongyuan {
OriginalState::OriginalState(const OriginalRom &rom, std::vector<std::uint8_t> sram)
    : rom_(rom), bytes_(std::move(sram)), roads_(rom.world_map().at("edges")), development_(rom.development_tables()), commands_(rom.command_tables()) {
    if(bytes_.size()!=8192)throw std::runtime_error("Expected complete 8192-byte SRAM image");
}
Json OriginalState::city(int id) const {
    if(id<0||id>=30)throw std::out_of_range("City");
    auto c=rom_.city(id);const int base=id*36;
    const auto read=[&](int off,int n){std::uint32_t v=0;for(int i=0;i<n;++i)v|=std::uint32_t(bytes_[base+off+i])<<(8*i);return v;};
    c["raw"]=std::vector<std::uint8_t>(bytes_.begin()+base,bytes_.begin()+base+36);
    c["flags_raw"]=bytes_[base];c["faction_id"]=bytes_[base]&7;c["faction_id_candidate"]=bytes_[base]&7;
    c["gold"]=read(1,3);c["land"]=read(4,2);c["commerce"]=read(6,2);c["population"]=read(8,3);c["control"]=bytes_[base+14];c["reserves"]=read(11,2)*100;
    c["inventory"]={read(1,3),read(28,2),read(30,1),read(34,1),read(35,1)};
    const auto &shops=commands_;c["shops"]=Json::array();
    for(int n=0;n<shops["shop_count_minus_two"][id].get<int>()+2;++n)c["shops"].push_back((shops["shop_types"][id].get<int>()>>(2*n))&3);
    c["officer_slots"]=Json::array();c["neighbors"]=Json::array();
    for(int i=0;i<12;++i)c["officer_slots"].push_back(bytes_[base+16+i]==255?Json(nullptr):Json(bytes_[base+16+i]));
    for(const auto &edge:roads_){if(edge[0]==id)c["neighbors"].push_back(edge[1]);else if(edge[1]==id)c["neighbors"].push_back(edge[0]);}
    return c;
}
Json OriginalState::officer(int id) const {
    if(id<0||id>=241)throw std::out_of_range("Officer");
    auto o=rom_.officer(id);const int base=0x438+id*8;
    o["raw"]=std::vector<std::uint8_t>(bytes_.begin()+base,bytes_.begin()+base+8);
    o["state_raw"]=bytes_[base];o["stamina"]=bytes_[base+1];o["intelligence"]=bytes_[base+2];o["martial"]=bytes_[base+3];
    o["loyalty"]=bytes_[base+5]==255?Json(nullptr):Json(bytes_[base+5]);
    o["equipment_or_rank_raw"]=bytes_[base+6];o["archer_low_nibble_raw"]=bytes_[base+4]&15;
    o["infantry"]=(bytes_[base+4]>>4)*100;o["cavalry"]=(bytes_[base+7]&15)*100;o["archers"]=(bytes_[base+7]>>4)*100;
    return o;
}
Json OriginalState::snapshot() const {
    Json cities=Json::array(),officers=Json::array(),rulers=Json::array();
    for(int i=0;i<30;++i)cities.push_back(city(i));
    for(int i=0;i<241;++i)officers.push_back(officer(i));
    for(int i=0;i<6;++i)rulers.push_back({{"id",i},{"seat",bytes_[0xd8d + 4*i]},{"city_count_raw",bytes_[0xd8e + 4*i]},{"books",bytes_[0xd8f + 4*i]},{"flags_raw",bytes_[0xd90 + 4*i]}});
    return {{"cities",cities},{"officers",officers},{"rulers",rulers},{"year",bytes_[0xd85]+256*bytes_[0xd86]},
        {"month",bytes_[0xd87]},{"difficulty",bytes_[0xd88]},{"player",bytes_[0xd89]&7},
        {"second_player",(bytes_[0xd8a]&128)?int(bytes_[0xd8a]&7):-1},{"current_ruler",bytes_[0xd8b]},
        {"action_counter",bytes_[0xd8c]},{"sram",bytes_}};
}
void OriginalState::set_players(int first,int second) {
    if(first<0||first>=6||second<-1||second>=6||second==first)throw std::invalid_argument("Player selection");
    bytes_[0xd89]=128+first;bytes_[0xd8a]=second<0?0:128+second;
}
void OriginalState::begin_ruler_turn(int ruler) {
    if(ruler<0||ruler>=6)throw std::out_of_range("Ruler");
    int count=0;for(int i=0;i<30;++i)if((bytes_[i*36]&7)==ruler)++count;
    if(!count)throw std::invalid_argument("Ruler has no cities");
    bytes_[0xd8b]=ruler;bytes_[0xd8c]=1;bytes_[0xda9]=0;
    bytes_[0xd8f+4*ruler]=rom_.command_books(bytes_[0xd88],count);
}
bool OriginalState::human_turn() const {
    const int ruler=bytes_[0xd8b];
    return ruler==(bytes_[0xd89]&7)||((bytes_[0xd8a]&128)&&ruler==(bytes_[0xd8a]&7));
}
void OriginalState::end_orders(){bytes_.at(0xd8f+4*bytes_.at(0xd8b))=0;}
void OriginalState::finish_ai(const Json &result) {
    if(!result.at("done").get<bool>())throw std::invalid_argument("AI has not finished");
    const int battle=result.at("battle_result");
    if(battle<0||battle>2)throw std::invalid_argument("Human battle must be resolved first");
    // D88D-D8AE applies occupation after the NPC battle report.
    if(battle==2){
        const int source=result.at("source"),target=result.at("target");
        if(source<0||source>=30||target<0||target>=30)throw std::out_of_range("Battle city");
        bytes_[target*36]=(bytes_[target*36]&248)|(bytes_[source*36]&7);
    }
    end_orders();
}
bool OriginalState::next_ruler(std::uint8_t &cursor) {
    if(bytes_.at(0xd8f+4*bytes_.at(0xd8b)))throw std::logic_error("Orders remain");
    // A086-A0CE: dead rulers are skipped; wrapping six rulers settles a month.
    int ruler=bytes_[0xd8b];bool month=false;
    for(int attempts=0;attempts<6;++attempts){
        if(++ruler==6){ruler=0;month=true;}
        if(!(bytes_[0x438+ruler*8]&32)){
            bytes_[0xd8c]=0;bytes_[0xd8b]=ruler;
            if(month)settle_month(cursor);
            return month;
        }
    }
    throw std::runtime_error("No living ruler");
}
Json OriginalState::ending() const {
    if(bytes_[0xd85]>=250)return {{"kind","year_limit"},{"winner",nullptr}};
    const int ruler=bytes_[0xd8b];int cities=0;
    for(int c=0;c<30;++c)cities+=(bytes_[c*36]&7)==ruler;
    if(cities==30)return {{"kind","unification"},{"winner",ruler}};
    const int first=bytes_[0xd89]&7,second=bytes_[0xd8a]&7;
    if((bytes_[0x438+first*8]&32)&&(!(bytes_[0xd8a]&128)||(bytes_[0x438+second*8]&32)))
        return {{"kind","players_eliminated"},{"winner",nullptr}};
    return nullptr;
}
std::string OriginalState::recruit_reserves(int city,int hundreds){
    if(city<0||city>=30||hundreds<0||hundreds>255)return "无效征兵数量";
    const int ruler=bytes_[0xd8b],base=city*36;
    if((bytes_[base]&7)!=ruler)return "只能在己方城池征兵";
    if(!bytes_[0xd8f+ruler*4])return "命令书已用完";
    const auto gold=bytes_[base+1]+(bytes_[base+2]<<8)+(bytes_[base+3]<<16);
    const int cost=20*hundreds;
    if(gold<cost)return "黄金不足，每百人需要二十金";
    // D5F7/D60A consumes the order even for an accepted zero quantity.
    ++bytes_[0xd8c];--bytes_[0xd8f+ruler*4];
    for(int i=0;i<3;++i)bytes_[base+1+i]=(gold-cost)>>(8*i);
    const auto reserve=std::uint16_t(bytes_[base+11]+(bytes_[base+12]<<8)+hundreds);
    bytes_[base+11]=reserve&255;bytes_[base+12]=reserve>>8;
    return {};
}
std::string OriginalState::assign_troops(int city,int officer,int hundreds){
    if(city<0||city>=30||officer<0||officer>=241||hundreds<0||hundreds>10)return "每将兵力须在零至一千人之间";
    if((bytes_[city*36]&7)!=bytes_[0xd8b]||!resident(city,officer))return "请选择本城武将";
    const int base=city*36,b=0x438+officer*8,current=bytes_[b+6]&15;
    const int reserve=bytes_[base+11]+(bytes_[base+12]<<8),delta=hundreds-current;
    if(delta>reserve)return "后备兵不足";
    const auto mix=rom_.ai_tables()["troop_mix"][bytes_[b+3]/10].get<int>();
    // BC2C-BC90 / F756-F7FB: add infantry, then cavalry, then archers;
    // removing soldiers reverses that order. No command book is spent here.
    for(int n=current;n<hundreds;++n){
        if((bytes_[b+4]>>4)<(mix&15))bytes_[b+4]+=16;
        else if((bytes_[b+7]&15)<(mix>>4))++bytes_[b+7];
        else bytes_[b+7]+=16;
    }
    for(int n=current;n>hundreds;--n){
        if(bytes_[b+7]&240)bytes_[b+7]-=16;
        else if(bytes_[b+7]&15)--bytes_[b+7];
        else bytes_[b+4]-=16;
    }
    bytes_[b+6]=(bytes_[b+6]&240)|((bytes_[b+4]>>4)+(bytes_[b+7]&15)+(bytes_[b+7]>>4));
    const auto left=std::uint16_t(reserve-delta);bytes_[base+11]=left&255;bytes_[base+12]=left>>8;
    return {};
}
void OriginalState::advance_calendar(std::uint8_t &cursor) {
    const auto random=[&](){const int v=development_["random_sequence"][cursor];++cursor;return v;};
    const auto human=[&](int owner){return owner==(bytes_[0xd89]&7)||((bytes_[0xd8a]&128)&&owner==(bytes_[0xd8a]&7));};
    for(int i=0;i<30;++i)if(!human(bytes_[i*36]&7)) {
        const int bonus=development_["calendar_control_bonus"][random()&7];
        const auto sum=static_cast<std::uint8_t>(bytes_[i*36+14]+bonus);
        const auto value=static_cast<std::uint8_t>(std::min<int>(101,sum)-2);
        bytes_[i*36+14]=(value&128)?0:value;
    }
    if(++bytes_[0xd87]>=13) {
        for(int i=0;i<30;++i)if(human(bytes_[i*36]&7)) {
            const int first=random()&2,loss=first+(random()&3);
            bytes_[i*36+14]=std::max<int>(0,bytes_[i*36+14]-loss);
        }
        bytes_[0xd87]=1;if(++bytes_[0xd85]==0)++bytes_[0xd86];
    }
}
void OriginalState::settle_month(std::uint8_t &cursor) {
    advance_calendar(cursor);
    if(bytes_[0xd85]>=250)return; // A0B6-A0C7 enters the year-limit ending here.
    // AE68-AEAF: pending personnel arrivals, preserving unknown record semantics.
    for(int i=0;i<93;i+=3) {
        const int base=0xc30+i,destination=bytes_[base+1],id=bytes_[base];
        if(destination==255)continue;
        if(destination&128){bytes_[base+1]&=127;continue;}
        int empty=-1;
        if(destination<30 && (bytes_[destination*36]&7)==bytes_[base+2])
            for(int slot=0;slot<12;++slot)if(bytes_[destination*36+16+slot]==255){empty=slot;break;}
        if(empty>=0)bytes_[destination*36+16+empty]=id;
        else if(id<241)bytes_[0x438+id*8+2]=(bytes_[0x438+id*8+2]&0xbf)|0x80;
        bytes_[base]=255;bytes_[base+1]=255;bytes_[base+2]=255;
    }
    // DC4F-DC6C visits 248 bytes, including eight bytes beyond the 30x8 table.
    for(int i=0;i<248;++i)if(bytes_[0xc8d+i]!=255) {
        auto value=static_cast<std::uint8_t>(bytes_[0xc8d+i]-32);
        bytes_[0xc8d+i]=(value&0xe0)?value:255;
    }
    const int month=bytes_[0xd87];if(month!=4 && month!=10)return;
    const int field=month==4?6:4;
    const auto read=[&](int base,int n){std::uint32_t result=0;for(int i=0;i<n;++i)result|=std::uint32_t(bytes_[base+i])<<(8*i);return result;};
    // DA4F-DACB: all cities receive three times (field + population/1024).
    // The intermediate addition is 16-bit; the result is added to 24-bit gold.
    for(int city=0;city<30;++city) {
        const int base=city*36;
        const auto sum=static_cast<std::uint16_t>(read(base+field,2)+(read(base+8,3)>>10));
        const auto gold=read(base+1,3)+std::uint32_t(sum)*3;
        for(int i=0;i<3;++i)bytes_[base+1+i]=static_cast<std::uint8_t>(gold>>(8*i));
    }
}
std::string OriginalState::move_officers(int source,int destination,const std::vector<int> &officers){
    if(source<0||source>=30||destination<0||destination>=30||source==destination)return "无效目标城池";
    const int ruler=bytes_[0xd8b]&7;
    if(ruler>=6)return "无效当前君主";
    if((bytes_[source*36]&7)!=ruler||(bytes_[destination*36]&7)!=ruler)return "只能移动到己方城池";
    const auto pair=Json::array({std::min(source,destination),std::max(source,destination)});
    if(std::find(roads_.begin(),roads_.end(),pair)==roads_.end())return "没有相连道路";
    // A89C-A8B3 additionally rejects destinations in this runtime table.
    for(int i=0;i<8;++i)if((bytes_[0xc8d+source*8+i]&31)==destination)return "当前道路不可通行";
    std::set<int> chosen(officers.begin(),officers.end());
    if(chosen.empty()||chosen.size()!=officers.size())return "请选择不重复的武将";
    for(int id:chosen)if(id<0||id>=241)return "无效武将";
    int count=0,available=0;
    std::vector<int> slots;
    for(int i=0;i<12;++i){
        const int id=bytes_[source*36+16+i];if(id!=255)++count;
        if(chosen.count(id))slots.push_back(i);
        if(bytes_[destination*36+16+i]==255)++available;
    }
    if(slots.size()!=chosen.size())return "武将不在出发城";
    if(static_cast<int>(slots.size())>=count)return "出发城必须留下武将";
    if(static_cast<int>(slots.size())>available)return "目标城没有足够空位";
    const int books=0xd8f+ruler*4;
    if(bytes_[books]==0)return "需要一枚命令书";
    auto next=bytes_;
    ++next[0xd8c];next[books]=static_cast<std::uint8_t>(std::min<int>(15,next[books]-1));
    // A921-A98D processes selected slots backwards, fills the first target gap,
    // preserves source gaps, and updates a ruler's seat when that ruler moves.
    for(auto it=slots.rbegin();it!=slots.rend();++it){
        const int id=next[source*36+16+*it];
        for(int i=0;i<12;++i)if(next[destination*36+16+i]==255){next[destination*36+16+i]=id;break;}
        if(id<6)next[0xd8d+id*4]=static_cast<std::uint8_t>(destination);
        next[source*36+16+*it]=255;
    }
    bytes_=std::move(next);return {};
}
std::string OriginalState::dispatch_search(int city,int officer) {
    if(!resident(city,officer))return "请选择本城武将";
    const int ruler=bytes_[0xd8b];
    if(ruler>=6||(bytes_[city*36]&7)!=ruler)return "只能在己方城池搜索";
    if(officer<6)return "君主不能外出搜索";
    int count=0;for(int i=0;i<12;++i)if(bytes_[city*36+16+i]!=255)++count;
    if(count<2)return "本城必须留下武将";
    int record=-1;
    for(int i=0;i<93;i+=3)if(bytes_[0xc30+i]==255){record=0xc30+i;break;}
    if(record<0)return "没有空闲派遣记录";
    const int books=0xd8f+4*ruler;if(bytes_[books]==0)return "需要一枚命令书";
    // D5F7-D60A followed by AC45-AC73: no gold or troop deduction.
    ++bytes_[0xd8c];bytes_[books]=std::min<int>(15,bytes_[books]-1);
    for(int i=0;i<12;++i)if(bytes_[city*36+16+i]==officer){bytes_[city*36+16+i]=255;break;}
    bytes_[record]=officer;bytes_[record+1]=128|city;bytes_[record+2]=ruler;
    return {};
}
int OriginalState::collect_search(int city) {
    if(city<0||city>=30||bytes_[0xd8b]>=6||(bytes_[city*36]&7)!=bytes_[0xd8b])return -1;
    // ADF4 scans 30 records; AE68 monthly cleanup scans 31. Preserve this difference.
    for(int i=0;i<90;i+=3) {
        const int base=0xc30+i;
        if(bytes_[base+1]!=city||(bytes_[base+2]&7)!=bytes_[0xd8b])continue;
        const int id=bytes_[base];if(id>=241)return -1;
        bytes_[base]=bytes_[base+1]=255; // The third byte is deliberately retained.
        for(int slot=0;slot<12;++slot)if(bytes_[city*36+16+slot]==255){bytes_[city*36+16+slot]=id;return id;}
        bytes_[0x438+id*8+2]=(bytes_[0x438+id*8+2]&0xbf)|0x80;
        return id;
    }
    return -1;
}
int OriginalState::search_kind(int officer,std::uint8_t &cursor,bool exclude_people) const {
    if(officer<0||officer>=241)throw std::out_of_range("Search officer");
    const auto tier=[](int value){return value<41?0:(value<71?1:2);};
    const int group=tier(bytes_[0x438+officer*8+2])| (tier(rom_.officer(officer)["virtue"])*4);
    const auto table=rom_.search_tables();
    for(int i=0;i<256;++i) {
        const int value=development_["random_sequence"][cursor++].get<int>()&7;
        const int kind=table["outcomes"][group*8+value];
        if(!exclude_people || kind<6 || kind>8)return kind;
    }
    throw std::runtime_error("Invalid search outcome table");
}
Json OriginalState::search_find(int city,int kind,std::uint8_t &cursor) {
    if(city<0||city>=30||kind<0||kind>9)throw std::out_of_range("Search result");
    const auto tables=rom_.search_tables();
    const auto random=[&](){return development_["random_sequence"][cursor++].get<int>();};
    Json result={{"kind",kind},{"value",0},{"candidate",nullptr},{"dialogue_id",tables["dialogues"][kind]}};
    const int base=city*36;
    if(kind<3) {
        const int amount=tables["gold_tens"][kind*16+(random()&15)].get<int>()*10;
        const std::uint32_t gold=bytes_[base+1]+256u*bytes_[base+2]+65536u*bytes_[base+3]+amount;
        for(int i=0;i<3;++i)bytes_[base+1+i]=static_cast<std::uint8_t>(gold>>(i*8));
        result["value"]=amount;
    }else if(kind<6) {
        const int amount=(random()&3)+1,field=tables["item_offsets"][kind];
        bytes_[base+field]=static_cast<std::uint8_t>(bytes_[base+field]+amount);
        result["value"]=amount;result["item_offset"]=field;
    }else if(kind<9) {
        int candidate=255,retries=0;
        // B041-B0BA retries unavailable candidates. Presentation between retries
        // has its own frame timing and is not reproduced by this state method.
        const auto year_count=static_cast<std::uint8_t>(bytes_[0xd85]-199);
        if(year_count==0)throw std::runtime_error("Original candidate selection cannot finish in this year");
        for(int attempt=0;attempt<16;++attempt) {
            int index;do {index=random()&7;}while(index>=year_count);
            candidate=tables["candidates"][city*8+index];
            if(candidate==255 || (bytes_[0x438+candidate*8]&64))break;
            ++retries;
            if(attempt==15) {
                candidate=255;
                for(int id=0;id<240;++id)if(bytes_[0x438+id*8]&128){candidate=id;break;}
            }
        }
        if(candidate==255){result["reroll"]=true;result["exclude_people"]=retries>0;return result;}
        result["candidate"]=candidate;
        if(kind==6) {
            const int off=0x438+candidate*8;
            result["value"]=(bytes_[off+1]+bytes_[off+2]+bytes_[off+3]+bytes_[off+5]+rom_.officer(candidate)["virtue"].get<int>())/5;
        }else {
            const std::set<int> special={227,185,113,115,31,130,92,93,62,200};
            if(special.count(candidate)){result["kind"]=7;result["dialogue_id"]=tables["dialogues"][7];}
        }
    }
    return result;
}
bool OriginalState::recruit_search(int city,int officer,int kind,int cost,std::uint8_t &cursor) {
    if(city<0||city>=30||officer<6||officer>=241||kind<6||kind>8||cost<0||cost>255)throw std::out_of_range("Search recruitment");
    const int base=city*36;
    int empty=-1;for(int i=0;i<12;++i)if(bytes_[base+16+i]==255){empty=base+16+i;break;}
    if(empty<0)return false;
    if(kind==6) {
        const std::uint32_t gold=bytes_[base+1]+256u*bytes_[base+2]+65536u*bytes_[base+3];
        if(gold<static_cast<std::uint32_t>(cost))return false;
        for(int i=0;i<3;++i)bytes_[base+1+i]=static_cast<std::uint8_t>((gold-cost)>>(i*8));
    }else if(kind==7) {
        const int chance=(officer==200||officer==113)?6:8;
        if((development_["random_sequence"][cursor++].get<int>()&15)>=chance)return false;
    }
    bytes_[empty]=officer;bytes_[0x438+officer*8]&=31;
    return true;
}
bool OriginalState::resident(int city,int officer) const {
    if(city<0||city>=30||officer<0||officer>=241)return false;
    const auto first=bytes_.begin()+city*36+16;
    return std::find(first,first+12,officer)!=first+12;
}
Json OriginalState::development_options(int city,int officer,int kind) const {
    Json result=Json::array();
    if(!resident(city,officer)||kind<0||kind>2)return result;
    const int intelligence=bytes_[0x438+officer*8+2];
    if(intelligence>100)return result;
    const int tier=intelligence<40?0:(intelligence<70?1:2);
    const int start=development_["tier_starts"][tier], count=development_["tier_counts"][tier];
    for(int i=0;i<count;++i)result.push_back(development_["proposals"][kind][start+i]);
    return result;
}
Json OriginalState::next_development_offer(int city,int officer,int kind,std::uint8_t &cursor) const {
    const auto options=development_options(city,officer,kind);
    if(options.empty())return nullptr;
    // E9E6 returns sequence[cursor++] with 8-bit wrap. B53D-B547 rejects
    // values outside this intelligence tier instead of reducing modulo count.
    for(int i=0;i<256;++i) {
        const int value=development_["random_sequence"][cursor].get<int>()&7;
        ++cursor;
        if(value<static_cast<int>(options.size()))return options[value];
    }
    throw std::runtime_error("Invalid original random sequence");
}
std::string OriginalState::develop(int city,int officer,int kind,int proposal,std::uint8_t frame_counter) {
    const auto options=development_options(city,officer,kind);
    auto option=std::find_if(options.begin(),options.end(),[proposal](const Json &v){return v["index"]==proposal;});
    if(option==options.end())return "无效开发方案或执行武将";
    const int ruler=bytes_[0xd8b]&7;
    if(ruler>=6||(bytes_[city*36]&7)!=ruler)return "只能开发己方城池";
    const int books=0xd8f+ruler*4;
    if(bytes_[books]==0)return "需要一枚命令书";
    const auto read=[this](int offset,int size){std::uint32_t v=0;for(int i=0;i<size;++i)v|=std::uint32_t(bytes_[offset+i])<<(8*i);return v;};
    const int base=city*36,cost=option->at("cost");
    if(read(base+1,3)<static_cast<unsigned>(cost))return "黄金不足";
    auto next=bytes_;
    const auto write=[&next](int offset,int size,std::uint32_t value){for(int i=0;i<size;++i)next[offset+i]=static_cast<std::uint8_t>(value>>(i*8));};
    const int intelligence=bytes_[0x438+officer*8+2];
    const int gain=(cost>>1)+development_["intelligence_bonus"][intelligence>>3].get<int>()+(frame_counter&7);
    write(base+1,3,read(base+1,3)-cost);
    const int field=kind==0?4:(kind==1?6:8),width=kind==2?3:2;
    write(base+field,width,read(base+field,width)+gain*(kind==2?100:1));
    // Preserve the original byte addition followed by the 100 clamp, including
    // its wrap behavior for artificially corrupted/out-of-range starting data.
    next[base+14]=std::min<int>(100,static_cast<std::uint8_t>(bytes_[base+14]+(gain>>2)));
    ++next[0xd8c];next[books]=static_cast<std::uint8_t>(std::min<int>(15,next[books]-1));
    bytes_=std::move(next);return {};
}
}
