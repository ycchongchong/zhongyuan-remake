#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string&m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string&p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string&p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char**argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 for(const auto &row:json(base+"invasion-defeat-flow.json")){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));const auto &c=row["context"];Json result;
  if(row["step"]==0)result=state.clash_general_defeat(c["first"],c["second"],c["active"],c["orientation"],c["tactical_side"]);
  else result=state.finish_clash_defeat(row["units"],c);
  check(!result.contains("error")&&result["officer"]==row["officer"]&&state.sram()==read(base+row["after"].get<std::string>()),"Original invading clash defeat full SRAM "+row["variant"].dump()+"/"+row["step"].dump());
  if(row["step"]==1)check(result["phase"]==row["phase"],"Original human/computer failure dispatch");
 }
 auto flows=json(base+"invasion-settlement-flows.json");flows.push_back(json(base+"invasion-limit-flow.json"));
 for(const auto &flow:flows){
  const auto &steps=flow["steps"];OriginalState state(rom,read(base+steps[0]["before"].get<std::string>()));auto captives=steps[0]["captives_before"];
  for(const auto &step:steps){
   check(state.sram()==read(base+step["before"].get<std::string>())&&captives==step["captives_before"],"Continuous original result has no omitted inter-step SRAM/captive changes");
   std::uint8_t cursor=step["cursor_before"];const auto stage=step["stage"];
   if(stage=="close")state.close_battle(flow["target"]);
   else if(stage=="annex"){
    auto r=state.annex_ruler_city(step["city"],step["loser"],step["winner"],cursor);check(!r.contains("error")&&r["next_city"]==step["city_after"],"Original annexation city order");
   }else if(stage.is_string()){
    auto r=state.time_limit_step(step["counter_before"],flow["human_mask"],step["speaker_before"]);
    check(!r.contains("error")&&r["stage"]==step["stage_after"]&&r["speaker"]==step["speaker"]&&r["phase"]==step["phase"]&&r["reason"]==step["reason"],"Original invading time-limit notice transition");
   }else check(!state.withdrawal_result_step(flow["target"],flow["source"],stage,captives,cursor).contains("error"),"Original invading settlement step");
   check(state.sram()==read(base+step["after"].get<std::string>())&&captives==step["captives_after"]&&cursor==step["cursor_after"],"Original complete SRAM/captive/RNG result comparison");
  }
 }
 OriginalSession session(rom),loaded(rom);
 auto replay=[&](){const auto saved=session.save();const auto e=loaded.restore(saved);check(e.empty()&&loaded.save()==saved,"Invading ending replay: "+e);};
 auto call=[&](const std::string&e){check(e.empty(),"Invading ending action: "+e);};
 call(session.restore(json(project+"/tests/invasion-time-limit.json")));replay();
 const auto held=session.save();check(held["battle"]["tactics"]["round"]==11&&held["battle"]["tactics"]["moves"].size()==607,"Limit reached through607 actual tactical events");
 for(int i=0;i<3;++i){
  auto saved=session.save();check(!session.advance_withdrawal_result().empty()&&!session.advance_computer_attack().empty()&&!session.end_tactical_turn().empty()&&session.save()==saved,"Cannot bypass time announcement");
  call(session.advance_time_limit_result());replay();check(session.save()["sram"]==held["sram"]&&session.save()["random_cursor"]==held["random_cursor"],"Time notices preserve SRAM/RNG");
 }
 for(int i=0;i<40;++i){call(session.advance_withdrawal_result());replay();if(session.save()["battle"]["tactics"]["settlement"]["stage"]==17)break;}
 auto report=session.save();check(report["battle"]["tactics"]["settlement"]["stage"]==17,"Full-source forced retreat reaches final report");
 for(int i=0;i<16;++i)check(report["sram"][21*36+i]==held["sram"][21*36+i],"Limit retains defending ownership/resources");
 int dispersed=0;for(const auto&o:report["battle"]["tactics"]["settlement"]["outcomes"])dispersed+=!o["returned"].get<bool>();check(dispersed==4,"All four invaders disperse when source is full");
 call(session.finish_withdrawal_result());replay();check(session.save()["phase"]=="player_commands"&&session.save()["sram"][0xd8b]==4,"Limit returns to following human ruler");
 for(const std::string name:{"army","single","second","ruler"}){
  call(session.restore(json(project+"/tests/invasion-defeat-"+name+".json")));replay();const auto before=session.save();const int loser=name=="army"?44:name=="ruler"?3:4;
  check(before["battle"]["tactics"]["attack"]["result"]["officer"]==loser&&!session.finish_clash_result().empty()&&session.save()==before,"Actual hit leads to guarded final battle result");
  if(name=="army")call(session.begin_defender_defeat_result());
  else if(name=="ruler")call(session.begin_ruler_defeat_result());
  else{
   call(session.begin_human_failure());replay();const auto failure=session.save();check(failure["sram"]==before["sram"]&&failure["battle"]["tactics"]["human_failure"]["can_continue"]==(name=="second"),"Human defeat respects surviving second player");
   if(name=="single"){check(!session.finish_human_failure().empty()&&!session.begin_ruler_defeat_result().empty()&&session.save()==failure,"Last human defeat stays terminal without annexation");continue;}
   call(session.finish_human_failure());
  }
  replay();const int reason=name=="army"?3:name=="ruler"?148:131,end=name=="ruler"?4:29;
  check(session.save()["battle"]["tactics"]["turn_reason"]==reason,"Correct side-specific original result reason");
  for(int i=0;i<45;++i){call(session.advance_withdrawal_result());if(session.save()["battle"]["tactics"]["settlement"]["stage"]==end)break;}replay();
  if(name!="army"){
   auto settled=session.save();check(!session.finish_withdrawal_result().empty()&&session.save()==settled,"Cannot skip defeated ruler's territory");
   for(int city=0;city<30;++city){call(session.advance_ruler_annexation());check(session.save()["battle"]["tactics"]["annexation"]["next_city"]==city+1,"Annex each city exactly once");if(city%10==9)replay();}
   for(int city=0;city<30;++city)check((session.save()["sram"][city*36].get<int>()&7)!=loser,"No defeated ruler land remains");
   auto settled_all=session.save();check(!session.advance_ruler_annexation().empty()&&session.save()==settled_all,"Annexation cannot be repeated");
  }
  const auto final=session.save();auto forged=final;forged["battle"]["tactics"]["turn_reason"]=36;check(!loaded.restore(forged).empty()&&loaded.save()==final,"Forged result cannot bypass ruler settlement");
  call(session.finish_withdrawal_result());replay();const auto closed=session.save();
  check(closed["battle"].is_null()&&closed["sram"][0xd8b]==(name=="second"?5:4)&&closed["phase"]==(name=="second"?"ai_turn":"player_commands"),"Invasion result continues next surviving ruler");
  check((closed["sram"][21*36].get<int>()&7)==(name=="ruler"?4:3),"Original winner retains city");
  check(!session.finish_withdrawal_result().empty()&&session.save()==closed,"Strategic return cannot repeat");
 }
 std::cout<<"Original invasion endings:8 defeat boundaries,4 continuous result flows,607-event limit,army and both ruler outcomes,replay and strategic return passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
