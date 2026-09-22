#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <map>
#include <queue>
#include <chrono>
using namespace zhongyuan;
std::vector<std::uint8_t> read(const std::string&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
// Deterministic public-API campaign probe, not an AI built into the game.
// It never assigns SRAM, ownership, resources, RNG or an ending. A failure or
// time limit is retained as a failed probe; only a settled 30-city win passes.
int main(int argc,char **argv){
 if(argc!=4){std::cerr<<"Usage: natural_campaign_probe ROM ruler(0..5) NEW_OUTPUT_DIRECTORY\n";return 64;}
 const auto output=std::filesystem::u8path(argv[3]);
 if(std::filesystem::exists(output)&&!std::filesystem::is_empty(output)){std::cerr<<"Output directory must be new or empty\n";return 64;}
 const int selected_ruler=std::stoi(argv[2]);if(selected_ruler<0||selected_ruler>5)return 64;
 std::filesystem::create_directories(output);
 OriginalRom rom(read(argv[1]));OriginalSession game(rom);game.start(selected_ruler,0);
 std::string action;int battles=0;std::map<std::size_t,int> repeated;Json trace=Json::array();std::map<int,int> visits;int last_round=-1,last_side=-1;
const auto started=std::chrono::steady_clock::now();try{for(int step=0;step<200000;++step){if(std::chrono::steady_clock::now()-started>std::chrono::seconds(600))throw std::runtime_error("600 second diagnostic bound; partial state retained");game.tick(6);auto saved=game.save();if(step%1000==0){OriginalSession loaded(rom);auto e=loaded.restore(saved);if(!e.empty()||loaded.save()!=saved)throw std::runtime_error("strict reload step="+std::to_string(step)+": "+e);}const auto raw=saved["sram"].get<std::vector<std::uint8_t>>();std::string phase=saved["phase"],error;auto snap=phase=="player_commands"?game.snapshot():Json::object();
 auto record=[&](){trace.push_back({{"step",step},{"action",action},{"phase",phase}});if(step%100==0){if(snap.empty())snap=game.snapshot();std::cout<<step<<" "<<phase<<" "<<action<<" "<<snap["year"]<<"/"<<snap["month"]<<" battles="<<battles<<std::endl;}};
 if(phase=="ending"){std::ofstream((output/"actions.json"))<<trace.dump()<<'\n';std::cout<<"ENDING "<<saved["ending"]<<std::endl;std::ofstream((output/"ending.json"))<<saved.dump()<<'\n';bool won=saved["ending"]["kind"]=="unification"&&saved["ending"]["winner"]==selected_ruler;
for(int city=0;city<30;++city)won=won&&((raw[city*36]&7)==selected_ruler);
OriginalSession restored(rom);won=won&&restored.restore(saved).empty()&&restored.save()==saved&&saved["battle"].is_null();
auto ending_snapshot=game.snapshot();
std::ofstream(output/"result.json")<<Json({{"passed",won},{"operations",step},{"battles",battles},{"ruler",selected_ruler},{"difficulty",0},{"frames_per_operation",6},{"year",ending_snapshot["year"]},{"month",ending_snapshot["month"]},{"ending",saved["ending"]},{"unification",ending_snapshot.value("unification",Json())}}).dump(2)<<'\n';
return won?0:2;}
 if(phase=="ai_turn"){action="advance";auto r=game.advance();error=r.value("error",std::string{});}
 else if(phase=="player_commands"){
  int ruler=raw[0xd8b];
  bool returned=false;
  for(int city=0;city<30;++city)if((raw[city*36]&7)==ruler){auto report=game.visit_city(city);if(!report.empty()&&!report.contains("error")){auto result=game.finish_search(true);error=result.value("error",std::string{});action="search_return";returned=true;break;}}
  if(returned){}else if(!saved["pending_army"].is_null()){
   action="assign_army";int city=saved["pending_army"];
   std::vector<int> army;for(auto id:snap["cities"][city]["officer_slots"])if(!id.is_null())army.push_back(id);
   std::sort(army.begin(),army.end(),[&](int a,int b){return raw[0x438+a*8+3]+raw[0x438+a*8+1]+2*raw[0x438+a*8+2]>raw[0x438+b*8+3]+raw[0x438+b*8+1]+2*raw[0x438+b*8+2];});for(int id:army)game.assign_troops(id,10);
   game.finish_recruitment();
  }else{
   bool done=false;int rally=-1,best_army=-1;
   // Move the ruler toward the friendly city farthest from hostile territory.
   std::vector<int> distances(30,100),path(30,-1);std::queue<int> fronts;
   for(int i=0;i<30;++i)if((raw[i*36]&7)!=ruler){distances[i]=0;fronts.push(i);}
   while(!fronts.empty()){int here=fronts.front();fronts.pop();for(auto nb:snap["cities"][here]["neighbors"]){int n=nb;if(distances[n]>distances[here]+1){distances[n]=distances[here]+1;fronts.push(n);}}}
   int capital=-1;for(int i=0;i<30;++i)for(auto id:snap["cities"][i]["officer_slots"])if(id==ruler)capital=i;
   if(capital>=0)for(auto nb:snap["cities"][capital]["neighbors"]){int dest=nb;if((raw[dest*36]&7)==ruler&&distances[dest]>distances[capital]&&game.move(capital,dest,{ruler}).empty()){action="protect_ruler";done=true;break;}}

   for(int city=0;city<30;++city)if((raw[city*36]&7)==ruler){bool frontier=false;for(auto n:snap["cities"][city]["neighbors"])frontier|=(raw[n.get<int>()*36]&7)!=ruler;if(!frontier)continue;int force=0;for(auto id:snap["cities"][city]["officer_slots"])if(!id.is_null())force+=10+(raw[0x438+id.get<int>()*8+6]&15);if(force>best_army){best_army=force;rally=city;}}
   if(!done&&capital>=0){auto c=snap["cities"][capital];if(raw[0x438+ruler*8+3]<99&&c["inventory"][1]>0){auto result=game.execute_command(capital,"award_weapons",{{"officer",ruler},{"quantity",std::min(12,c["inventory"][1].get<int>())}});if(!result.contains("error")){action="train_ruler";done=true;}}
    if(!done&&(raw[0x438+ruler*8+6]&15)<10&&c["gold"]>200&&game.recruit(capital,10).empty()){action="guard_ruler";done=true;}}
   for(int city=0;city<30&&!done&&rally>=0;++city)if(city!=rally&&(raw[city*36]&7)==ruler){
    std::queue<int> q;std::map<int,int> first;q.push(city);first[city]=-1;
    while(!q.empty()&&!first.count(rally)){int current=q.front();q.pop();for(auto node:snap["cities"][current]["neighbors"]){int next=node;if((raw[next*36]&7)==ruler&&!first.count(next)){first[next]=current==city?next:first[current];q.push(next);}}}
    if(!first.count(rally))continue;int destination=first[rally];auto c=snap["cities"][city];std::vector<int> ids;for(auto id:c["officer_slots"])if(!id.is_null())ids.push_back(id);
    std::sort(ids.begin(),ids.end(),[&](int a,int b){return raw[0x438+a*8+3]+raw[0x438+a*8+1]+2*raw[0x438+a*8+2]>raw[0x438+b*8+3]+raw[0x438+b*8+1]+2*raw[0x438+b*8+2];});
    if(ids.size()>1){ids.pop_back();ids.erase(std::remove_if(ids.begin(),ids.end(),[](int id){return id<6;}),ids.end());auto e=game.move(city,destination,ids);if(e.empty()){action="concentrate "+std::to_string(city)+"->"+std::to_string(destination);done=true;continue;}}
    if(c["gold"]>500&&snap["cities"][rally]["gold"]<5000){auto result=game.execute_command(city,"transport",{{"target",destination},{"items",Json::array({c["gold"].get<int>()-100,c["inventory"][1].get<int>(),0,0,0})}});if(!result.contains("error")){action="supplies "+std::to_string(city);done=true;}}
   }
   for(int city=0;city<30&&!done;++city)if((raw[city*36]&7)==ruler){
    auto c=snap["cities"][city];std::vector<int> ids;for(auto id:c["officer_slots"])if(!id.is_null())ids.push_back(id);
    if(ids.size()<2)continue;
    auto power=[&](int id){return int(raw[0x438+id*8+3])+int(raw[0x438+id*8+1])+2*int(raw[0x438+id*8+2]);};
    std::sort(ids.begin(),ids.end(),[&](int a,int b){return power(a)>power(b);});ids.pop_back();ids.erase(std::remove_if(ids.begin(),ids.end(),[](int id){return id<6;}),ids.end());if(ids.empty())continue;if(ids.size()>10)ids.resize(10);
    for(auto neighbor:c["neighbors"]){int target=neighbor;if((raw[target*36]&7)==ruler)continue;
     for(int id:ids)if(raw[0x438+id*8+1]+20<rom.officer(id)["stamina"].get<int>()){auto heal=game.execute_command(city,"heal",{{"officer",id}});if(!heal.contains("error")){action="heal "+std::to_string(id);done=true;break;}}
     if(done)break;
     for(int id:ids)if(id>=6&&raw[0x438+id*8+5]<90){auto award=game.execute_command(city,"award_gold",{{"officer",id},{"quantity",100}});if(!award.contains("error")){action="loyalty "+std::to_string(id);done=true;break;}}
     if(done)break;
     for(int id:ids)if(raw[0x438+id*8+3]<99&&c["inventory"][1]>0){auto award=game.execute_command(city,"award_weapons",{{"officer",id},{"quantity",std::min(12,c["inventory"][1].get<int>())}});if(!award.contains("error")){action="weapons "+std::to_string(id);done=true;break;}}
     if(done)break;
     if(c["inventory"][1]==0&&c["gold"]>500){auto bought=game.execute_command(city,"buy",{{"quantity",100}});if(!bought.contains("error")){action="buy_weapons";done=true;break;}}
     int needed=0;for(int id:ids)needed+=10-(raw[0x438+id*8+6]&15);
     int reserves=c["reserves"].get<int>()/100,gold=c["gold"],quantity=std::min(std::max(0,needed-reserves),std::max(0,(gold-int(ids.size())*200)/20));
     if(needed>0&&(reserves>=needed||quantity>0)&&game.recruit(city,quantity).empty()){action="recruit "+std::to_string(city);done=true;break;}
     auto quote=game.expedition_quote(city,target,ids,ids[0]);if(quote.contains("error"))continue;
     int own=0,enemy=0;for(int id:ids)own+=(raw[0x438+id*8+6]&15)*raw[0x438+id*8+3]*raw[0x438+id*8+1];
     for(auto id:snap["cities"][target]["officer_slots"])if(!id.is_null())enemy+=(raw[0x438+id.get<int>()*8+6]&15)*raw[0x438+id.get<int>()*8+3]*raw[0x438+id.get<int>()*8+1];
     if(own<enemy)continue;
     auto result=game.dispatch_expedition(city,target,ids,ids[0]);error=result.value("error",std::string{});action="expedition "+std::to_string(city)+"->"+std::to_string(target);done=true;break;
    }
   }
   if(!done){
    for(int city=0;city<30&&!done;++city)if((raw[city*36]&7)==ruler){
     auto c=snap["cities"][city];std::vector<int> ids;for(auto id:c["officer_slots"])if(!id.is_null())ids.push_back(id);
     if(ids.size()<2)continue;bool frontier=false;for(auto id:c["neighbors"])frontier|=(raw[id.get<int>()*36]&7)!=ruler;if(frontier)continue;
     std::queue<int> pending;std::map<int,int> first;pending.push(city);first[city]=-1;int destination=-1;
     while(!pending.empty()&&destination<0){int current=pending.front();pending.pop();for(auto node:snap["cities"][current]["neighbors"]){int next=node;if((raw[next*36]&7)!=ruler){destination=first[current];break;}if(!first.count(next)){first[next]=current==city?next:first[current];pending.push(next);}}}
     if(destination<0)continue;ids.pop_back();ids.erase(std::remove_if(ids.begin(),ids.end(),[](int id){return id<6;}),ids.end());auto e=game.move(city,destination,ids);if(e.empty()){action="rally "+std::to_string(city)+"->"+std::to_string(destination);done=true;}
    }
    if(!done)for(int city=0;city<30&&!done;++city)if((raw[city*36]&7)==ruler){
      auto c=snap["cities"][city];
      if(c["control"]<60&&c["gold"]>500){auto result=game.execute_command(city,"award_people",{{"quantity",200}});if(!result.contains("error")){action="control";done=true;break;}}
      std::vector<int> ids;for(auto id:c["officer_slots"])if(!id.is_null())ids.push_back(id);
      if(ids.size()<2)continue;
      std::sort(ids.begin(),ids.end(),[&](int a,int b){return raw[0x438+a*8+2]<raw[0x438+b*8+2];});
      for(int id:ids)if(id>=6&&game.search(city,id).empty()){action="search";done=true;break;}
    }
    if(!done){action="end_turn";error=game.end_turn();}
   }
  }
 }else if(phase=="battle"||phase=="expedition"){
  const auto b=saved["battle"];
  if(!b.contains("deployment")){++battles;repeated.clear();visits.clear();last_round=-1;action="prepare";error=game.prepare_deployment();}
  else if(b["deployment"].value("handover",false)){action="handover";error=game.prepare_deployment();}
  else if(b["deployment"]["current"]!=-1){action="deploy";error=game.confirm_deployment();}
  else if(!b.contains("tactics")){action="tactics";error=game.begin_tactics();}
  else error=game.perform_battle_action([&](auto &s)->std::string{
   auto t=b["tactics"];if(t.value("turn_boundary",std::string{})=="computer"){auto stable=t;for(auto key:{"moves","history","deployment_sram"})stable.erase(key);auto signature=std::hash<std::string>{}(stable.dump()+saved["sram"].dump()+saved["random_cursor"].dump());if(++repeated[signature]>16)return "Repeated computer decision state; diagnostic stopped";}if(t.contains("computer")&&t["computer"].value("role_slot",-1)>=0&&!t.contains("computer_plan")&&t["computer"]["kind"]=="plan"){std::ofstream((output/"role.json"))<<saved.dump();}
   if(t.contains("human_strategy")){action="human_strategy";return t["human_strategy"]["stage"]=="confirm"?s.confirm_tactical_strategy():s.finish_tactical_strategy();}
   if(t.contains("human_failure")){action="human_failure";return t["human_failure"].value("can_continue",false)?s.finish_human_failure():"Player defeated";}
   if(t.contains("attack")){
    auto a=t["attack"];std::string stage=a["stage"];action=stage;
    if(stage=="confirm")return s.confirm_tactical_attack();
    if(stage=="clash_ready")return s.begin_clash();
    if(stage=="clash_orders"||(stage=="clash_running"&&a["clash"]["runtime"]["phase"]==4&&a["clash"]["runtime"]["counter"]==1))for(int army=0;army<2;++army)if(a["clash"]["players"][army]!=0){int id=a["clash"][army?"second":"first"];int health=a["clash"]["runtime"]["units"][army?34:1];int enemy_health=a["clash"]["runtime"]["units"][army?1:34];int desired=(health>enemy_health+20&&raw[0x438+id*8+3]>=95)?0:2;if(health<(id<6?70:50))desired=1;if(a["clash"]["runtime"]["orders"][army*4+3]!=desired)return s.cycle_clash_order(army,3);}
    if(stage=="clash_orders"||stage=="clash_running"){auto before=s.save();std::string cue;auto e=s.advance_clash(&cue);return e;}
    if(stage=="clash_boundary"){
     if(a["boundary"]=="ai_scratch"){auto e=s.resume_clash_strategy();return e.empty()?e:s.recover_clash_strategy();}
     if(a["boundary"]=="retreat")return s.advance_clash_retreat();
     if(a["boundary"]=="general_defeat")return s.advance_clash_defeat();
     if(a["boundary"]=="duel")return s.begin_duel();
    }
    if(stage=="clash_retreat")return s.advance_clash_retreat();if(stage=="clash_defeat")return s.advance_clash_defeat();
    if(stage=="clash_result"){
     auto state=s.snapshot()["battle"];
     if(state.value("human_failure_available",false))return s.begin_human_failure();
     if(state.value("ruler_defeat_result_available",false))return s.begin_ruler_defeat_result();
     if(state.value("commander_defeat_result_available",false))return s.begin_commander_defeat_result();
     if(state.value("army_defeat_result_available",false))return s.begin_defender_defeat_result();return s.finish_clash_result();
    }
    if(stage=="surrender_confirm")return s.answer_clash_surrender(false);
    if(stage=="surrender_notice"||stage=="surrender_accepted")return s.advance_clash_surrender();
    if(stage=="duel_orders"){int side=a["duel"]["side"].get<int>()?1:0;return a["clash"]["players"][side]!=0?s.choose_duel_command(1):s.advance_duel();}
    if(stage=="duel_confirm")return s.answer_duel_surrender(false);
    if(stage.rfind("duel_",0)==0)return s.advance_duel();return "Unhandled clash "+stage;
   }
   if(t.contains("retreat")){action="finish_retreat";return s.finish_tactical_retreat();}
   if(t.value("turn_boundary",std::string{})=="battle_result"){
    int stage=t.value("settlement",Json::object()).value("stage",-1);action="settlement";
    if((t.value("turn_reason",0)&128)&&t.value("annexation",Json::object()).value("next_city",0)<30&&(stage==4||stage==29))return s.advance_ruler_annexation();return stage==4||stage==13||stage==17||stage==29?s.finish_withdrawal_result():s.advance_withdrawal_result();
   }
   if(t.value("turn_boundary",std::string{})=="time_limit"){action="time_limit";return s.advance_time_limit_result();}
   if(t.value("turn_boundary",std::string{})=="computer"){
    action="computer";
    if(phase=="battle")return t.value("attacker_ai",Json::object()).value("stage",std::string{})=="held_retreat"?s.resume_computer_attacker_retreat():s.advance_computer_attack();
    if(t.value("computer",Json::object()).value("kind",std::string{})!="plan")return s.advance_computer_tactics();
    auto motion=t.value("computer_motion",Json::object());
    if(t["computer"].value("role_slot",-1)>=0){auto e=s.continue_computer_role();if(e.empty())return e;e=s.continue_empty_computer_role();return e.empty()?e:s.resume_computer_role_after_handover();}
    if(t.contains("strategy_result"))return s.finish_computer_strategy();
    if(motion.value("kind",std::string{})=="retry_strategy")return s.retry_computer_strategy();
    if(motion.value("kind",std::string{})=="scan")return s.continue_computer_scan();
    if(motion.value("branch",std::string{})=="role"){auto e=s.continue_computer_nearby();return e.empty()?e:s.finish_exhausted_computer_attack();}
    if(t.contains("computer_motion"))return s.continue_computer_flank();
    if(t.value("computer_strategy",Json::object()).value("kind",std::string{})=="role_reassignment")return s.continue_computer_occupied_fort(true);
    if(t.contains("computer_strategy")){auto e=t["computer_strategy"]["kind"]=="no_strategy"?s.continue_computer_motion():s.execute_computer_strategy();return e.empty()?e:s.finish_exhausted_computer_attack();}
    return t.contains("computer_plan")?s.evaluate_computer_strategy():s.plan_computer_tactics(t.contains("computer_reuse"));
   }
   int side=t.value("side",128),round=t.value("round",0);if(side!=last_side||round!=last_round){visits.clear();last_side=side;last_round=round;}
   int base=side?0xdc2:0xdaa,stride=side?3:2,count=side?11:12,opposite=side?0xdaa:0xdc2,enemy_stride=side?2:3;
   for(int slot=0;slot<count;++slot){int caster=raw[base+slot*stride];if(caster>=241||raw[0x438+caster*8+2]<80)continue;
    auto choices=s.player_tactical_strategies(slot);if(choices.contains("error"))continue;
    for(int desired:{7,3,2,0})for(auto choice:choices["choices"])if(choice["strategy"]==desired&&choice["cost"]<=t["points"]){
     for(int target=0;target<(side?12:11);++target){int enemy=raw[opposite+target*enemy_stride];if(enemy>=241)continue;
      int from=raw[base+slot*stride+1],to=raw[opposite+target*enemy_stride+1];
      if(std::max(abs((from&15)-(to&15)),abs((from>>4)-(to>>4)))>2)continue;
      if(desired==7&&(enemy<6||raw[0x438+enemy*8+5]>=90))continue;
      if(desired!=7&&(raw[0x438+enemy*8+6]&15)<2)continue;
      auto e=s.prepare_tactical_strategy(slot,target,desired);if(e.empty()){action="cast "+std::to_string(desired);return e;}
     }
    }
   }
   OriginalState board(rom,raw);int best=-100000,best_slot=-1,best_direction=-1;bool attack=false;
   for(int slot=0;slot<count;++slot){int officer=raw[base+slot*stride];if(officer>=241)continue;
    int strength=(raw[0x438+officer*8+6]&15)*10+raw[0x438+officer*8+1];
    for(int dir=0;dir<4;++dir){int points=t["points"];auto trial=board;auto a=trial.tactical_attack(slot,dir,points,side);
     if(!a.contains("error")){if(!attack||strength>best){best=strength;best_slot=slot;best_direction=dir;attack=true;}continue;}
     if(attack)continue;points=t["points"];trial=board;if(!trial.tactical_step(slot,dir,points,side).empty())continue;
     int dest=trial.sram()[base+slot*stride+1],distance=1000;
     for(int enemy_slot=0;enemy_slot<(side?12:11);++enemy_slot){if(raw[opposite+enemy_slot*enemy_stride]>=241)continue;int pos=raw[opposite+enemy_slot*enemy_stride+1];distance=std::min(distance,abs((pos&15)-(dest&15))+abs((pos>>4)-(dest>>4)));}
     int score=strength-distance*30-visits[slot*256+dest]*100;
     if(score>best){best=score;best_slot=slot;best_direction=dir;}
    }
   }
   if(best_slot<0){action="end_tactics";return s.end_tactical_turn();}
   if(attack){action="human_attack";return s.attack_tactical(best_slot,best_direction);}
   action="human_move";auto e=s.move_tactical(best_slot,best_direction);if(e.empty()){auto result=s.save();++visits[best_slot*256+result["sram"][base+best_slot*stride+1].template get<int>()];}return e;
  });
 }else error="Unknown phase "+phase;
 record();if(!error.empty())throw std::runtime_error("step="+std::to_string(step)+" "+action+": "+error);
 }throw std::runtime_error("200000 operation bound reached");
}catch(const std::exception&e){std::ofstream((output/"held.json"))<<game.save().dump()<<'\n';std::ofstream((output/"actions.json"))<<trace.dump()<<'\n';std::ofstream(output/"result.json")<<Json({{"passed",false},{"error",e.what()},{"operations",trace.size()},{"battles",battles}}).dump(2)<<'\n';std::cerr<<e.what()<<std::endl;return 1;}}
