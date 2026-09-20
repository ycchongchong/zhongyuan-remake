#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string&p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char**argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"invading-retreat.json");check(rows.size()==32,"32 original retreat flows");
 for(const auto&r:rows){
  const auto &steps=r["steps"];const std::string label=r["id"].dump();check(steps.size()==6,"Single-unit computer dispatch, including commander "+label);
  OriginalState state(rom,read(base+steps[0]["sram"].get<std::string>()));std::uint8_t cursor=steps[0]["cursor"];
  const auto decision=state.tactical_ai_assessment(128,4,r["points"],cursor);
  check(decision["kind"]=="retreat"&&decision["slot"]==r["slot"]&&cursor==steps[1]["cursor"]&&state.sram()==read(base+steps[1]["sram"].get<std::string>()),"Original retreat assessment SRAM/RNG "+label);
  for(int index:{2,3})check(state.sram()==read(base+steps[index]["sram"].get<std::string>()),"No omitted SRAM changes before retreat "+label);
  check(steps[2]["points"]==r["points"]&&steps[3]["points"].get<int>()==r["points"].get<int>()-1&&steps[2]["token"]==(48|r["slot"].get<int>()),"Computer token omits commander marker and charges one point "+label);
  cursor=steps[3]["cursor"];const auto result=state.retreat_attacker(r["slot"]);
  check(!result.contains("error")&&result["returned"]!=r["full"],"Original capacity-dependent return "+label);
  check(state.sram()==read(base+steps[4]["sram"].get<std::string>())&&cursor==steps[4]["cursor"],"Original retreat complete SRAM/RNG "+label);
  check(state.sram()==read(base+steps[5]["sram"].get<std::string>())&&steps[4]["points"]==steps[3]["points"],"Original report does not repeat the charge "+label);
  for(const auto &step:steps)check(step["scan_slot"]==7,"Original retreat preserves scan cursor "+label);
 }
 for(const auto&r:json(base+"invasion-return.json")){
  const auto &s=r["steps"];OriginalState state(rom,read(base+s[0]["sram"].get<std::string>()));std::uint8_t cursor=s[0]["cursor"];
  state.close_battle(r["target"]);check(state.sram()==read(base+s[1]["sram"].get<std::string>()),"Original D8F4 full SRAM cleanup");
  check(!state.next_ruler(cursor),"Original invasion return remains in current month");state.begin_ruler_turn(state.sram()[0xd8b]);
  check(state.sram()==read(base+s[2]["sram"].get<std::string>()),"Original A086 next-ruler/full SRAM command books");
 }
 OriginalSession session(rom),loaded(rom);
 auto replay=[&](){auto saved=session.save();auto error=loaded.restore(saved);check(error.empty()&&loaded.save()==saved,"Invasion result strict replay: "+error);};
 auto call=[&](const std::string &e){check(e.empty(),"Invasion action: "+e);replay();};
 auto settle=[&](int reason){
  auto saved=session.save();check(saved["battle"]["tactics"]["turn_reason"]==reason,"Expected invasion battle result");
  check(!session.finish_withdrawal_result().empty()&&session.save()==saved,"No strategic return before settlement");
  int final_stage=reason==2?13:reason==20?4:17;
  for(int i=0;i<50;++i){call(session.advance_withdrawal_result());if(session.save()["battle"]["tactics"]["settlement"]["stage"]==final_stage)break;}
  saved=session.save();check(saved["battle"]["tactics"]["settlement"]["stage"]==final_stage,"Completed original settlement");
  check(!session.advance_withdrawal_result().empty()&&session.save()==saved,"No repeated officer/resource transfer");
  auto forged=saved;forged["sram"][21*36+1]=0;
  check(!loaded.restore(forged).empty()&&loaded.save()==saved,"Forged settled state rejected atomically");
  const auto expected_owner=reason==2?3:4;check((saved["sram"][21*36].get<int>()&7)==expected_owner,"Correct city owner after invasion");
  call(session.finish_withdrawal_result());saved=session.save();
  check(saved["battle"].is_null()&&saved["phase"]=="player_commands"&&saved["sram"][0xd8b]==4,"Return to following human ruler, without repeating computer turn");
  check(saved["sram"][0xd9f]==(reason==2?2:3),"Next human ruler receives original command books");
  check(saved["events"].back()["kind"]=="ai"&&saved["events"].back()["ruler"]==3&&saved["events"].back()["battle_result"]==(reason==2?2:1),"Strategic event records settled invasion outcome");
  for(int i=0;i<59;++i)check(saved["sram"][0xdaa+i]==255,"Battle ledgers cleared");
  check(saved["sram"][21*36+32]==255&&saved["sram"][21*36+33]==255,"Siege markers cleared");
  check(!session.finish_withdrawal_result().empty()&&session.save()==saved,"Cannot acknowledge strategic return twice");
 };
 call(session.restore(json(project+"/tests/attacker-invasion-far.json")));call(session.begin_tactics());
 for(int i=0;i<100&&session.save()["battle"]["tactics"].contains("turn_boundary");++i)call(session.advance_computer_attack());
 check(session.save()["battle"]["tactics"]["side"]==0,"Human defense reached through real computer moves");
 auto initial=session.save();check(!session.resume_computer_attacker_retreat().empty()&&session.save()==initial,"Cannot inject a computer retreat in human turn");
 for(int slot=0;slot<12;++slot)if(session.save()["sram"][0xdaa+2*slot]!=255){
  auto before=session.save();call(session.begin_tactical_retreat(slot));call(session.cancel_tactical_retreat());
  check(session.save()["sram"]==before["sram"]&&session.save()["random_cursor"]==before["random_cursor"]&&session.save()["battle"]["tactics"]["points"]==before["battle"]["tactics"]["points"],"Cancel defender withdrawal preserves SRAM/RNG/mobility");
  call(session.begin_tactical_retreat(slot));call(session.confirm_tactical_retreat());
  auto result=session.save();check(!session.confirm_tactical_retreat().empty()&&!session.end_tactical_turn().empty()&&session.save()==result,"Pending withdrawal cannot be repeated or skipped");
  call(session.finish_tactical_retreat());
 }
 settle(2);
 for(const auto &name:{"open","full","commander"}){
  call(session.restore(json(project+"/tests/invasion-retreat-"+name+".json")));
  bool finished=false;int retreats=0;
  for(int i=0;i<80;++i){
   auto before=session.save();auto &t=before["battle"]["tactics"];
   if(t.value("turn_boundary",std::string{})=="battle_result"){settle(std::string(name)=="commander"?20:36);finished=true;break;}
   if(t["attacker_ai"]["stage"]=="held_retreat"){
    const int slot=t["attacker_ai"]["assessment"]["slot"],points=t["points"],scan=t["computer_cursor"];
    const bool commander=before["sram"][0xdc4+3*slot].get<int>()&128;
    check(!session.advance_computer_attack().empty()&&session.save()==before,"Legacy held event retains its original meaning");
    call(session.resume_computer_attacker_retreat());++retreats;const auto result=session.save();const auto &r=result["battle"]["tactics"]["retreat"];
    check(result["battle"]["tactics"]["points"]==points-1&&result["battle"]["tactics"]["computer_cursor"]==scan&&r["outcomes"].size()==1&&!r["commander"].get<bool>(),"Computer retreat charges once and preserves scan, even for commander");
    check(!session.resume_computer_attacker_retreat().empty()&&!session.advance_computer_attack().empty()&&session.save()==result,"Cannot repeat or bypass computer retreat report");
    auto forged=result;forged["battle"]["tactics"]["moves"].back()["cursor"]=256;
    check(!loaded.restore(forged).empty()&&loaded.save()==result,"Malformed computer retreat event rejected atomically");
    call(session.finish_tactical_retreat());
    if(std::string(name)=="full"){
     check(result["sram"]==before["sram"]&&r["outcomes"][0]["returned"]==false&&session.save()["battle"]["tactics"]["turn_boundary"]=="computer","Full source city costs a point but preserves army and resumes computer");
     call(session.advance_computer_attack());finished=true;break;
    }
    if(commander&&std::string(name)=="commander"){
     check(session.save()["battle"]["tactics"]["turn_reason"]==20,"Commander leaves alone, then triggers commander-loss settlement");
     int survivors=0;for(int j=0;j<11;++j)survivors+=session.save()["sram"][0xdc2+3*j]!=255;
     check(survivors==3,"Other invaders remain until settlement returns them");
    }
   }else call(session.advance_computer_attack());
  }
  check(finished&&retreats==(std::string(name)=="open"?4:1),"Complete real invading retreat progression");
 }
 // A last-ruler invasion crosses the month boundary; rotation appends its
 // own event after the invasion outcome. Keep the save's64-event bound.
 {
  auto held=json(project+"/tests/invasion-retreat-commander.json"),entry=held;
  entry["sram"]=held["battle"]["tactics"]["deployment_sram"];entry["battle"].erase("tactics");entry["random_cursor"]=entry["ai"]["random_cursor"];
  entry["sram"][0xd8b]=5;entry["sram"][0xde3]=0x54;entry["sram"][0xd8f+4*5]=0;
  for(int city=0;city<30;++city)if((entry["sram"][city*36].get<int>()&7)==3)entry["sram"][city*36]=(entry["sram"][city*36].get<int>()&248)|5;
  entry["events"]=Json::array();for(int i=0;i<64;++i)entry["events"].push_back({{"kind","ai"},{"ruler",3},{"battle_result",0},{"source",255},{"target",255}});
  call(session.restore(entry));call(session.begin_tactics());
  for(const auto &event:held["battle"]["tactics"]["moves"]){
   const auto kind=event.value("kind",std::string("move"));
   if(kind=="computer_attack")call(session.advance_computer_attack());
   else if(kind=="move")call(session.move_tactical(event["slot"],event["direction"]));
   else if(kind=="end_tactical_turn"){call(session.select_tactical_unit(event["slot"]));call(session.end_tactical_turn());}
   else check(false,"Unexpected fixture setup event");
  }
  call(session.resume_computer_attacker_retreat());call(session.finish_tactical_retreat());
  for(int i=0;i<50;++i){call(session.advance_withdrawal_result());if(session.save()["battle"]["tactics"]["settlement"]["stage"]==4)break;}
  call(session.finish_withdrawal_result());const auto saved=session.save();
  check(saved["battle"].is_null()&&saved["phase"]=="ai_turn"&&saved["sram"][0xd8b]==0,"Last invading ruler returns to first ruler of next month");
  check(saved["events"].size()==64&&saved["events"].back()["kind"]=="month"&&saved["events"][62]["ruler"]==5,"Month rotation and invasion result fit save event bound");
 }
 std::cout<<"Original invasion result:32 retreat flows,3 strategic-return flows,human withdrawal,ordinary/commander/full-city AI retreats and complete settlement/replay passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
