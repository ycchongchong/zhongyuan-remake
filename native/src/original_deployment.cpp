#include "zhongyuan/original_state.hpp"
#include <algorithm>
#include <set>
namespace zhongyuan {
void OriginalState::auto_deploy_defenders(int target) { auto_deploy(target,false); }
void OriginalState::auto_deploy_attackers(int target) { auto_deploy(target,true); }
void OriginalState::auto_deploy(int target,bool attacking) {
    const int count=attacking?11:12,base=attacking?0xdc2:0xdaa,stride=attacking?3:2,role=attacking?0xe04:0xdec;
    const auto table=rom_.deployment_tables(target);auto next=bytes_;
    int leader=-1;const int ruler=(next[0xdaa+57]>>(attacking?4:0))&15;
    for(int slot=count-1;slot>=0;--slot){
        if(next[base+slot*stride]==255)next[base+1+slot*stride]=255;
        next[role+slot*2]=next[role+1+slot*2]=255;
        if(next[base+slot*stride]==ruler&&leader<0)leader=slot;
    }
    // A0F7/AEAE: prefer the ruler; otherwise lower the loyalty threshold
    // by ten until a candidate exists. Ties select the earlier ledger slot.
    if(leader<0)for(int threshold=60;threshold>=0&&leader<0;threshold-=10){
        int best=0,chosen=-1;
        for(int slot=count-1;slot>=0;--slot){
            const int id=next[base+slot*stride];if(id==255)continue;
            if(id>=241)throw std::invalid_argument("Invalid battle officer");
            const int b=0x438+id*8;if(next[b+5]<threshold)continue;
            const int product=(next[b+3]>>2)*((next[b+6]&15)+1);
            const int score=product>=256?254:product;
            if(score>=best){best=score;chosen=slot;}
        }
        if(best!=255)leader=chosen;
    }
    if(leader<0)throw std::invalid_argument("电脑主将选择未收敛");
    if(leader)std::swap(next[base],next[base+leader*stride]);
    if(attacking){
        for(int slot=1;slot<11;++slot)if(next[base+slot*3]!=255)next[base+2+slot*3]&=127;
        next[base+2]|=128;
    }
    for(int slot=0;slot<count;++slot){
        if(next[base+slot*stride]==255)continue;
        next[base+1+slot*stride]=table[attacking?"attackers":"defenders"][slot].get<int>();
        next[role+slot*2]=slot==0?15:(attacking?1:(slot<5?2:(slot<11?3:1)));
        next[role+1+slot*2]=255;
    }
    bytes_=std::move(next);
}
Json OriginalState::deployment_area(int target,bool defending) const {
    const auto table=rom_.deployment_tables(target);Json area=Json::array();
    for(int cell=0;cell<160;++cell){
        if(cell<16){area.push_back(false);continue;}
        const int limit=table["row_limits"][cell/16-1];
        // B93C-B9AB, attacker side: bit 7 reverses the row's half-plane.
        area.push_back(((cell%16<(limit&31))!=((limit&128)!=0))!=defending);
    }
    return area;
}
void OriginalState::start_deployment_unit(int target,int slot,bool defending) {
    const int count=defending?12:11,base=defending?0xdaa:0xdc2,stride=defending?2:3;
    if(slot<0||slot>=count||bytes_[base+slot*stride]>=241||bytes_[base+1+slot*stride]!=255)
        throw std::invalid_argument("无效待部署部队");
    const auto table=rom_.deployment_tables(target);int position=-1;
    for(int i=15;i>=0;--i){
        const int cell=table[defending?"defenders":"attackers"][i];
        if(cell>=160)throw std::invalid_argument("无效部署候选位置");
        if(!(bytes_[0xe1a+cell]&16)){position=cell;break;}
    }
    if(position<0)throw std::invalid_argument("没有可用的部署位置");
    bytes_[base+1+slot*stride]=position;
    bytes_[0xe1a+position]=(defending?0x10:0x30)|slot|((!defending&&(bytes_[base+2+slot*stride]&128))?0x40:0);
}
int OriginalState::prepare_deployment(int source,int target,bool defending,bool two_players) {
    if(source<0||source>=30||target<0||target>=30)throw std::out_of_range("Battle city");
    auto next=*this;next.collect_defenders(target);
    const auto terrain=rom_.battlefield(target)["terrain"].get<std::vector<std::uint8_t>>();
    std::copy(terrain.begin(),terrain.end(),next.bytes_.begin()+0xe1a);
    std::copy(terrain.begin(),terrain.end(),next.bytes_.begin()+0xf1a);
    const auto origin=rom_.city(source)["map_position"],destination=rom_.city(target)["map_position"];
    next.bytes_[0xda5]=origin[0].get<int>()+8;next.bytes_[0xda7]=origin[1].get<int>()+8;
    next.bytes_[0xda6]=destination[0].get<int>()+8;next.bytes_[0xda8]=destination[1].get<int>()+8;
    if(defending&&!two_players)next.end_orders(); // D8EE: the attacking AI spends its remaining orders before deployment.
    if(!defending)next.auto_deploy_defenders(target);
    for(int i=0;!defending&&i<12;++i)if(next.bytes_[0xdaa+i*2]!=255){
        const int position=next.bytes_[0xdab+i*2];
        if(position>=160||(next.bytes_[0xe1a+position]&16))throw std::invalid_argument("守军布阵位置冲突");
        next.bytes_[0xe1a+position]=0x10|i;
    }
    int current=-1;
    for(int i=0;i<(defending?12:11);++i)if(next.bytes_[(defending?0xdaa:0xdc2)+i*(defending?2:3)]!=255){current=i;break;}
    if(current<0)throw std::invalid_argument("没有出征部队");
    next.start_deployment_unit(target,current,defending);*this=std::move(next);return current;
}
std::string OriginalState::move_deployment_unit(int target,int slot,int direction,bool defending) {
    const int count=defending?12:11,base=defending?0xdaa:0xdc2,stride=defending?2:3;
    if(slot<0||slot>=count||direction<0||direction>3||bytes_[base+slot*stride]>=241)return "无效布阵指令";
    const int old=bytes_[base+1+slot*stride];if(old<16||old>=160)return "部队尚未进入布阵";
    const int dx[]={1,-1,0,0},dy[]={0,0,1,-1};
    const int x=old%16+dx[direction],y=old/16+dy[direction];
    if(x<0||x>=16||y<1||y>=10)return "已到战场边界";
    const int cell=y*16+x;const auto area=deployment_area(target,defending);
    if(!area[cell].get<bool>())return "不能越出本方部署区域";
    if(bytes_[0xe1a+cell]&16)return "该位置已有部队";
    bytes_[base+1+slot*stride]=cell;
    bytes_[0xe1a+cell]=(defending?0x10:0x30)|slot|((!defending&&(bytes_[base+2+slot*stride]&128))?0x40:0);
    bytes_[0xe1a+old]=bytes_[0xf1a+old];return {};
}
bool OriginalState::valid_deployment(int source,int target,int current) const {
    if(source<0||source>=30||target<0||target>=30||current<-1||current>=11)return false;
    if(current>=0&&bytes_[0xdc2+current*3]>=241)return false;
    auto expected=*this;
    std::fill(expected.bytes_.begin()+0xdaa,expected.bytes_.begin()+0xdc2,255);
    expected.collect_defenders(target);expected.auto_deploy_defenders(target);
    for(int i=0;i<24;++i)if(bytes_[0xdaa+i]!=expected.bytes_[0xdaa+i]||bytes_[0xdec+i]!=expected.bytes_[0xdec+i])return false;
    auto terrain=rom_.battlefield(target)["terrain"].get<std::vector<std::uint8_t>>();
    if(!std::equal(terrain.begin(),terrain.end(),bytes_.begin()+0xf1a))return false;
    const auto origin=rom_.city(source)["map_position"],destination=rom_.city(target)["map_position"];
    if(bytes_[0xda5]!=origin[0].get<int>()+8||bytes_[0xda7]!=origin[1].get<int>()+8||bytes_[0xda6]!=destination[0].get<int>()+8||bytes_[0xda8]!=destination[1].get<int>()+8)return false;
    for(int i=0;i<12;++i)if(bytes_[0xdaa+i*2]!=255){
        const int position=bytes_[0xdab+i*2];if(position>=160||(terrain[position]&16))return false;
        terrain[position]=0x10|i;
    }
    const auto area=deployment_area(target);int count=0;
    for(int i=0;i<11;++i){
        const int position=bytes_[0xdc3+i*3];
        if(bytes_[0xdc2+i*3]==255||(current>=0&&i>current)){if(position!=255)return false;continue;}
        if(position>=160||!area[position].get<bool>()||(terrain[position]&16))return false;
        terrain[position]=0x30|i|((bytes_[0xdc4+i*3]&128)?0x40:0);++count;
    }
    return count>0&&std::equal(terrain.begin(),terrain.end(),bytes_.begin()+0xe1a);
}
void OriginalState::finish_defense_deployment(int target) {
    auto next=*this;next.auto_deploy_attackers(target);
    for(int i=0;i<11;++i)if(next.bytes_[0xdc2+i*3]!=255){
        const int cell=next.bytes_[0xdc3+i*3];
        if(cell>=160||(next.bytes_[0xe1a+cell]&16))throw std::invalid_argument("进攻部队布阵位置冲突");
        next.bytes_[0xe1a+cell]=0x30|i|((next.bytes_[0xdc4+i*3]&128)?0x40:0);
    }
    *this=std::move(next);
}
bool OriginalState::valid_defense_deployment(int source,int target,int current,const std::vector<std::uint8_t> &attackers) const {
    if(source<0||source>=30||target<0||target>=30||current<-1||current>=12||attackers.size()!=33)return false;
    if(current>=0&&bytes_[0xdaa+current*2]>=241)return false;
    std::set<int> ids;int attack_count=0;
    for(int i=0;i<11;++i){
        const int id=attackers[i*3],origin=attackers[i*3+2]&127;
        if(attackers[i*3+1]!=255)return false;
        if(id==255)continue;
        if(id>=241||!ids.insert(id).second||origin>=30||(bytes_[origin*36]&7)!=(bytes_[source*36]&7))return false;
        for(int c=0;c<30;++c)for(int j=0;j<12;++j)if(bytes_[c*36+16+j]==id)return false;
        ++attack_count;
    }
    if(!attack_count)return false;
    auto expected=*this;
    std::fill(expected.bytes_.begin()+0xdaa,expected.bytes_.begin()+0xdc2,255);
    expected.collect_defenders(target);
    auto terrain=rom_.battlefield(target)["terrain"].get<std::vector<std::uint8_t>>();
    if(!std::equal(terrain.begin(),terrain.end(),bytes_.begin()+0xf1a))return false;
    const auto origin=rom_.city(source)["map_position"],destination=rom_.city(target)["map_position"];
    if(bytes_[0xda5]!=origin[0].get<int>()+8||bytes_[0xda7]!=origin[1].get<int>()+8||bytes_[0xda6]!=destination[0].get<int>()+8||bytes_[0xda8]!=destination[1].get<int>()+8||bytes_[0xd8f+4*bytes_[0xd8b]]!=0)return false;
    const auto area=deployment_area(target,true);int count=0;
    for(int i=0;i<12;++i){
        const int id=bytes_[0xdaa+i*2],cell=bytes_[0xdab+i*2];
        if(id!=expected.bytes_[0xdaa+i*2])return false;
        if(id==255||(current>=0&&i>current)){if(cell!=255)return false;continue;}
        if(cell>=160||!area[cell].get<bool>()||(terrain[cell]&16))return false;
        terrain[cell]=0x10|i;++count;
    }
    std::copy(attackers.begin(),attackers.end(),expected.bytes_.begin()+0xdc2);
    if(current==-1){
        expected.auto_deploy_attackers(target);
        for(int i=0;i<22;++i)if(bytes_[0xe04+i]!=expected.bytes_[0xe04+i])return false;
        for(int i=0;i<11;++i)if(bytes_[0xdc2+i*3]!=255){
            const int cell=bytes_[0xdc3+i*3];if(cell>=160||(terrain[cell]&16))return false;
            terrain[cell]=0x30|i|((bytes_[0xdc4+i*3]&128)?0x40:0);
        }
    }
    for(int i=0;i<33;++i)if(bytes_[0xdc2+i]!=expected.bytes_[0xdc2+i])return false;
    return count>0&&std::equal(terrain.begin(),terrain.end(),bytes_.begin()+0xe1a);
}

bool OriginalState::valid_pvp_deployment(int source,int target,int current,bool defending,bool handover) const {
    if(source<0||source>=30||target<0||target>=30||current<-1||current>=(defending?12:11))return false;
    if((handover&&(defending||current!=-1))||(defending&&current==-1))return false;
    const int base=defending?0xdaa:0xdc2,stride=defending?2:3;
    if(current>=0&&bytes_[base+current*stride]>=241)return false;
    auto expected=*this;
    std::fill(expected.bytes_.begin()+0xdaa,expected.bytes_.begin()+0xdc2,255);
    expected.collect_defenders(target);
    for(int i=0;i<12;++i)if(bytes_[0xdaa+i*2]!=expected.bytes_[0xdaa+i*2])return false;
    auto terrain=rom_.battlefield(target)["terrain"].get<std::vector<std::uint8_t>>();
    if(!std::equal(terrain.begin(),terrain.end(),bytes_.begin()+0xf1a))return false;
    const auto origin=rom_.city(source)["map_position"],destination=rom_.city(target)["map_position"];
    if(bytes_[0xda5]!=origin[0].get<int>()+8||bytes_[0xda7]!=origin[1].get<int>()+8||bytes_[0xda6]!=destination[0].get<int>()+8||bytes_[0xda8]!=destination[1].get<int>()+8)return false;
    for(bool side:{true,false}){
        const auto area=deployment_area(target,side);int count=0;
        for(int i=0;i<(side?12:11);++i){
            const int b=(side?0xdaa:0xdc2)+i*(side?2:3),id=bytes_[b],cell=bytes_[b+1];
            const bool placed=side?(!defending||i<=current):(!defending&&!handover&&(current==-1||i<=current));
            if(id==255||!placed){if(cell!=255)return false;continue;}
            if(id>=241||cell>=160||!area[cell].get<bool>()||(terrain[cell]&16))return false;
            terrain[cell]=(side?0x10:0x30)|i|((!side&&(bytes_[b+2]&128))?0x40:0);++count;
        }
        if((side||(!defending&&!handover))&&count==0)return false;
    }
    return std::equal(terrain.begin(),terrain.end(),bytes_.begin()+0xe1a);
}

// D354-D395: an ordinary attacking unit returns to the first home roster hole.
Json OriginalState::retreat_attacker(int slot) {
    if(slot<0||slot>=11)return {{"error","无效撤退部队"}};
    const int b=0xdc2+slot*3,id=bytes_[b],city=bytes_[b+2]&31,position=bytes_[b+1];
    if(id>=241||city>=30||position>=160)return {{"error","无效撤退记录"}};
    int vacancy=-1;for(int i=0;i<12;++i)if(bytes_[city*36+16+i]==255){vacancy=i;break;}
    if(vacancy<0)return {{"returned",false},{"officer",id},{"city",city}};
    bytes_[city*36+16+vacancy]=id;bytes_[0xe1a+position]=bytes_[0xf1a+position];
    std::fill(bytes_.begin()+b,bytes_.begin()+b+3,255);
    return {{"returned",true},{"officer",id},{"city",city}};
}
// D4D3-D5C2: defending units choose the least populated friendly city, lowest
// city index on ties, without an adjacency test. Human failure costs no mobility.
Json OriginalState::retreat_defender(int slot,int target,bool human,std::uint8_t &cursor) {
    if(slot<0||slot>=12||target<0||target>=30)return {{"error","无效守军撤退指令"}};
    const int b=0xdaa+slot*2,id=bytes_[b],position=bytes_[b+1],owner=bytes_[0xde3]&15;
    if(id>=241||position>=160||owner>=6)return {{"error","无效守军撤退记录"}};
    // Validate before any mutation; legal defending records also occur in the city.
    int origin=-1;for(int i=0;i<12;++i)if(bytes_[target*36+16+i]==id){origin=i;break;}
    if(origin<0)return {{"error","守军不在守城名册中"}};
    int city=-1,count=13,vacancy=-1;
    for(int c=0;c<30;++c){
        if(c==target||(bytes_[c*36]&7)!=owner)continue;
        int n=0;for(int i=0;i<12;++i)n+=bytes_[c*36+16+i]!=255;
        if(n<count){count=n;city=c;}
    }
    if(city>=0)for(int i=0;i<12;++i)if(bytes_[city*36+16+i]==255){vacancy=i;break;}
    if(vacancy<0&&(human||(id<6&&city<0)))
        return {{"departed",false},{"returned",false},{"officer",id},{"city",city}};
    int removed=id;
    if(vacancy>=0||id<6)bytes_[city*36+16+(vacancy<0?0:vacancy)]=id;
    else{
        auto &loyalty=bytes_[0x438+id*8+5];
        if(loyalty>=80)loyalty=20+(development_["random_sequence"][cursor++].get<int>()&3);
        else if(loyalty>=60)loyalty=25+(development_["random_sequence"][cursor++].get<int>()&7);
        else if(loyalty>=50)loyalty=67;
        bytes_[0x438+id*8]|=128;
        // D447 leaves the updated flag byte in A. D558 literally searches for
        // that byte, and clears city offset 1C if not found, rather than the id.
        removed=bytes_[0x438+id*8];
    }
    int at=16;while(at<28&&bytes_[target*36+at]!=removed)++at;
    bytes_[target*36+at]=255;
    bytes_[0xe1a+position]=bytes_[0xf1a+position];bytes_[b]=bytes_[b+1]=255;
    return {{"departed",true},{"returned",vacancy>=0||id<6},{"officer",id},{"city",city}};
}
// D3E7-D446: commander retreat processes highest loyalty first, later slot on ties.
Json OriginalState::retreat_army_step(std::uint8_t &cursor) {
    int chosen=-1,loyalty=-1;
    for(int i=0;i<11;++i){const int id=bytes_[0xdc2+i*3];if(id==255)continue;
        if(id>=241)return {{"error","无效撤退武将"}};
        const int value=bytes_[0x438+id*8+5];if(value>=loyalty){loyalty=value;chosen=i;}}
    if(chosen<0)return {{"done",true},{"chosen",255}};
    const int b=0xdc2+chosen*3,id=bytes_[b],city=bytes_[b+2]&31;
    if(city>=30||bytes_[b+1]>=160)return {{"error","无效来源城池或战场位置"}};
    int vacancy=-1;for(int i=0;i<12;++i)if(bytes_[city*36+16+i]==255){vacancy=i;break;}
    if(vacancy>=0)bytes_[city*36+16+vacancy]=id;
    else{
        // AF84 preserves its literal LDA $E9D4 (opcode $20) for loyalty 50..59.
        if(loyalty>=80)loyalty=20+(development_["random_sequence"][cursor++].get<int>()&3);
        else if(loyalty>=60)loyalty=25+(development_["random_sequence"][cursor++].get<int>()&7);
        else if(loyalty>=50)loyalty=67;
        bytes_[0x438+id*8+5]=loyalty;bytes_[0x438+id*8]|=128;
    }
    // C7CB restores the old terrain even if a blinking sprite already hid the unit.
    bytes_[0xe1a+bytes_[b+1]]=bytes_[0xf1a+bytes_[b+1]];
    std::fill(bytes_.begin()+b,bytes_.begin()+b+3,255);
    return {{"done",false},{"chosen",24+chosen*3},{"officer",id},{"city",city},{"returned",vacancy>=0}};
}
// D8F4-D913, after the result flow has finished all troop/ownership transfers.
void OriginalState::close_battle(int target) {
    if(target<0||target>=30)throw std::out_of_range("Battle target");
    std::fill(bytes_.begin()+0xdaa,bytes_.begin()+0xdaa+59,255);
    bytes_[target*36+32]=255;bytes_[target*36+33]=255;
}

// CAA8-CAD3: change only the two formation bits; arrows clamp at either end.
std::string OriginalState::tactical_formation(int slot,int direction,int side) {
    if((side!=0&&side!=128)||slot<0||slot>=(side?11:12)||direction<0||direction>1||bytes_[(side?0xdc2+slot*3:0xdaa+slot*2)]>=241)return "无效阵容指令";
    auto &value=bytes_[0x438+bytes_[(side?0xdc2+slot*3:0xdaa+slot*2)]*8+4];
    const int formation=std::clamp((value&3)+(direction==0?1:-1),0,3);
    value=(value&252)|formation;return {};
}
// C8FA-C953: select any opposing occupied cell and spend one mobility point.
Json OriginalState::tactical_scout(int slot,int cell,int &points,int side) const {
    if((side!=0&&side!=128)||slot<0||slot>=(side?11:12)||cell<0||cell>=160||points<1||points>40||bytes_[(side?0xdc2+slot*3:0xdaa+slot*2)]>=241)return {{"error","请选择武将和敌军位置，侦察需要 1 点机动力"}};
    const int token=bytes_[0xe1a+cell],target=token&15;
    if(!(token&16)||bool(token&32)==bool(side))return {{"error","请选择敌军部队"}};
    if(target>=(side?12:11)||bytes_[side?0xdaa+target*2:0xdc2+target*3]>=241||bytes_[side?0xdab+target*2:0xdc3+target*3]!=cell)return {{"error","无效敌军记录"}};
    --points;return {{"slot",slot},{"cell",cell},{"target_slot",target},{"officer",bytes_[side?0xdaa+target*2:0xdc2+target*3]}};
}

// D5D3-D6B8. Direction API follows movement: right, left, down, up.
Json OriginalState::tactical_attack(int slot,int direction,int &points,int side) const {
    if((side!=0&&side!=128)||slot<0||slot>=(side?11:12)||direction<0||direction>3||points<0||points>40||bytes_[(side?0xdc2+slot*3:0xdaa+slot*2)]>=241)return {{"error","无效攻击指令"}};
    if(points<3)return {{"error","攻击需要 3 点机动力"}};
    const int b=(side?0xdc2+slot*3:0xdaa+slot*2),position=bytes_[b+1];
    if(position<16||position>=160)return {{"error","无效战场位置"}};
    const int delta[]={1,-1,16,-16};
    // The original adds a signed byte to the packed cell, including row wrap.
    const int target=(position+delta[direction])&255,token=bytes_[0xe1a+target];
    if(!(token&16)||bool(token&32)==bool(side))return {{"error","该方向没有敌人"}};
    const int enemy=token&15;
    if(enemy>=(side?12:11)||bytes_[side?0xdaa+enemy*2:0xdc2+enemy*3]>=241||bytes_[side?0xdab+enemy*2:0xdc3+enemy*3]!=target)return {{"error","无效敌军记录"}};
    points-=3;
    return {{"slot",slot},{"direction",direction},{"target_slot",enemy},{"target_cell",target},
        {"attacker",bytes_[b]},{"defender",bytes_[side?0xdaa+enemy*2:0xdc2+enemy*3]},
        {"attacker_token",(side?0x30|slot|((bytes_[b+2]&128)?0x40:0):0x10|slot)},{"defender_token",token},
        {"terrain_pair",((bytes_[0xf1a+position]&7)<<3)|(bytes_[0xf1a+target]&7)},{"stage","confirm"}};
}
// D6D5-D745: NPC formation uses the frame byte, then terrain may swap sides.
Json OriginalState::prepare_clash(const Json &attack,bool human_defender,std::uint8_t frame,bool human_attacker) {
    const int defender=attack.at("defender"),attacker=attack.at("attacker"),pair=attack.at("terrain_pair");
    if(defender<0||defender>=241||attacker<0||attacker>=241)throw std::out_of_range("clash officer");
    auto result=rom_.clash_terrain(pair);
    // D6D5/C35E first checks the initiating side. A computer initiator
    // randomizes its own formation; otherwise a computer target randomizes.
    if(!human_attacker||!human_defender){const int npc=human_attacker?defender:attacker;
        auto &formation=bytes_[0x438+npc*8+4];formation=(formation&252)|(frame&3);}

    const bool swapped=(result["orientation"].get<int>()&128)!=0;
    result["first"]=swapped?defender:attacker;result["second"]=swapped?attacker:defender;
    result["first_token"]=attack[swapped?"defender_token":"attacker_token"];
    result["second_token"]=attack[swapped?"attacker_token":"defender_token"];
    result["phase"]=0;result["counter"]=0;return result;
}

// 9BD7-9D64: 22 records [type, stamina, cell]; empty records retain all FF.
Json OriginalState::clash_units(int first,int second) const {
    if(first<0||first>=241||second<0||second>=241||first==second)return {{"error","无效交战武将"}};
    std::array<std::uint8_t,66> units;units.fill(255);
    for(int side=0;side<2;++side){
        const int officer=side?second:first,b=0x438+officer*8;
        const int counts[]={bytes_[b+4]>>4,bytes_[b+7]&15,bytes_[b+7]>>4};
        if(counts[0]+counts[1]+counts[2]>10)return {{"error","交战部队超过原版十队上限"}};
        const int formation=bytes_[b+4]&3;
        auto put=[&](int slot,int type){const int offset=side*33+slot*3;
            units[offset]=type|(side?128:0);units[offset+1]=bytes_[b+1];units[offset+2]=rom_.clash_position(side,formation,slot);};
        put(0,3);int slot=1;
        for(int kind=0;kind<3;++kind)for(int index=0;index<counts[kind];++index)put(slot++,(index<<2)|kind);
    }
    return units;
}
std::string OriginalState::clash_damage(Json &units,int first,int second,int attacker,int target,std::uint8_t &cursor) const {
    auto valid_token=[](int value){return value>=0&&value<=138&&(value&0x70)==0&&(value&15)<11;};
    if(first<0||first>=241||second<0||second>=241||first==second||!valid_token(attacker)||!valid_token(target)||!((attacker^target)&128))return "无效交战对象";
    if(!units.is_array()||units.size()!=66)return "无效交战兵队";
    for(const auto &v:units)if(!v.is_number_integer()||v<Json(0)||v>Json(255))return "无效交战兵队数值";
    const int a=(attacker&15)*3+((attacker&128)?33:0),t=(target&15)*3+((target&128)?33:0);
    if(units[a]==255||units[t]==255||units[a+1]==0||units[a+1]==255||units[t+1]==0||units[t+1]==255)return "交战兵队已失效";
    const int martial=bytes_[0x438+((attacker&128)?second:first)*8+3];
    const int half=martial>>1,quarter=martial>>2,eighth=martial>>3;
    const int kind=units[a].get<int>()&3,target_kind=units[t].get<int>()&3;
    int damage=quarter;
    if(kind==1)damage=martial>=95?half:quarter+eighth;
    else if(kind==2)damage=eighth+(eighth>>1);
    else if(kind==3){
        damage=half;
        // A82D compares A (half martial), not X (the >=95 tier).
        if(martial>=90)damage=half+(half==2?quarter:eighth);
    }
    const int random=development_["random_sequence"][cursor].get<int>()&7;
    damage=std::max(0,((damage+random+1)&255)-4);
    // A858 tests the slot's low two bits, so slots 4 and 8 also double damage.
    if((attacker&3)==0)damage=(damage*2)&255;
    if((target&15)==0)damage>>=1;
    if(target_kind==2)damage=(damage*2)&255;
    if(target_kind==1){damage>>=1;damage+=damage>>1;}
    damage=(damage+1)&255;
    units[t+1]=std::max(0,units[t+1].get<int>()-damage);++cursor;return {};
}

// AD13-AE0A: surviving troop counts and stamina, or general death and ledger removal.
std::string OriginalState::settle_clash(const Json &units,int first,int second,int first_token,int second_token,int target_city){
    if(first<0||first>=241||second<0||second>=241||first==second||target_city<0||target_city>=30)return "无效交战结果";
    if(!units.is_array()||units.size()!=66)return "无效交战兵队";
    for(const auto &v:units)if(!v.is_number_integer()||v<Json(0)||v>Json(255))return "无效交战兵队数值";
    auto next=bytes_;
    for(int side=0;side<2;++side){
        const int token=side?second_token:first_token,officer=side?second:first,slot=token&15;
        const bool attacker=(token&32)!=0;
        if(token<0||token>127||!(token&16)||slot>=(attacker?11:12))return "无效交战登记";
        const int ledger=attacker?0xdc2+slot*3:0xdaa+slot*2,b=0x438+officer*8,unit=side*33;
        if(next[ledger]!=officer)return "交战登记与武将不一致";
        if(units[unit]==255){
            if(!attacker){int resident=0;
                while(resident<12&&next[target_city*36+16+resident]!=officer)++resident;
                if(resident==12)return "守军不在目标城中";
                next[target_city*36+16+resident]=255;
            }
            next[b]=32;next[ledger]=255;next[ledger+1]=255;
        }else{
            int counts[3]={};int total=0;
            for(int i=1;i<11;++i)if(units[unit+i*3]!=255){
                const int kind=units[unit+i*3].get<int>()&3;
                if(kind==3)return "普通兵队类型无效";
                ++counts[kind];++total;
            }
            next[b+1]=units[unit+1];next[b+4]=(next[b+4]&15)|(counts[0]<<4);
            next[b+6]=(next[b+6]&240)|total;next[b+7]=(counts[2]<<4)|counts[1];
        }
    }
    bytes_=std::move(next);return {};
}

// Phase 11, A009/ADCB/AEE3: the tactical map is reloaded after the clash.
std::string OriginalState::restore_tactical_terrain(int target){
    if(target<0||target>=30)return "无效战术地图";
    const auto terrain=rom_.battlefield(target)["terrain"].get<std::vector<std::uint8_t>>();
    std::copy(terrain.begin(),terrain.end(),bytes_.begin()+0xe1a);
    std::copy(terrain.begin(),terrain.end(),bytes_.begin()+0xf1a);
    return {};
}
// BA08-BA6D: repaint a surviving tactical ledger entry after the terrain reload.
std::string OriginalState::repaint_tactical_unit(int token){
    if(token<0||token>138||(token&0x70)||(token&15)>=((token&128)?11:12))return "无效战术登记编号";
    const int slot=token&15,b=(token&128)?0xdc2+slot*3:0xdaa+slot*2;
    if(bytes_[b]>=241||bytes_[b+1]<16||bytes_[b+1]>=160)return "战术部队不存在或尚未部署";
    bytes_[0xe1a+bytes_[b+1]]=((token&128)?0x30:0x10)|slot|(((token&128)&&(bytes_[b+2]&128))?64:0);
    return {};
}

std::string OriginalState::restore_tactical_board(int target){
    auto next=*this;
    auto error=next.restore_tactical_terrain(target);if(!error.empty())return error;
    // The original redraw visits defender slots before attacker slots. This
    // helper requires all remaining troops to have completed deployment.
    for(int side:{0,128})for(int slot=0;slot<(side?11:12);++slot){
        const int b=side?0xdc2+slot*3:0xdaa+slot*2;
        if(next.bytes_[b]==255)continue;
        error=next.repaint_tactical_unit(side+slot);if(!error.empty())return error;
    }
    bytes_=std::move(next.bytes_);return {};
}

int OriginalState::tactical_mobility() const {
    int count=0;for(int i=0;i<11;++i)count+=bytes_[0xdc2+i*3]!=255;
    return rom_.tactical_mobility(count);
}
std::string OriginalState::tactical_step(int slot,int direction,int &points,int side) {
    if((side!=0&&side!=128)||slot<0||slot>=(side?11:12)||direction<0||direction>3||points<0||points>40||bytes_[(side?0xdc2+slot*3:0xdaa+slot*2)]>=241)return "无效战术移动指令";
    const int b=(side?0xdc2+slot*3:0xdaa+slot*2),old=bytes_[b+1];
    if(old<16||old>=160)return "无效战场位置";
    const int dx[]={1,-1,0,0},dy[]={0,0,1,-1};
    const int x=old%16+dx[direction],y=old/16+dy[direction];
    if(x<0||x>=16||y<1||y>=10)return "已到战场边界";
    const int cell=y*16+x;
    if(bytes_[0xe1a+cell]&16)return "该位置已有部队";
    const int cost=rom_.tactical_step_cost(bytes_[0x438+bytes_[b]*8+6],bytes_[0xe1a+cell]);
    if(points<cost)return "剩余机动力不足";
    points-=cost;bytes_[b+1]=cell;
    bytes_[0xe1a+cell]=(side?0x30|slot|((bytes_[b+2]&128)?0x40:0):0x10|slot);
    bytes_[0xe1a+old]=bytes_[0xf1a+old];return {};
}

}
