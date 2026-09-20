#include "zhongyuan/original_state.hpp"
#include <algorithm>
#include <array>
namespace zhongyuan {
// Native translation of strategic AI routines BCEF-D159. The ROM supplies
// immutable tables only; this module does not interpret CPU instructions.
class OriginalAi {
    struct BudgetEnd{};struct NextAction{};
    OriginalState &state;
    std::vector<std::uint8_t> &s;
    Json tables,search,links,sequence;
    Json trace=Json::array();
    int ruler,y=0;
    std::uint8_t budget,phase,plan,round,cursor;
    int source=255,target=255,result=0,winner=255,a1=0;
    bool done=false;
    std::uint32_t work_gold=160;
    int random(){int value=sequence[cursor];++cursor;return value;}
    int percent(){int value;do{value=random();}while(value>=100);return value;}
    int owner(int c)const{return s.at(c*36)&7;}
    int count(int c)const{if(c<0||c>=30)return 0;int n=0;for(int i=0;i<12;++i)n+=s[c*36+16+i]!=255;return n;}
    int seat()const{for(int c=0;c<30;++c)if(owner(c)==ruler)for(int i=0;i<12;++i)if(s[c*36+16+i]==ruler)return c;return -1;}
    std::uint32_t read(int at,int width)const{std::uint32_t v=0;for(int i=0;i<width;++i)v|=std::uint32_t(s.at(at+i))<<(8*i);return v;}
    void write(int at,int width,std::uint32_t v){for(int i=0;i<width;++i)s.at(at+i)=std::uint8_t(v>>(8*i));}
    void spend(int cost){const bool exhausted=budget<cost;budget=std::uint8_t(budget-cost);if(exhausted)throw BudgetEnd{};}
    bool pay(int city,int cost){auto gold=read(city*36+1,3);if(gold<unsigned(cost))return false;write(city*36+1,3,gold-(cost&255));return true;}
    void pay_small(int city){if(!pay(city,(random()&7)+11))throw NextAction{};}
    bool border(int city,int faction)const{if(owner(city)!=faction)return false;for(const auto &link:links[city])if(owner(link["target"])!=ruler)return true;return false;}
    int frontier(int from,int faction){
        for(int c=from;c<30;++c){y=0;if(owner(c)!=faction)continue;
            int slot=0;for(const auto &link:links[c]){y=0;if(owner(link["target"])!=ruler)return c;slot+=2;}y=slot;
        }return -1;
    }
    int interior(int from){for(int c=from;c<30;++c)if(owner(c)==ruler&&!border(c,ruler))return c;return -1;}
    void add_officer(int city,int id){for(int i=0;i<12;++i)if(s[city*36+16+i]==255){s[city*36+16+i]=id;return;}throw std::runtime_error("AI destination roster is full");}
    void transfer(int from,int to,int slot){int id=s[from*36+16+slot];add_officer(to,id);s[from*36+16+slot]=255;}
    int population_average()const{int cities=0,officers=0;for(int c=0;c<30;++c)if(owner(c)==ruler){++cities;officers=(officers+count(c))&255;}return cities?(((officers/cities)*4)&255)/8:0;}
    void add_troops(int id){
        const int b=0x438+id*8;if((s[b+6]&15)>=10)return;
        const int mix=tables["troop_mix"][s[b+3]/10];
        if((s[b+4]>>4)<(mix&15))s[b+4]+=16;
        else if((s[b+7]&15)<(mix>>4))++s[b+7];else s[b+7]+=16;
        s[b+6]=(s[b+6]&240)|((s[b+4]>>4)+(s[b+7]&15)+(s[b+7]>>4));
    }
    void prepare(){
        // CBFD: move the ruler away from an exposed city when an interior exists.
        const int capital=seat();
        if(capital>=0&&border(capital,ruler)){
            int destination=-1,maximum=0;
            for(int c=interior(0);c>=0;c=interior(c+1)){int n=count(c);if(n>=maximum&&n<10){maximum=n;destination=c;}}
            if(destination>=0)for(int slot=0;slot<12;++slot)if(s[capital*36+16+slot]==ruler){transfer(capital,destination,slot);spend(2);break;}
        }
        // CCB0: at most one available historical candidate is added per city.
        for(int c=0;c<30;++c)if(owner(c)==ruler){
            if((random()&15)>=tables["recruit_threshold"][s[0xd88]].get<int>())continue;
            const int n=std::min<int>(8,std::uint8_t(s[0xd85]-199));
            for(int i=0;i<n;++i){int id=search["candidates"][c*8+i];if(id==255)break;if(!(s[0x438+id*8]&64))continue;
                if(count(c)<12){add_officer(c,id);s[0x438+id*8]&=31;spend(5);}break;
            }
        }
        // CD4A: owning the relevant terrain enables stamina recovery faction-wide.
        bool recovery=false;
        for(int c=0;c<30;++c)if(owner(c)==ruler){const int t=tables["terrain"][c],n=tables["terrain_count"][c];if((t&3)==3||(t&12)==12||(n>=1&&(t&48)==48)||(n>=2&&(t&192)==192)){recovery=true;break;}}
        if(recovery)for(int c=0;c<30;++c)if(s[c*36]==ruler)for(int i=0;i<12;++i){const int id=s[c*36+16+i];if(id==255)continue;const int stamina=state.rom_.officer(id)["stamina"];if(s[0x438+id*8+1]<stamina){s[0x438+id*8+1]=stamina;spend(5);}}
    }
    void choose_plan(){
        if(++round>=2)throw BudgetEnd{};
        int city_count=0,borders=0,weak=0;std::uint32_t gold=0;
        for(int c=0;c<30;++c)if(owner(c)==ruler){++city_count;gold=(gold+read(c*36+1,3))&0xffffff;for(const auto &link:links[c])if(owner(link["target"])!=ruler){borders=(borders+1)&255;if(count(link["target"])<4)weak=(weak+1)&255;}}
        std::array<int,3> weight;
        const int tier=tables["city_tiers"][city_count];
        const int ratio=borders?(100*weak/borders):65535;
        const int edge_tier=(ratio>>8)<31?0:2; // Original compares the quotient's high byte twice.
        const int gold_tier=gold<500?0:(gold<1000?1:(gold<2000?2:3));
        for(int i=0;i<3;++i)weight[i]=(tables["weights"][i*6+ruler].get<int>()+tables["city_bonus"][i*5+tier].get<int>()+tables["border_bonus"][i*3+edge_tier].get<int>()+tables["gold_bonus"][i*4+gold_tier].get<int>())&255;
        const int total=weight[0]+weight[1]+weight[2];
        const int first=total?100*weight[0]/total:255,second=total?first+100*weight[1]/total:254;
        const auto pick=[&](){int v=percent();return v<first?0:(v<second?1:2);};
        const int a=pick();int b;do{b=pick();}while(a==b);
        plan=(a<<6)|(b<<4)|((3-a-b)<<2);phase=1;
    }
    void military(){
        // C831-C9EC: redistribute officers, then fill the strongest eligible unit.
        for(int loops=0;loops<1000;++loops){
            const int capital=seat(),average=population_average();if(capital<0)throw NextAction{};
            int from=-1,n=0,to=-1;
            bool inland=true;
            for(int c=interior(0);c>=0;c=interior(c+1))if(c!=capital&&count(c)>=n){from=c;n=count(c);}
            if(from<0){inland=false;for(int c=frontier(0,ruler);c>=0;c=frontier(c+1,ruler))if(c!=capital&&count(c)>=2&&count(c)>=n){from=c;n=count(c);}}
            if(from>=0){
                if(count(capital)<average+3)to=capital;
                else if(inland||n>=average+2){int minimum=12;for(int c=frontier(0,ruler);c>=0;c=frontier(c+1,ruler))if(count(c)<minimum){minimum=count(c);to=c;}}
            }
            if(to>=0&&count(from)>=average+1){int slot=0;while(slot<12&&s[from*36+16+slot]==255)++slot;transfer(from,to,slot);spend(2);continue;}
            int city=-1,officer=-1,best=0;
            const auto examine=[&](int c){for(int i=0;i<12;++i){const int id=s[c*36+16+i];if(id==255||(s[0x438+id*8+6]&15)>=10)continue;int martial=s[0x438+id*8+3];if(martial>=best){best=martial;city=c;officer=id;}}};
            for(int c=frontier(0,ruler);c>=0;c=frontier(c+1,ruler))examine(c);
            examine(capital);if(officer<0)throw NextAction{};
            const int needed=10-(s[0x438+officer*8+6]&15);pay_small(city);
            for(int i=0;i<needed;++i)add_troops(officer);
            spend(3);domestic();return;
        }
        throw std::runtime_error("AI officer redistribution did not converge");
    }
    bool train_field(int id,int field,int gain){const int b=0x438+id*8+field,v=s[b];if(field==5?v>=69:(v<50||v>=79))return false;s[b]=std::min(99,v+gain);return true;}
    void training(){
        int best=0,id=0;
        for(int c=frontier(0,ruler);c>=0;c=frontier(c+1,ruler)){
            if(!pay(c,(random()&7)+20))throw NextAction{};
            for(int slot=0;slot<12;++slot){y=16+slot;int candidate=s[c*36+16+slot];if(candidate==255)continue;y=5;const int b=0x438+candidate*8,score=s[b+2]+s[b+3]+s[b+5];if(score>=best){best=score;id=candidate;}}
            y=28;
        }
        const int gain=(random()&3)+5;
        trace.push_back({{"point","train"},{"id",id},{"y",y},{"gain",gain},{"rng",cursor},{"budget",budget}});
        // CB87 is LDA $E9DA, not a random subroutine call; Y remains from scanning.
        const int choice=tables["training_choice"][y];
        const int field=choice==0?3:(choice==1?2:5);
        if(train_field(id,field,gain)||train_field(id,3,gain)||train_field(id,2,gain)||train_field(id,5,gain)){spend(5);return;}
        develop_ai();
    }
    void develop_ai(){
        const bool population=random()>=160;const int field=population?8:((s[0xd87]>=4&&s[0xd87]<10)?4:6);
        const int progress=(population?0xf26:field==4?0xf1a:0xf20)+ruler;
        int city=-1;
        for(int i=0;i<32;++i){if((s[progress]&31)>=30)s[progress]&=224;const int c=s[progress]&31;if(!population)++s[progress];if(owner(c)==ruler){city=c;break;}if(population)++s[progress];}
        if(city<0)throw std::runtime_error("AI has no owned city");
        trace.push_back({{"point","develop"},{"city",city},{"field",field},{"rng",cursor},{"budget",budget},{"gold",read(city*36+1,3)}});
        pay_small(city);
        const int gain=population?((random()&3)+10)*100:(random()&7)+16;
        write(city*36+field,population?3:2,read(city*36+field,population?3:2)+gain);
        if(population)++s[progress];
        spend(5);
    }
    void domestic(){for(int steps=0;steps<1000;++steps){const int mask=tables["training_mask"][ruler];if((mask>>(random()&7))&1)training();else develop_ai();}throw std::runtime_error("AI domestic budget did not converge");}
    void war();
public:
    OriginalAi(OriginalState &st,const Json &r):state(st),s(st.bytes_),tables(st.rom_.ai_tables()),search(st.rom_.search_tables()),links(st.rom_.world_map()["links"]),sequence(st.development_["random_sequence"]),ruler(s[0xd8b]),budget(r.at("budget")),phase(r.at("phase")),plan(r.at("plan")),round(r.at("round")),cursor(r.at("random_cursor")) {
        a1=r.value("a1",0);work_gold=r.value("work_gold",160u);
        if(ruler>=6||phase>2)throw std::invalid_argument("Invalid original AI state");
    }
    Json run(){
        try {
            prepare();
            if(phase==0)choose_plan();
            else if(phase==1)for(int actions=0;actions<1000;++actions){
                if((plan&3)==3){phase=0;prepare();choose_plan();break;}
                const int action=(plan>>(6-2*(plan&3)))&3;
                try {if(action==0)war();else if(action==1)military();else domestic();}
                catch(const NextAction&){++plan;continue;}
                break;
            }
        }catch(const BudgetEnd&){done=true;phase=2;result=0;}
        return {{"phase",phase},{"budget",budget},{"plan",plan},{"round",round},{"random_cursor",cursor},{"done",done},{"battle_result",result},{"source",source},{"target",target},{"winner",winner},{"a1",a1},{"trace",trace},{"work_gold",work_gold}};
    }
};
void OriginalAi::war(){
    const int difficulty=s[0xd88];
    if(s[0xd85]<201 && (difficulty==0 || (difficulty==1 && s[0xd87]<7)))throw NextAction{};
    int minimum=10;source=target=255;
    for(int city=frontier(0,ruler);city>=0;city=frontier(city+1,ruler))for(const auto &link:links[city]){
        const int neighbor=link["target"];bool blocked=false;
        for(int i=0;i<8;++i)if((s[0xc8d+city*8+i]&31)==neighbor)blocked=true;
        if(blocked||owner(neighbor)==ruler)continue;
        const int n=count(neighbor);
        trace.push_back({{"point","candidate"},{"source",city},{"target",neighbor},{"minimum",minimum},{"count",n},{"previous",target},{"rng",cursor}});
        bool select=n<minimum;int replacement=n;
        if(n==minimum){
            if(neighbor==target){replacement=count(source);select=count(city)>replacement;}
            else {const int roll=random();replacement=roll>>1;select=(roll&1)==0;}
        }
        // C022 stores the accumulator left by the tie breaker, not a fresh count.
        if(select){minimum=replacement;source=city;target=neighbor;}
    }
    trace.push_back({{"point","target"},{"source",source},{"target",target},{"rng",cursor},{"budget",budget}});
    if(source==255||target==255)throw std::runtime_error("Original AI has no selectable war target");
    const int capital=seat(),average=population_average();
    // C060-C181 gathers the strongest available officers at the attacking city.
    for(int loops=0;count(source)<count(target)+3;++loops){
        if(loops>=360)throw std::runtime_error("AI reinforcement loop");
        int from=-1,maximum=0;
        for(int c=interior(0);c>=0;c=interior(c+1))if(c!=capital&&c!=source&&count(c)>=maximum){from=c;maximum=count(c);}
        if(from<0||std::uint8_t(maximum-1)<a1){
            from=-1;maximum=0;
            for(int c=frontier(0,ruler);c>=0;c=frontier(c+1,ruler))if(c!=capital&&c!=source&&count(c)>=maximum){from=c;maximum=count(c);}
            if(from<0||maximum<average+1)throw NextAction{};
        }
        if(count(from)<2||count(source)>=12)throw NextAction{};
        int slot=-1,best=0;
        for(int i=0;i<12;++i){int id=s[from*36+16+i];if(id!=255&&id!=ruler&&s[0x438+id*8+3]>=best){best=s[0x438+id*8+3];slot=i;}}
        if(slot<0)throw std::runtime_error("AI reinforcement has no officer");
        transfer(from,source,slot);spend(2);
    }
    trace.push_back({{"point","reinforced"},{"source",source},{"target",target},{"rng",cursor},{"budget",budget}});
    const auto levy=[&](){int contributors=0;for(int c=0;c<30;++c)if(c!=capital&&owner(c)==ruler&&read(c*36+1,3)>=100){write(c*36+1,3,read(c*36+1,3)-100);++contributors;}return contributors;};
    for(int goal:{6,10})for(int slot=0;slot<12;++slot){
        const int id=s[source*36+16+slot];if(id==255)continue;
        const int needed=goal-(s[0x438+id*8+6]&15);if(needed<=0)continue;
        // The original multiply overwrites its city pointer with zero; the
        // subsequent debit affects scratch RAM, not the city gold record.
        while(work_gold<unsigned(needed*40)){
            if(!levy()){if(goal==6)throw NextAction{};goto troops_ready;}
        }
        work_gold-=((needed*40)&255);
        for(int i=0;i<needed;++i)add_troops(id);
        spend(3);
    }
troops_ready:
    spend(5);
    const int ledger=0xdaa;
    std::fill(s.begin()+ledger,s.begin()+ledger+57,255);
    std::fill(s.begin()+ledger+58,s.begin()+ledger+66,0);
    s[ledger+57]=(ruler<<4)|owner(target);
    // Original defender and attacker records use different widths.
    for(int tries=0;;++tries){a1=std::uint8_t(count(target)+(random()&3)-1);if(a1!=0&&std::uint8_t(count(source)-a1)!=0)break;if(tries>256)throw std::runtime_error("AI sortie selection");}
    int attackers=0;
    while(a1){
        int slot=-1,best=0,id=0;
        for(int i=0;i<12;++i){int current=s[source*36+16+i];if(current==255||current==ruler)continue;int martial=s[0x438+current*8+3];if(martial>=best){best=martial;slot=i;id=current;}}
        if(slot<0||attackers>=11)throw std::runtime_error("Original AI sortie exceeds available roster");
        s[ledger+24+attackers*3]=id;s[ledger+26+attackers*3]=source;s[source*36+16+slot]=255;++attackers;--a1;
    }
    const bool human=owner(target)==(s[0xd89]&7)||((s[0xd8a]&128)&&owner(target)==(s[0xd8a]&7));
    int defenders=0;
    for(int i=0;i<12;++i)if(s[target*36+16+i]!=255){s[ledger+defenders*2]=s[target*36+16+i];++defenders;if(!human)s[target*36+16+i]=255;}
    if(human){done=true;phase=2;result=3;return;}
    std::vector<int> attack,defend;
    for(int i=0;i<attackers;++i)attack.push_back(s[ledger+24+i*3]);
    for(int i=0;i<defenders;++i)defend.push_back(s[ledger+i*2]);
    const auto strength=[&](const std::vector<int>&group){int total=0;for(int id:group)total=(total+s[0x438+id*8+3]*(s[0x438+id*8+6]&15))&65535;return total;};
    const int offence=strength(attack),defence=strength(defend),denominator=(offence+defence)&65535;
    const std::uint32_t dividend=((offence*100)&65535)<<8;
    const int threshold=denominator?(dividend/denominator)&255:255;
    const bool victory=percent()<threshold;
    const auto casualties=[&](const std::vector<int>&group){for(int id:group){int b=0x438+id*8,n=s[b+1]*(s[b+6]&15)/100;s[b+7]=0;s[b+4]&=15;s[b+6]&=240;for(int i=0;i<n;++i)add_troops(id);}};
    casualties(attack);casualties(defend);
    const auto deaths=[&](const std::vector<int>&group){
        int strongest=0,best=0;for(int id:group)if(s[0x438+id*8+3]>=best){best=s[0x438+id*8+3];strongest=id;}
        for(int id:group)if(id>=6&&id!=strongest){const int b=0x438+id*8;if(percent()>=std::uint8_t(s[b+1]+20))s[b]=(s[b]&31)|32;}
        // C7CF reads the record again rather than clearing it; retain that behavior.
    };
    // C759/C78B scan through offset 57, also treating the packed faction
    // byte as an officer ID. Casualty/repatriation loops stop before it.
    auto mortality_attack=attack;
    if(s[ledger+57]!=255)mortality_attack.push_back(s[ledger+57]);
    deaths(mortality_attack);deaths(defend);
    const auto loyalty=[&](int id){int &unused=y;(void)unused;auto &v=s[0x438+id*8+5];if(v>=80)v=(random()&3)+20;else if(v>=60)v=(random()&7)+25;else if(v>=50)v=67;};
    if(victory){
        for(int id:attack)add_officer(target,id);
        for(int id:defend){
            if(id>=6&&percent()>=s[0x438+id*8+5]&&count(target)<12){add_officer(target,id);loyalty(id);continue;}
            int destination=-1,fewest=12;const int faction=owner(target);
            for(int c=frontier(0,faction);c>=0;c=frontier(c+1,faction))if(c!=target&&count(c)<fewest){destination=c;fewest=count(c);}
            if(destination>=0){add_officer(destination,id);continue;}
            const int b=0x438+id*8;
            if(id<6){const int conqueror=owner(source);for(int c=0;c<30;++c)if(owner(c)==faction)s[c*36]=(s[c*36]&248)|conqueror;s[b]=(s[b]&31)|32;}
            else {s[b]=(s[b]&31)|128;loyalty(id);}
        }
    }else{
        for(int id:defend)add_officer(target,id);
        for(int id:attack){if(id>=6&&percent()>=s[0x438+id*8+5]&&count(target)<12){add_officer(target,id);loyalty(id);}else add_officer(source,id);}
    }
    done=true;phase=2;result=victory?2:1;winner=victory?ruler:owner(target);
}
Json OriginalState::begin_ai()const {
    const int ruler=bytes_[0xd8b];if(ruler>=6)throw std::invalid_argument("Invalid AI ruler");
    const int multiplier=rom_.ai_tables()["budget_multiplier"][bytes_[0xd88]];
    return {{"phase",0},{"budget",(bytes_[0xd8f+4*ruler]*multiplier)&255},{"plan",0},{"round",0},{"random_cursor",0},{"done",false}};
}
Json OriginalState::ai_phase(const Json &runtime){auto next=*this;OriginalAi ai(next,runtime);auto result=ai.run();bytes_=std::move(next.bytes_);return result;}
}
