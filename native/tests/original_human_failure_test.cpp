#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool value,const std::string &message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string &path){std::ifstream f(std::filesystem::u8path(path),std::ios::binary);check(bool(f),path);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &path){std::ifstream f(std::filesystem::u8path(path));check(bool(f),path);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"human-failure.json");
 for(const auto &row:rows){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));
  const auto result=state.human_failure_step(row["loser"],row["winner"],row["inputs"]);
  check(!result.contains("error")&&result["phase"]==row["phase"],"Original failure/controller decision "+row["id"].dump());
  check(state.sram()==read(base+row["after"].get<std::string>())&&row["cursor_before"]==row["cursor_after"],"Original failure report makes no SRAM/RNG changes");
 }
 {
  const auto bridge=json(base+"human-failure-bridge.json");OriginalState state(rom,read(base+bridge["before"].get<std::string>()));
  check(state.restore_tactical_board(bridge["target"]).empty()&&state.sram()==read(base+bridge["after"].get<std::string>()),"Full original failure-to-tactics bridge SRAM");
  auto before=state.sram();check(state.human_failure_step(-1,2,0).contains("error")&&state.human_failure_step(4,4,0).contains("error")&&state.human_failure_step(4,2,4).contains("error")&&state.sram()==before,"Invalid failure arguments are atomic");
 }
 OriginalSession session(rom),loaded(rom);
 auto replay=[&](){const auto save=session.save();const auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Strict human failure replay: "+error);};
 for(const auto *name:{"clash-single-ruler-defeat","clash-defeat-3"}){
  const bool single=std::string(name)=="clash-single-ruler-defeat";
  const auto entry=json(project+"/tests/"+name+".json");check(session.restore(entry).empty(),"Restore legal defeat history");
  check(!session.begin_human_failure().empty()&&!session.finish_human_failure().empty()&&session.save()==entry,"Premature failure cannot mutate battle");
  check(session.advance_clash_defeat().empty()&&session.advance_clash_defeat().empty(),"Confirm actual human ruler defeat");
  const auto held=session.save();check(session.snapshot()["battle"]["human_failure_available"]==true,"Human ruler report available");
  check(!session.finish_clash_result().empty()&&!session.begin_ruler_defeat_result().empty(),"Ordinary and NPC paths remain guarded");
  check(session.begin_human_failure().empty(),"Enter human failure report");replay();const auto report=session.save();
  check(report["sram"]==held["sram"]&&report["random_cursor"]==held["random_cursor"]&&report["battle"]["tactics"]["human_failure"]["can_continue"]==!single,"Report preserves SRAM/RNG and determines surviving players");
  check(!session.begin_human_failure().empty()&&!session.end_tactical_turn().empty()&&!session.advance_ruler_annexation().empty()&&!session.advance_withdrawal_result().empty()&&session.save()==report,"Failure screen locks other actions");
  auto forged=report;forged["battle"]["tactics"]["human_failure"]["winner"]=0;check(!loaded.restore(forged).empty()&&loaded.save()==report,"Forged winner rejected atomically");
  forged=report;forged["battle"]["tactics"]["human_failure"]["can_continue"]=single;check(!loaded.restore(forged).empty(),"Forged survival cannot unlock continuation");
  if(single){check(!session.finish_human_failure().empty()&&session.save()==report,"Last player failure holds without land/resource writes");continue;}
  check(session.finish_human_failure().empty(),"Living second player resumes battle result");replay();
  check(session.save()["battle"]["tactics"]["turn_reason"]==148&&!session.save()["battle"]["tactics"].contains("attack"),"Human attacking ruler uses original reason 94");
  const auto resumed=session.save();check(!session.finish_human_failure().empty()&&session.save()==resumed,"Failure continuation happens once");
  for(int i=0;i<42;++i){check(session.advance_withdrawal_result().empty(),"Resolve human ruler battle");replay();if(session.save()["battle"]["tactics"]["settlement"]["stage"]==4)break;}
  check(!session.finish_withdrawal_result().empty(),"Human ruler territory cannot be skipped");
  for(int city=0;city<30;++city){check(session.advance_ruler_annexation().empty(),"Annex next city");replay();}
  const auto final=session.save();for(int city=0;city<30;++city)check((final["sram"][city*36].get<int>()&7)!=4,"No defeated human territory remains");
  check(final["sram"][0xd8f+4*4]==0&&final["sram"][0xd89]==held["sram"][0xd89]&&final["sram"][0xd8a]==held["sram"][0xd8a],"Defeated ruler books cleared, player assignments preserved");
  check(session.finish_withdrawal_result().empty()&&session.save()["battle"].is_null()&&session.save()["phase"]!="ending","Surviving human keeps campaign running");replay();
  check(loaded.restore(held).empty()&&loaded.save()==held,"Legacy held human defeat save stays compatible");
 }
 // Cover the other player assignment and defender-ruler branch using actual
 // move/attack/clash histories. The slower history is replayed at boundaries.
 for(const auto *name:{"clash-second-ruler-defeat","clash-defending-human-ruler-defeat"}){
  const bool defending=std::string(name)=="clash-defending-human-ruler-defeat";
  check(session.restore(json(project+"/tests/"+name+".json")).empty(),"Restore alternate human ruler history");
  check(session.advance_clash_defeat().empty()&&session.advance_clash_defeat().empty()&&session.begin_human_failure().empty(),"Other human ruler reaches failure report");replay();
  check(session.save()["battle"]["tactics"]["human_failure"]["can_continue"]==true&&session.save()["battle"]["tactics"]["human_failure"]["loser"]==(defending?2:4),"Surviving primary player may continue");
  check(session.finish_human_failure().empty()&&session.save()["battle"]["tactics"]["turn_reason"]==(defending?131:148),"Correct original side-specific ruler reason");replay();
  for(int i=0;i<42;++i){check(session.advance_withdrawal_result().empty(),"Alternate ruler battle settlement");if(session.save()["battle"]["tactics"]["settlement"]["stage"]==(defending?29:4))break;}
  for(int i=0;i<30;++i)check(session.advance_ruler_annexation().empty(),"Alternate ruler land transfer");replay();
  check(session.finish_withdrawal_result().empty()&&session.save()["battle"].is_null()&&session.save()["phase"]!="ending","Primary player campaign survives secondary defeat");replay();
 }
 std::cout<<"Original human failure "<<rows.size()<<" decisions; original bridge, single-player terminal report, dual-player annexation and strict replay passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
