#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool value,const std::string &message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string &path){std::ifstream f(std::filesystem::u8path(path),std::ios::binary);check(bool(f),path);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &path){std::ifstream f(std::filesystem::u8path(path));check(bool(f),path);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2||argc==3,"Expected ROM and optional generated-fixture directory");
 OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"commander-result.json");
 for(const auto &row:rows){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));auto captives=row["captives_before"];std::uint8_t cursor=row["cursor_before"];
  const auto result=state.withdrawal_result_step(row["target"],row["source"],row["stage"],captives,cursor);
  check(!result.contains("error")&&result["stage"]==row["stage_after"],"Original commander phase "+row["id"].dump()+": "+result.dump());
  check(state.sram()==read(base+row["after"].get<std::string>())&&captives==row["captives_after"]&&cursor==row["cursor_after"],"Original full SRAM/captives/RNG "+row["id"].dump());
 }
 const auto flow=json(base+"commander-result-flow.json");const auto &steps=flow["steps"];
 OriginalState state(rom,read(base+steps[0]["before"].get<std::string>()));auto captives=steps[0]["captives_before"];
 for(const auto &step:steps){
  check(state.sram()==read(base+step["before"].get<std::string>())&&captives==step["captives_before"],"No omitted SRAM/captive writes between original dispatches");
  std::uint8_t cursor=step["cursor_before"];
  if(step["stage"]=="close")state.close_battle(flow["target"]);
  else check(!state.withdrawal_result_step(flow["target"],flow["source"],step["stage"],captives,cursor).contains("error"),"Continuous original result step");
  check(state.sram()==read(base+step["after"].get<std::string>())&&captives==step["captives_after"]&&cursor==step["cursor_after"],"Continuous original result reaches strategic phase with measured entry cursors");
 }
 // Invalid lists must not partially transfer an earlier valid officer.
 const auto raw=state.sram();captives=std::vector<int>(24,255);captives[0]=145;captives[23]=241;auto invalid_captives=captives;std::uint8_t cursor=255;
 check(state.withdrawal_result_step(12,13,1,captives,cursor).contains("error")&&state.sram()==raw&&captives==invalid_captives&&cursor==255,"Invalid captive input is atomic");
 OriginalSession session(rom),loaded(rom);const auto clash=json(project+"/tests/tactical-defender-clash.json");
 auto replay=[&](){const auto save=session.save();const auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Strict commander result replay: "+error);};
 auto settle=[&](){
  const auto before=session.save();check(!session.finish_withdrawal_result().empty(),"Premature strategic return rejected");
  for(int i=0;i<42;++i){const auto error=session.advance_withdrawal_result();check(error.empty(),"Advance commander result: "+error);replay();if(session.save()["battle"]["tactics"]["settlement"]["stage"]==4)break;}
  const auto report=session.save();check(report["battle"]["tactics"]["settlement"]["stage"]==4,"Commander report complete");
  for(int i=0;i<16;++i)check(report["sram"][12*36+i]==before["sram"][12*36+i],"Commander loss preserves defending ownership and resources");
  check(!session.advance_withdrawal_result().empty()&&session.save()==report,"Completed report cannot run twice");
  auto forged=report;forged["battle"]["tactics"]["turn_reason"]=36;
  check(!loaded.restore(forged).empty()&&loaded.save()==report,"Cannot forge attacker withdrawal to change prisoner destinations");
  check(session.finish_withdrawal_result().empty()&&session.save()["phase"]=="player_commands"&&session.save()["battle"].is_null(),"Commander result returns to current strategic turn");replay();
  const auto closed=session.save();for(int i=0;i<59;++i)check(closed["sram"][0xdaa+i]==255,"Battle army ledgers cleared");
  for(int i=0;i<30;++i)for(int j:{30,31})check(closed["sram"][i*36+j]==before["sram"][i*36+j],"Result does not reset city command books");
 };
 auto setup=clash;setup["sram"]=clash["battle"]["tactics"]["deployment_sram"];setup["battle"].erase("tactics");setup["battle"]["leader"]=145;
 for(int slot=0;slot<11;++slot){auto &s=setup["sram"];if(s[0xdc2+slot*3]==255)continue;const bool leader=s[0xdc2+slot*3]==145;s[0xdc4+slot*3]=(s[0xdc4+slot*3].get<int>()&127)|(leader?128:0);s[0xe1a+s[0xdc3+slot*3].get<int>()]=0x30|slot|(leader?64:0);}
 for(bool last:{false,true}){
  check(session.restore(setup).empty()&&session.begin_tactics().empty(),"Controlled commander deployment starts");
  if(last)for(int slot:{1,2})check(session.begin_tactical_retreat(slot).empty()&&session.confirm_tactical_retreat().empty()&&session.finish_tactical_retreat().empty(),"Other attackers withdraw before commander loss");
  for(const auto &move:clash["battle"]["tactics"]["moves"]){
   const auto kind=move.value("kind",std::string("move"));std::string error;
   if(kind=="end_tactical_turn")error=session.end_tactical_turn();
   else if(kind=="move")error=session.move_tactical(move["slot"],move["direction"]);
   else if(kind=="attack")error=session.attack_tactical(move["slot"],move["direction"]);
   else if(kind=="confirm_attack")error=session.confirm_tactical_attack();
   else if(kind=="begin_clash")error=session.begin_clash();else throw std::runtime_error("Unknown fixture action");
   check(error.empty(),"Legal commander approach: "+error);
  }
  replay();if(argc==3){std::ofstream out(std::string(argv[2])+(last?"/tactical-last-commander-clash.json":"/tactical-commander-clash.json"));check(bool(out),"Create generated fixture");out<<session.save().dump()<<"\n";}
  const auto save=session.save();const int side=save["battle"]["tactics"]["attack"]["clash"]["first"]==145?0:1;
  for(int i=0;i<3;++i)check(session.cycle_clash_order(side,3).empty(),"Set commander surrender order");
  check(session.request_clash_surrender().empty()&&session.answer_clash_surrender(true).empty()&&session.advance_clash_surrender().empty()&&session.advance_clash_surrender().empty(),"Actual surrender removes commander");replay();
  check(session.snapshot()["battle"]["commander_defeat_result_available"]==true&&session.snapshot()["battle"]["army_defeat_result_available"]==false,"C5F1 selects commander reason 20");
  const auto held=session.save();check(!session.finish_clash_result().empty()&&session.save()==held,"Legacy ordinary continuation stays rejected");
  check(session.begin_commander_defeat_result().empty()&&session.save()["battle"]["tactics"]["turn_reason"]==20,"Explicit replayable commander result transition");replay();settle();
  const auto closed=session.save();bool found=false;for(int i=0;i<12;++i)found|=closed["sram"][12*36+16+i]==145;check(found,"Captured commander stays in defending city");
  for(int id:{143,144}){found=false;for(int i=0;i<12;++i)found|=closed["sram"][13*36+16+i]==id;check(found,"Surviving attacker returns to own source city");}
  check(loaded.restore(held).empty()&&loaded.save()==held,"Old held commander save still restores exactly");
 }
 check(session.restore(json(project+"/tests/clash-defeat-2.json")).empty(),"Restore actual commander defeat history");
 check(session.advance_clash_defeat().empty()&&session.advance_clash_defeat().empty()&&session.begin_commander_defeat_result().empty(),"Direct defeat reaches same original commander branch");replay();settle();
 for(const auto *name:{"clash-defeat-0","clash-defeat-3"}){
  check(session.restore(json(project+"/tests/"+name+".json")).empty()&&session.advance_clash_defeat().empty()&&session.advance_clash_defeat().empty(),"Other original defeat entry");const auto before=session.save();
  check(session.snapshot()["battle"]["commander_defeat_result_available"]==false&&!session.begin_commander_defeat_result().empty()&&session.save()==before,"Ordinary and ruler defeat cannot enter commander settlement");
 }
 std::cout<<"Original commander result "<<rows.size()<<" samples; continuous flow, surrendered/defeated/last commander, preserved city, strict replay and strategic return passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
