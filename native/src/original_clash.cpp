#include "zhongyuan/original_state.hpp"
#include <algorithm>
#include <cmath>

namespace zhongyuan {
namespace {
bool byte_array(const Json &value,std::size_t size){
    if(!value.is_array()||value.size()!=size)return false;
    for(const auto &v:value)if(!v.is_number_integer()||v<Json(0)||v>Json(255))return false;
    return true;
}
bool unit_token(int value){return value>=0&&value<=138&&(value&0x70)==0&&(value&15)<11;}
int record(int token){return (token&15)*3+((token&128)?33:0);}
bool byte_fields(const Json &value,std::initializer_list<const char *> keys){
    if(!value.is_object())return false;
    for(const auto *key:keys)if(!value.contains(key)||!value[key].is_number_integer()||value[key]<Json(0)||value[key]>Json(255))return false;
    return true;
}
}

// Scene load, 9ACE initialization, roster/occupancy and A152/A17E human prompt.
// Stops before either player's initial order menu; no animation frames are implied.
Json OriginalState::initialize_clash(const Json &context){
    if(!byte_fields(context,{"first","second","map","orientation","tactical_side","human_mask"})||
       context["map"]>15||(context["tactical_side"]!=0&&context["tactical_side"]!=128))return {{"error","无效交战初始化"}};
    const int mask=context["human_mask"],orientation=context["orientation"],tactical=context["tactical_side"];
    if((mask&15)>2||(mask>>4)>2||mask==0)return {{"error","交战初始化需要玩家一方"}};
    auto units=clash_units(context["first"],context["second"]);if(units.is_object())return units;
    const int controller=((orientation&128)^(tactical?0:128))?mask&3:(mask>>4)&3;
    Json result={{"units",units},{"orders",Json::array({0,0,0,0,0,0,0,0})},
        {"phase",1},{"counter",controller?15:16},{"active",128},{"cycle",0},
        {"countdown",((tactical?mask>>4:mask)&15)?7:8}};
    const auto terrain=rom_.clash_map(context["map"]);
    for(int i=0;i<160;++i)bytes_[0xe1a+i]=bytes_[0xf1a+i]=terrain[i].get<std::uint8_t>();
    for(int side=0;side<2;++side)for(int slot=0;slot<11;++slot){
        const int b=side*33+slot*3;if(units[b]==255)continue;
        bytes_[0xe1a+units[b+2].get<int>()]=(side?128:0)|64|slot;
    }
    return result;
}

// One logic boundary of A2C2/A30C/A31E, without input polling or sprite frames.
// Movement/attack animation counters are advanced to their completion boundary;
// projectile flight retains its original two-pixel collision steps. Commit only
// after every called kernel succeeds. Unimplemented global phases remain held.
Json OriginalState::clash_step(const Json &runtime,const Json &context,std::uint8_t &cursor){
    if(!byte_fields(context,{"first","second","map","orientation","tactical_side","human_mask","second_token"})||
       context["first"]>=241||context["second"]>=241||context["first"]==context["second"]||context["map"]>15||
       (context["tactical_side"]!=0&&context["tactical_side"]!=128)||
       !byte_fields(runtime,{"global_phase","phase","counter","active","extra","direction","distance","x","y","target","command","sequential","ai_status","ai_mode","ai_side","cycle","countdown"})||
       !runtime.contains("units")||!byte_array(runtime["units"],66)||!runtime.contains("orders")||!byte_array(runtime["orders"],8)||
       runtime["extra"]>3)return {{"error","无效交锋执行状态"}};
    if(runtime["global_phase"]!=10||runtime["phase"]<2||runtime["phase"]>4)return {{"error","当前交锋阶段尚未接入执行"}};
    auto candidate=*this;auto next=runtime;auto next_cursor=cursor;auto &units=next["units"];
    const int phase=next["phase"],counter=next["counter"],active=next["active"];
    Json result;
    if(phase==2){
        auto input=context;input.update(next);
        result=candidate.clash_handover(units,input);
    }else if(phase==3){
        auto input=context;input.update(next);input["scene"]=context["map"];
        result=candidate.clash_strategy(units,next["orders"],input,next_cursor);
        if(result.value("boundary",std::string{})=="ai_scratch"){
            next["boundary"]="ai_scratch";return next; // No invented scratch bytes or RNG consumption.
        }
        if(!result.contains("error"))result.update({{"phase",4},{"counter",0}});
    }else{
        if(!unit_token(active)||units[record(active)]==255||units[record(active)+2]<16||units[record(active)+2]>=160)
            return {{"error","当前兵队不存在"}};
        auto end=[&](bool moved){return candidate.clash_end_action(units,context["first"],context["second"],context["map"],active,next["extra"],moved);};
        if(counter==0)result={{"counter",1}};
        else if(counter==1){
            result=candidate.clash_order(units,next["orders"],active,context["second_token"],next["command"],next_cursor);
            if(result.contains("error"))return result;
            next.update(result);
            if(result["phase"]==4){
                const int command=result["command"],bits=command>>4;
                const int direction=bits==1?0:bits==2?1:bits==4?2:bits==8?3:-1;
                if(bits&&direction<0)return {{"error","无效交锋方向"}};
                if(bits){ // A44D uses direction, not army ownership, in the high bits.
                    static const int masks[]={191,255,63,63},faces[]={0,64,128,0};
                    units[record(active)]=(units[record(active)].get<int>()&masks[direction])|faces[direction];
                }
                if(command&1){
                    if(bits){
                        next["direction"]=direction;next["distance"]=0;
                        const auto error=candidate.clash_move(units,active,direction);
                        // An obstructed move stays at the decision boundary in the ROM.
                        if(error.empty())next["counter"]=2;
                    }
                }else if(command&2){
                    if(bits){
                        next.update({{"direction",direction},{"distance",0},{"x",0},{"y",0}});
                        auto attack=candidate.clash_attack(units,active,command,next["target"]);
                        if(attack.contains("error"))return attack;
                        next["global_phase"]=attack["phase"];next["target"]=attack["target"];next["counter"]=attack["counter"];
                    }
                }else if(command&4){
                    result=end(false);if(result.contains("error"))return result;next.update(result);
                }
            }
            result=Json::object();
        }else if(counter==2){next["distance"]=32;result=end(true);}
        else if(counter==3){
            const auto error=candidate.clash_damage(units,context["first"],context["second"],active,next["target"],next_cursor);
            if(!error.empty())return {{"error",error}};
            next["distance"]=48;result={{"counter",4}};
        }else if(counter==4){
            const auto error=candidate.clash_resolve_hit(units,active,next["target"],context["orientation"],context["tactical_side"]);
            if(!error.empty())return {{"error",error}};
            result={{"counter",5}};
        }else if(counter==5)result=end(false);
        else if(counter==6||counter==7){
            result=candidate.clash_projectile(units,active,next["direction"],next,counter==6);
        }else if(counter==8){
            const int target=next["target"];
            if(!unit_token(target))return {{"error","投射物旧目标编号无效"}};
            // A9A8 repaints the previous target even on a miss. A departed unit
            // retains position FF; the original writes the off-board map byte.
            candidate.bytes_[0xe1a+units[record(target)+2].get<int>()]=target|64;
            result=end(false);
        }else return {{"error","当前军令菜单或交锋演出尚未接入执行"}};
    }
    if(result.contains("error"))return result;
    next.update(result);*this=std::move(candidate);cursor=next_cursor;return next;
}

// AA00-AAB6, accepted input byte: A=1, B=2, Up=16, Down=32.
// Input polling, sound and drawing are outside this deterministic menu operation.
Json OriginalState::clash_menu(const Json &orders,const Json &context,int input) const {
    if(!byte_array(orders,8)||!byte_fields(context,{"first","second","side","row","return_counter"})||
       context["first"]>=241||context["second"]>=241||context["row"]>3||context["return_counter"]>15||
       (context["side"]!=0&&context["side"]!=128)||input<0||input>255)return {{"error","无效交战军令菜单"}};
    for(int i=0;i<8;++i){
        const int officer=context[i<4?"first":"second"];
        if(orders[i]>(i%4==3&&officer>=6?3:2))return {{"error","兵种或君主军令不合法"}};
    }
    auto next=orders;int row=context["row"];
    if(input&32)row=(row+1)&3;else if(input&16)row=(row+3)&3;
    if(input&1){
        const int side=context["side"],officer=context[side?"second":"first"],index=(side?4:0)+row;
        next[index]=(next[index].get<int>()+1)%(row==3&&officer>=6?4:3);
    }
    const bool close=input&2;const int ret=context["return_counter"];
    return {{"orders",next},{"row",row},{"phase",close&&ret?1:4},{"counter",close?ret:11}};
}

// B05F-B573: per-unit army orders. Directions in the ROM are up, down, left, right.
// This returns a decision only; frame animation and command execution are separate.
Json OriginalState::clash_order(const Json &units,const Json &orders,int active,int second_token,int command,std::uint8_t &cursor) const {
    if(!byte_array(units,66)||!byte_array(orders,8)||!unit_token(active)||second_token<0||second_token>255||command<0||command>255)return {{"error","无效交战军令"}};
    for(const auto &v:orders)if(v>Json(3))return {{"error","无效兵种军令"}};
    const int offset=record(active),kind=units[offset].get<int>()&3,side=(active&128)?1:0;
    if(units[offset]==255||units[offset+1]==0||units[offset+1]==255||units[offset+2]<Json(16)||units[offset+2]>=Json(160))return {{"error","当前兵队不可行动"}};
    const int position=units[offset+2];
    if(units[2]<Json(16)||units[2]>=Json(160)||units[35]<Json(16)||units[35]>=Json(160))return {{"error","主将已离开交战场"}};
    auto result=[&](int phase,int who){return Json{{"command",command},{"active",who},{"phase",phase},{"counter",phase==4?1:0}};};
    if(orders[3]==3)return result(6,0);
    if(orders[7]==3)return result(6,128);
    const int mode=orders[side*4+kind];
    auto random=[&](){return development_["random_sequence"][cursor++].get<int>();};
    const int delta[]={-16,16,-1,1};
    const int move_command[]={0x11,0x21,0x41,0x81},attack_command[]={0x12,0x22,0x42,0x82};
    auto cell_in_direction=[&](int cell,int direction){
        if((direction==2&&(cell&15)==0)||(direction==3&&(cell&15)==15))return -1;
        const int target=cell+delta[direction];return target>=16&&target<160?target:-1;
    };
    auto neighboring=[&](int direction){const int cell=cell_in_direction(position,direction);return cell<0?255:int(bytes_[0xe1a+cell]);};
    auto empty=[&](int tile){return !(tile&0xc0)&&(tile&63)<24;};
    auto enemy=[&](int tile){return tile!=255&&(tile&0xc0)&&((tile^active)&128);};
    // B492 checks five squares in a cursor-dependent direction order. Friendlies
    // do not block the ray; blocking terrain terminates only that direction.
    auto shoot=[&](){
        static const int directions[4][4]={{3,2,1,0},{2,3,0,1},{1,0,3,2},{0,1,2,3}};
        const int choice=random()&3;
        for(int d:directions[choice]){
            command=attack_command[d];int cell=position;
            for(int distance=0;distance<5;++distance){
                cell=cell_in_direction(cell,d);if(cell<0)break;
                const int tile=bytes_[0xe1a+cell];
                if(tile&0xc0){if((tile^active)&128)return true;}
                else if(tile>=24)break;
            }
        }
        return false;
    };
    auto attack_or_wait=[&](){
        if(kind==2){if(shoot())return;}
        else for(int d=0;d<4;++d){const int tile=neighboring(d);if(tile!=255&&tile>=64&&((tile^active)&128)){command=attack_command[d];return;}}
        command=4;
    };
    if(mode==2){attack_or_wait();return result(4,active);}
    if(mode==0){attack_or_wait();if(command!=4)return result(4,active);}
    if(kind==2&&shoot())return result(4,active);
    const int self=kind==3?side*33:offset,other=side?0:33;
    const int x=units[self+2].get<int>()&15,y=units[self+2].get<int>()>>4;
    // B4FD uses FF, not 0F, for the second army's retreat target.
    const int target_x=mode==1?(side?255:0):(units[other+2].get<int>()&15);
    const int target_y=mode==1?5:(units[other+2].get<int>()>>4);
    const int dx=std::abs(x-target_x),dy=std::abs(y-target_y);
    int preferred=0;
    auto vertical=[&](){if(y==target_y)return (random()&1)?0:1;return y<target_y?1:0;};
    auto horizontal=[&](){if(x==target_x)return (random()&1)?2:3;return x<target_x?3:2;};
    if(kind==0||kind==3){
        preferred=dy>dx?(y<target_y?1:0):(x<target_x?3:2);
        if(kind==3&&mode==1&&((!side&&x==0)||(side&&x==15)))return result(5,active);
    }else if(kind==1){
        bool along_y=false;
        if(dx<2){if(dy>dx)along_y=true;else if(dy==dx)along_y=(random()&1)!=0;}
        preferred=along_y?vertical():horizontal();
    }else preferred=dy>=3?vertical():horizontal();
    const int index=preferred*32+(random()&(kind==3?7:3));
    int direction=rom_.clash_direction(index);
    auto attempt=[&](){const int tile=neighboring(direction);
        if(empty(tile)){command=move_command[direction];return true;}
        if(enemy(tile)){command=attack_command[direction];return true;}
        return false;
    };
    if(attempt())return result(4,active);
    // B377 references the first tactical ledger position, not the nearest enemy.
    const int anchor=bytes_[(second_token&128)?0xdab:0xdc3];
    if(direction>=2){const int row=position&240,target_row=anchor&240;
        direction=row==target_row?((random()&1)?1:0):(row<target_row?1:0);
    }else{const int col=position&15,target_col=anchor&15;
        direction=col==target_col?((random()&1)?2:3):(col<target_col?2:3);
    }
    if(attempt())return result(4,active);
    direction=rom_.clash_direction(index+8);
    // B116 sends a free third candidate to B11B (fallback), whereas the
    // commander's B337 sends it to B33F (move). Keep that original asymmetry.
    if(kind==3){if(attempt())return result(4,active);}
    else if(enemy(neighboring(direction))){command=attack_command[direction];return result(4,active);}
    attack_or_wait();return result(4,active);
}

// A492-A4DE: movement start only. Destination occupancy is painted after animation.
std::string OriginalState::clash_move(Json &units,int active,int direction){
    if(!byte_array(units,66)||!unit_token(active)||direction<0||direction>3)return "无效交战移动";
    const int b=record(active),position=units[b+2];
    if(units[b]==255||units[b+1]==0||units[b+1]==255||position<16||position>=160)return "兵队不可移动";
    if((direction==2&&(position&15)==0)||(direction==3&&(position&15)==15))return "已到交战边界";
    const int delta[]={-16,16,-1,1},target=position+delta[direction];
    if(target<16||target>=160)return "已到交战边界";
    if(bytes_[0xe1a+target]&64||bytes_[0xe1a+target]>=24)return "目标格无法通行";
    bytes_[0xe1a+position]=bytes_[0xf1a+position];units[b+2]=target;return {};
}
// A4FA-A586: choose melee/projectile animation or the original general duel phase.
Json OriginalState::clash_attack(const Json &units,int active,int command,int previous_target){
    if(!byte_array(units,66)||!unit_token(active)||command<0||command>255||previous_target<0||previous_target>255)return {{"error","无效交战攻击"}};
    const int b=record(active),position=units[b+2],kind=units[b].get<int>()&3;
    if(units[b]==255||units[b+1]==0||units[b+1]==255||position<16||position>=160)return {{"error","兵队不可攻击"}};
    const int directions=command>>4;
    if(directions!=1&&directions!=2&&directions!=4&&directions!=8)return {{"error","请选择单一攻击方向"}};
    const int direction=directions==1?0:directions==2?1:directions==4?2:3;
    int target=previous_target;
    if(kind!=2){
        const int delta[]={-16,16,-1,1},cell=(position+delta[direction])&255;
        if(!(bytes_[0xe1a+cell]&64))return {{"error","相邻格没有兵队"}};
        target=bytes_[0xe1a+cell]&191;
        if(!unit_token(target)||units[record(target)]==255)return {{"error","目标兵队登记无效"}};
        if(kind==3&&(units[record(target)].get<int>()&3)==3)return {{"direction",direction},{"target",target},{"phase",13},{"counter",1}};
    }
    bytes_[0xe1a+position]=bytes_[0xf1a+position];
    return {{"direction",direction},{"target",target},{"phase",10},{"counter",kind==2?6:3}};
}

// A612/A77A after animation: repaint, optional extra action, then skip inactive units.
Json OriginalState::clash_end_action(const Json &units,int first,int second,int scene,int active,int extra,bool moved){
    if(!byte_array(units,66)||!unit_token(active)||first<0||first>=241||second<0||second>=241||scene<0||scene>15||extra<0||extra>3)return {{"error","无效交战行动结束状态"}};
    const int offset=record(active),position=units[offset+2],kind=units[offset].get<int>()&3;
    if(units[offset]==255||position<16||position>=160)return {{"error","当前兵队不存在"}};
    bytes_[0xe1a+position]=active|64;
    auto result=[&](int phase,int counter){return Json{{"active",active},{"extra",extra},{"phase",phase},{"counter",counter}};};
    if(!moved&&(units[0]==255||units[33]==255))return result(7,0);
    if(moved&&(kind&1)&&!(extra&1)){extra|=1;return result(4,1);}
    const int terrain=bytes_[0xf1a+position];bool bonus=false;
    if(scene==15)bonus=(active&128)&&(terrain==6||terrain==16||terrain==17);
    else{
        const int rank=bytes_[0x438+((active&128)?second:first)*8+6]&240;
        bonus=rank==16?terrain==5:rank!=0&&(terrain==8||terrain==9||terrain==14||terrain==15);
    }
    // The terrain bonus assigns 2; it does not OR bit 1 into the old value.
    if(bonus&&!(extra&2)){extra=2;return result(4,1);}
    extra=0;
    do{++active;}while((active&15)<11&&(units[record(active)+1]==0||units[record(active)+1]==255));
    return (active&15)>=11?result(2,0):result(4,1);
}

// A8E4-A9A5: one projectile initialization or flight frame, with byte wrapping.
Json OriginalState::clash_projectile(const Json &units,int active,int direction,const Json &flight,bool launch){
    if(!byte_array(units,66)||!unit_token(active)||direction<0||direction>3||!flight.is_object())return {{"error","无效投射物状态"}};
    for(const auto *key:{"x","y","distance","target"})if(!flight.contains(key)||!flight[key].is_number_integer()||flight[key]<Json(0)||flight[key]>Json(255))return {{"error","无效投射物数值"}};
    const int b=record(active),position=units[b+2];
    if(units[b]==255||position<16||position>=160)return {{"error","发射兵队不存在"}};
    int x=flight["x"],y=flight["y"],distance=flight["distance"],target=flight["target"];
    auto result=[&](int counter){return Json{{"x",x},{"y",y},{"distance",distance},{"target",target},{"counter",counter}};};
    if(launch){
        const int y_offset[]={4,12,8,8},x_offset[]={8,8,4,12};
        y=((position&240)+y_offset[direction])&255;x=((position<<4)+x_offset[direction])&255;
        bytes_[0xe1a+position]=bytes_[0xf1a+position];return result(7);
    }
    distance=(distance+2)&255;
    if(distance>=88){bytes_[0xe1a+position]=active|64;return result(8);}
    const int cell=(y&240)|(x>>4),tile=bytes_[0xe1a+cell];
    if(tile&192){
        if((tile^active)&128){target=tile&143;distance=16;return result(3);}
    }else if(tile>=24){bytes_[0xe1a+position]=active|64;return result(8);}
    const int dx[]={0,0,-2,2},dy[]={-2,2,0,0};
    x=(x+dx[direction])&255;y=(y+dy[direction])&255;return result(7);
}

// A72C-A779, after the frame renderer is ready: remove a zero-stamina unit.
std::string OriginalState::clash_resolve_hit(Json &units,int active,int target,int orientation,int tactical_side){
    if(!byte_array(units,66)||!unit_token(active)||!unit_token(target)||orientation<0||orientation>255||(tactical_side!=0&&tactical_side!=128))return "无效交战伤亡状态";
    const int b=record(target),position=units[b+2];
    if(units[b]==255||position<16||position>=160)return "目标兵队不存在";
    if(units[b+1]!=0){bytes_[0xe1a+position]=target|64;return {};}
    bytes_[0xe1a+position]=bytes_[0xf1a+position];
    units[b]=255;units[b+1]=255;units[b+2]=255;
    if((target&15)!=0){
        const int side=active^((orientation&128)?0:128)^(tactical_side?0:128);
        const int count=0xde4+((side&128)?2:0);
        const auto value=static_cast<std::uint16_t>(bytes_[count]+256*bytes_[count+1]+1);
        bytes_[count]=value&255;bytes_[count+1]=value>>8;
    }
    return {};
}

// A2C2-A31D: next unit in sequential mode, or army handover and AI dispatch.
Json OriginalState::clash_handover(const Json &units,const Json &runtime) const {
    if(!byte_array(units,66)||!runtime.is_object())return {{"error","无效交战交接状态"}};
    for(const auto *key:{"active","sequential","human_mask","orientation","tactical_side","ai_status","ai_mode","ai_side"})if(!runtime.contains(key)||!runtime[key].is_number_integer()||runtime[key]<Json(0)||runtime[key]>Json(255))return {{"error","无效交战交接数值"}};
    int active=runtime["active"],ai_status=runtime["ai_status"],ai_mode=runtime["ai_mode"],ai_side=runtime["ai_side"];
    if(!unit_token(active)&&active!=11&&active!=139)return {{"error","无效交接兵队编号"}};
    auto result=[&](int phase){return Json{{"active",active},{"phase",phase},{"counter",0},{"ai_status",ai_status},{"ai_mode",ai_mode},{"ai_side",ai_side}};};
    if(runtime["sequential"]!=0){
        while((++active&15)<11){
            const int hp=units[record(active)+1];
            if(hp!=0&&hp!=255)return result(ai_status==255?4:3);
        }
    }
    active=(active&128)?0:128;ai_side=active;
    int controller=active;
    if(runtime["orientation"].get<int>()&128)controller^=128;
    if(!(runtime["tactical_side"].get<int>()&128))controller^=128;
    const int mask=runtime["human_mask"],human=((controller&128)?mask:mask>>4)&3;
    if(human)return result(4);
    ai_status=128;ai_mode=32;return result(3);
}

// B920-BC3E: army-level computer orders, distinct from the per-unit B05F routine.
Json OriginalState::clash_strategy(const Json &units,const Json &orders,const Json &context,std::uint8_t &cursor) const {
    if(!byte_array(units,66)||!byte_array(orders,8)||!byte_fields(context,{"first","second","ai_side","orientation","tactical_side","scene","countdown","cycle"})||(context.contains("tail")&&!byte_array(context["tail"],3)))return {{"error","无效电脑军令状态"}};
    for(const auto &order:orders)if(order>Json(3))return {{"error","无效兵种军令"}};
    const int first=context["first"],second=context["second"],side=context["ai_side"].get<int>()&128;
    if(first>=241||second>=241||first==second||context["scene"]>Json(15)||(context["ai_side"]!=0&&context["ai_side"]!=128)||(context["tactical_side"]!=0&&context["tactical_side"]!=128))return {{"error","无效电脑军令对象"}};
    const int own=side?33:0,position=units[own+2];
    if(units[own]==255||position<16||position>=160)return {{"error","电脑主将不在交战场"}};
    auto next=orders;int countdown=context["countdown"],cycle=context["cycle"];
    if(countdown)--countdown;
    auto next_cursor=cursor;
    auto random=[&](){return development_["random_sequence"][next_cursor++].get<int>();};
    auto result=[&](){cursor=next_cursor;return Json{{"orders",next},{"countdown",countdown},{"cycle",cycle},{"ai_status",255}};};
    const int order_base=side?4:0;
    auto retreat=[&](){for(int i=0;i<4;++i)next[order_base+i]=1;return result();};
    auto enemy=[&](int cell){const int tile=bytes_[0xe1a+cell];return (tile&64)&&((tile^side)&128);};
    const int hp=units[own+1];bool threatened=false;
    if(hp<50){
        const int x=position&15;
        // A threat on the edgeward flank alone is sufficient; otherwise two
        // threats among the other three neighbors trigger the same branch.
        threatened=side?(x<15&&enemy(position+1)):(x>0&&enemy(position-1));
        if(!threatened){int count=0;
            count+=side?(x>0&&enemy(position-1)):(x<15&&enemy(position+1));
            if(position>=16)count+=enemy(position-16);
            if(position+16<160)count+=enemy(position+16);
            threatened=count>=2;
        }
    }
    bool choose_orders=false;
    if(threatened){
        if((side?second:first)<6){if(!countdown)return retreat();choose_orders=true;}
        else if(random()<204){next[order_base+3]=3;return result();}
    }
    if(!choose_orders&&!countdown&&hp>=60){
        int first_count=0,second_count=0;
        // BA0F counts first-army positions; BA26 counts second-army type bytes.
        for(int i=0;i<10;++i){first_count+=units[5+i*3]!=255;second_count+=units[36+i*3]!=255;}
        int threshold=rom_.clash_retreat_threshold(side?second_count:first_count,side?first_count:second_count);
        if(threshold==255)return retreat();
        if(threshold){
            const int a=bytes_[0x438+first*8+3],b=bytes_[0x438+second*8+3];
            if(std::abs(a-b)>=20){threshold+=((side?b:a)>=(side?a:b))?51:-51;
                // Both original overflow and underflow branches select retreat.
                if(threshold<0||threshold>255)return retreat();
            }
            if(random()<threshold)return retreat();
        }
    }
    cycle=(cycle+1)&255;
    if(!(cycle&1))return result();
    if(!side&&!context.contains("tail"))return {{"error","电脑军令需要尚未恢复的原版暂存字节"},{"boundary","ai_scratch"}};
    const int other=side?0:33,rank=bytes_[0x438+(side?first:second)*8+6]&240;
    auto raw=[&](int offset){return offset<66?units[offset].get<int>():context["tail"][offset-66].get<int>();};
    auto advantageous=[&](int cell){const int terrain=bytes_[0xf1a+cell];
        if(terrain==5)return rank==16;
        if(terrain==8||terrain==9||terrain==14||terrain==15)return rank==32;
        if(terrain==6||terrain==16||terrain==17)return ((side^context["orientation"].get<int>()^context["tactical_side"].get<int>())&128)!=0;
        return false;
    };
    auto ratio=[&](int kind){int count=0,good=0;
        // BC28 deliberately scans ordinary slots 1..11. Slot 11 reads the
        // neighboring general or three scratch bytes following the unit array.
        for(int slot=1;slot<=11;++slot)if((raw(other+slot*3)&3)==kind){++count;if(advantageous(raw(other+slot*3+2)))++good;}
        return count?(2*good/count):255; // Low byte of original division by zero.
    };
    const int archer_threshold=ratio(2)?128:179;
    next[order_base+2]=random()<archer_threshold?0:2;
    next[order_base]=ratio(0)&&(random()&8)?2:0;
    next[order_base+1]=ratio(1)&&(random()&128)?2:0;
    next[order_base+3]=random()<(context["scene"]==0?51:76)?0:2;
    return result();
}

// Resolve an unknown 04F2..04F4 read only when every possible value gives the
// same orders AND RNG cursor. BC28 uses just type & 3 and terrain[position];
// the middle byte is never read. One position per distinct terrain byte thus
// covers all 256^3 tails without assigning any scratch value to the session.
// Keep clash_strategy's legacy behavior for exact replay of existing saves.
Json OriginalState::resolve_clash_strategy(const Json &units,const Json &orders,const Json &context,std::uint8_t &cursor) const {
    auto next_cursor=cursor;
    auto result=clash_strategy(units,orders,context,next_cursor);
    if(result.value("boundary",std::string{})!="ai_scratch"){
        if(!result.contains("error"))cursor=next_cursor;
        return result;
    }
    std::array<bool,256> seen{};std::vector<int> positions;
    for(int position=0;position<256;++position){
        const int terrain=bytes_[0xf1a+position];
        if(!seen[terrain]){seen[terrain]=true;positions.push_back(position);}
    }
    Json consensus;std::uint8_t consensus_cursor=cursor;
    for(int kind=0;kind<4;++kind)for(const int position:positions){
        auto input=context;input["tail"]=Json::array({kind,0,position});next_cursor=cursor;
        auto candidate=clash_strategy(units,orders,input,next_cursor);
        if(candidate.contains("error"))return candidate;
        if(consensus.is_null()){consensus=std::move(candidate);consensus_cursor=next_cursor;}
        else if(candidate!=consensus||next_cursor!=consensus_cursor)
            return {{"error","电脑军令仍受未还原的原版暂存数据影响，已保留当前进度"},{"boundary","ai_scratch"}};
    }
    cursor=consensus_cursor;return consensus;
}

// AB5B-ABAC: pursuit injury when a general leaves the clash field.
Json OriginalState::clash_retreat_injury(Json &units,int active,std::uint8_t &cursor) const {
    if(!byte_array(units,66)||(active!=0&&active!=128))return {{"error","无效交战撤离状态"}};
    const int b=record(active),hp=units[b+1];
    if(units[b]==255||hp==255)return {{"error","撤离主将不存在"}};
    int damage=(development_["random_sequence"][cursor++].get<int>()&31)+5;
    if(!(development_["random_sequence"][cursor++].get<int>()&3))return {{"damage",damage},{"counter",4}};
    if(hp<=damage){damage=hp;units[b]=255;units[b+1]=255;units[b+2]=255;}
    else units[b+1]=hp-damage;
    return {{"damage",damage},{"counter",2}};
}

// AC49-AD13: desertion after surviving pursuit, before its optional message.
Json OriginalState::clash_retreat_desertion(Json &units,const Json &context,std::uint8_t &cursor){
    if(!byte_array(units,66)||!byte_fields(context,{"first","second","first_token","second_token","target","active"})||(context["active"]!=0&&context["active"]!=128))return {{"error","无效撤离散兵状态"}};
    const int first=context["first"],second=context["second"],active=context["active"],target=context["target"],first_token=context["first_token"],second_token=context["second_token"];
    if(units[0]==255||units[33]==255)return {{"error","撤离结算主将不存在"}};
    // Validate both ledgers and troop types on a private candidate. Failed
    // validation must not alter live SRAM or consume the caller's random cursor.
    auto candidate=*this;
    const auto error=candidate.settle_clash(units,first,second,first_token,second_token,target);
    if(!error.empty())return {{"error",error}};
    const int b=record(active),officer=active?second:first;
    auto next_cursor=cursor;int losses=0;
    if(development_["random_sequence"][next_cursor++].get<int>()<rom_.clash_desertion_threshold(officer)){
        int count=0;for(int i=1;i<11;++i)count+=units[b+i*3]!=255;
        losses=std::min(count,rom_.clash_desertion_size(development_["random_sequence"][next_cursor++].get<int>()&3));
    }
    if(losses){
        const int persistent=0x438+officer*8;
        for(int i=1,remaining=losses;i<11&&remaining;++i)if(units[b+i*3]!=255){
            // F7C2 reduces persistent archers, then cavalry, then infantry,
            // independently of the first live unit record removed by ACDD.
            if(bytes_[persistent+6]&15){
                if(bytes_[persistent+7]&240)bytes_[persistent+7]-=16;
                else if(bytes_[persistent+7]&15)--bytes_[persistent+7];
                else bytes_[persistent+4]-=16;
                const int total=(bytes_[persistent+4]>>4)+(bytes_[persistent+7]&15)+(bytes_[persistent+7]>>4);
                bytes_[persistent+6]=(bytes_[persistent+6]&240)|total;
            }
            for(int j=0;j<3;++j)units[b+i*3+j]=255;
            --remaining;
        }
    }else bytes_=std::move(candidate.bytes_);
    cursor=next_cursor;
    return {{"losses",losses},{"phase",losses?10:11},{"counter",losses?6:0},{"settled",losses==0}};
}

// ABB7/AC03/AC30/AC46/AD0B after the original dialog accepts confirmation.
Json OriginalState::clash_retreat_confirm(Json &units,const Json &context,std::uint8_t &cursor){
    if(!byte_array(units,66)||!byte_fields(context,{"first","second","first_token","second_token","target","active","orientation","tactical_side","counter"}))return {{"error","无效撤离确认状态"}};
    const int active=context["active"],counter=context["counter"],first=context["first"],second=context["second"],tactical=context["tactical_side"];
    if((active!=0&&active!=128)||(tactical!=0&&tactical!=128)||first>=241||second>=241||first==second||context["target"]>=Json(30)||counter<2||counter>7)return {{"error","无效撤离确认阶段"}};
    if(counter==2)return {{"phase",10},{"counter",units[record(active)+1]==255?3:4}};
    if(counter==3){
        if(units[record(active)]!=255)return {{"error","撤离主将尚未阵亡"}};
        const int winner=active^128^((context["orientation"].get<int>()&128)?0:128)^(tactical?0:128);
        ++bytes_[0xde8+((winner&128)?1:0)];
        return {{"phase",10},{"counter",7}};
    }
    if(counter==4)return clash_retreat_desertion(units,context,cursor);
    if(counter==7){
        auto result_context=context;result_context["active"]=active^128;
        return finish_clash_defeat(units,result_context);
    }
    const auto error=settle_clash(units,first,second,context["first_token"],context["second_token"],context["target"]);
    if(!error.empty())return {{"error",error}};
    return {{"phase",11},{"counter",0}};
}

// AF1C-AFAD: after surrender confirmation, settle twice around captive removal.
Json OriginalState::clash_surrender(Json &units,Json &captives,const Json &context,std::uint8_t &cursor){
    if(!byte_fields(context,{"first","second","first_token","second_token","target","active","orientation","tactical_side"})||!byte_array(units,66)||!byte_array(captives,24))return {{"error","无效投降状态"}};
    for(const auto &id:captives)if(id>=Json(241)&&id!=255)return {{"error","无效俘虏名册"}};
    const int active=context["active"],tactical=context["tactical_side"];
    if((active!=0&&active!=128)||(tactical!=0&&tactical!=128)||units[0]==255||units[33]==255)return {{"error","投降主将不存在"}};
    const int first=context["first"],second=context["second"],first_token=context["first_token"],second_token=context["second_token"],target=context["target"];
    const auto original=bytes_;
    auto error=settle_clash(units,first,second,first_token,second_token,target);
    if(!error.empty())return {{"error",error}};
    auto next_units=units,next_captives=captives;
    const int b=record(active),officer=(active&128)?second:first;
    next_units[b]=255;next_units[b+1]=255;next_units[b+2]=255;
    const int side=tactical^(context["orientation"].get<int>()&128)^(active&128),start=(side&128)?0:12;
    int slot=start;while(slot<start+11&&next_captives[slot]!=255)++slot;
    next_captives[slot]=officer;
    int loyalty=bytes_[0x438+officer*8+5];auto next_cursor=cursor;
    // Preserve the original literal LDA $E9D4 for the 50..59 range.
    if(loyalty>=80)loyalty=20+(development_["random_sequence"][next_cursor++].get<int>()&3);
    else if(loyalty>=60)loyalty=25+(development_["random_sequence"][next_cursor++].get<int>()&7);
    else if(loyalty>=50)loyalty=67;
    bytes_[0x438+officer*8+5]=loyalty;
    error=settle_clash(next_units,first,second,first_token,second_token,target);
    if(!error.empty()){bytes_=original;return {{"error",error}};}
    units=std::move(next_units);captives=std::move(next_captives);cursor=next_cursor;
    // Flag 32 here is an intermediate result; the captive ledger is needed later.
    return {{"officer",officer},{"captive_slot",slot},{"phase",11},{"counter",0}};
}

// AFD3-B002: result announcement's state changes, once the renderer is ready.
Json OriginalState::clash_general_defeat(int first,int second,int active,int orientation,int tactical_side){
    if(first<0||first>=241||second<0||second>=241||first==second||!unit_token(active)||orientation<0||orientation>255||(tactical_side!=0&&tactical_side!=128))return {{"error","无效主将战败状态"}};
    const int loser=(active&128)?first:second,b=0x438+loser*8;
    bytes_[b]=(bytes_[b]&31)|32;
    const int side=active^((orientation&128)?0:128)^(tactical_side?0:128);
    ++bytes_[0xde8+((side&128)?1:0)];
    return {{"officer",loser}};
}

// B009-B05E: confirmation after the general-defeat announcement.
Json OriginalState::finish_clash_defeat(const Json &units,const Json &context){
    if(!byte_fields(context,{"first","second","first_token","second_token","target","active"})||!unit_token(context["active"]))return {{"error","无效主将战败结算"}};
    const int first=context["first"],second=context["second"],loser=(context["active"].get<int>()&128)?first:second;
    if(!byte_array(units,66)||units[(context["active"].get<int>()&128)?0:33]!=255)return {{"error","战败主将仍在场"}};
    const auto error=settle_clash(units,first,second,context["first_token"],context["second_token"],context["target"]);
    if(!error.empty())return {{"error",error}};
    const bool human=loser<6&&((bytes_[0xd89]&7)==loser||((bytes_[0xd8a]&128)&&(bytes_[0xd8a]&7)==loser));
    Json result={{"officer",loser},{"phase",human?15:11},{"counter",0}};
    if(loser<6){const int attacker=bytes_[0xde3]&7;result["winner"]=attacker==loser?bytes_[0xde3]>>4:attacker;}
    return result;
}
}
