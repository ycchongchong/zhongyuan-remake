#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try{
 check(argc==2||argc==3,"Expected ROM and optional generated-fixture path");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 std::ifstream f(base+"remaining-results.json");const auto rows=Json::parse(f);
 for(const auto &c:rows){
  OriginalState state(rom,read(base+c["before"].get<std::string>()));auto captives=c["captives_before"];std::uint8_t cursor=c["cursor_before"];
  const auto result=state.withdrawal_result_step(c["target"],c["source"],c["stage"],captives,cursor);
  check(!result.contains("error")&&result["stage"]==c["stage_after"],"Original phase "+c["id"].dump()+": "+result.dump());
  check(state.sram()==read(base+c["after"].get<std::string>())&&captives==c["captives_after"]&&cursor==c["cursor_after"],"Original full SRAM/captives/RNG "+c["id"].dump());
 }
 for(const auto *name:{"attacker-result-flow","defender-defeat-flow"}){
  std::ifstream file(base+name+".json");const auto flow=Json::parse(file);const auto &steps=flow["steps"];
  OriginalState state(rom,read(base+steps[0]["before"].get<std::string>()));auto captives=steps[0]["captives_before"];
  for(const auto &step:steps){
   check(state.sram()==read(base+step["before"].get<std::string>())&&captives==step["captives_before"],"Continuous original dispatch has no omitted SRAM/captive writes");
   std::uint8_t cursor=step["cursor_before"];
   if(step["stage"]=="close")state.close_battle(flow["target"]);
   else check(!state.withdrawal_result_step(flow["target"],flow["source"],step["stage"],captives,cursor).contains("error"),"Continuous original step");
   check(state.sram()==read(base+step["after"].get<std::string>())&&captives==step["captives_after"]&&cursor==step["cursor_after"],"Continuous original flow reaches strategy exactly with measured entry cursors");
  }
 }
 OriginalSession session(rom),loaded(rom);std::ifstream file(project+"/tests/tactical-defender-clash.json");const auto clash_entry=Json::parse(file);
 auto replay=[&](){const auto save=session.save();auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Strict result replay: "+error);};
 auto surrender=[&](int officer){
  const auto save=session.save();const auto &clash=save["battle"]["tactics"]["attack"]["clash"];const int side=clash["first"]==officer?0:1;
  for(int i=0;i<3;++i)check(session.cycle_clash_order(side,3).empty(),"Set ordinary surrender order");
  check(session.request_clash_surrender().empty()&&session.answer_clash_surrender(true).empty()&&session.advance_clash_surrender().empty()&&session.advance_clash_surrender().empty(),"Real surrender creates captive ledger");replay();
 };
 auto settle=[&](int reason){
  const auto before=session.save();check(before["battle"]["tactics"]["turn_reason"]==reason,"Correct original result reason");
  check(!session.finish_withdrawal_result().empty(),"Cannot close incomplete result");
  const int final_stage=reason==36?17:29;
  for(int i=0;i<42;++i){check(session.advance_withdrawal_result().empty(),"Advance result");replay();if(session.save()["battle"]["tactics"]["settlement"]["stage"]==final_stage)break;}
  const auto report=session.save();check(report["battle"]["tactics"]["settlement"]["stage"]==final_stage,"Reach final report");
  if(reason==36)for(int i=0;i<16;++i)check(report["sram"][12*36+i]==before["sram"][12*36+i],"Attacker withdrawal preserves city ownership and all resources");
  else check((report["sram"][12*36].get<int>()&7)==4,"Defender defeat transfers city");
  check(!session.advance_withdrawal_result().empty()&&session.save()==report,"Cannot repeat completed result");
  auto forged=report;forged["battle"]["tactics"]["turn_reason"]=reason==36?3:36;check(!loaded.restore(forged).empty()&&loaded.save()==report,"Cannot forge a different result branch");
  check(session.finish_withdrawal_result().empty()&&session.save()["phase"]=="player_commands"&&session.save()["battle"].is_null(),"Return to current strategic turn");replay();
 };
 for(int officer:{12,145}){
  check(session.restore(clash_entry).empty(),"Restore legal counterattack");surrender(officer);
  check(!session.begin_defender_defeat_result().empty(),"Cannot settle army defeat while defenders remain");
  check(session.finish_clash_result().empty()&&session.end_tactical_turn().empty(),"Return to attacker turn");
  check(session.begin_tactical_retreat(2).empty()&&session.confirm_tactical_retreat().empty(),"Commander withdraws surviving army");replay();
  const auto held=session.save();check(session.finish_tactical_retreat().empty(),"Continue legacy captive-held retreat");replay();settle(36);
  const auto closed=session.save();const int city=officer==12?13:12;bool found=false;
  for(int i=0;i<12;++i)found|=closed["sram"][city*36+16+i]==officer;
  check(found,"Captive enters exact original destination, without fallback");
  check(loaded.restore(held).empty()&&loaded.save()==held,"Old held retreat save still loads exactly");
 }
 // Keep only the ordinary defender in a controlled legal deployment. Move the
 // other residents to a friendly city, then replay actual movement and attack.
 auto setup=clash_entry;setup["sram"]=setup["battle"]["tactics"]["deployment_sram"];setup["battle"].erase("tactics");
 auto &raw=setup["sram"];
 for(int slot:{0,2,3}){
  const int id=raw[0xdaa+slot*2],cell=raw[0xdab+slot*2];raw[0xe1a+cell]=raw[0xf1a+cell];raw[0xdaa+slot*2]=255;raw[0xdab+slot*2]=255;
  for(int i=0;i<12;++i)if(raw[12*36+16+i]==id)raw[12*36+16+i]=255;
  for(int i=0;i<12;++i)if(raw[14*36+16+i]==255){raw[14*36+16+i]=id;break;}
 }
 const int last_position=raw[0xdad];raw[0xdaa]=12;raw[0xdab]=last_position;raw[0xdac]=255;raw[0xdad]=255;raw[0xe1a+last_position]=16;
 setup["battle"]["defenders"]=Json::array({12});const auto setup_error=session.restore(setup);check(setup_error.empty(),"Controlled single-defender deployment restores: "+setup_error);check(session.begin_tactics().empty(),"Begin controlled single-defender tactics");
 for(const auto &move:clash_entry["battle"]["tactics"]["moves"]){
  const auto kind=move.value("kind",std::string("move"));std::string error;
  if(kind=="end_tactical_turn")error=session.end_tactical_turn();
  else if(kind=="move")error=session.move_tactical(0,move["direction"]);
  else if(kind=="attack")error=session.attack_tactical(0,move["direction"]);
  else if(kind=="confirm_attack")error=session.confirm_tactical_attack();
  else if(kind=="begin_clash")error=session.begin_clash();else throw std::runtime_error("Unknown fixture action");
  if(!error.empty()&&(kind=="move"||kind=="attack")){
   check(session.end_tactical_turn().empty()&&session.end_tactical_turn().empty(),"Single defender renews mobility through both legal turns");
   error=kind=="move"?session.move_tactical(0,move["direction"]):session.attack_tactical(0,move["direction"]);
  }
  check(error.empty(),"Legal last-defender counterattack: "+error);
 }
 const auto last_defender=session.save();replay();if(argc==3){std::ofstream out(argv[2]);out<<last_defender.dump()<<"\n";}
 surrender(12);check(session.snapshot()["battle"]["army_defeat_result_available"]==true,"Last ordinary defender surrender exposes reason 03");
 check(session.begin_defender_defeat_result().empty(),"Enter actual army defeat result");replay();settle(3);
 // Ruler-defeat priority cannot be bypassed by the new ordinary-army path.
 std::ifstream ruler(project+"/tests/clash-defeat-3.json");check(session.restore(Json::parse(ruler)).empty(),"Load original ruler defeat boundary");
 check(session.advance_clash_defeat().empty()&&session.advance_clash_defeat().empty(),"Reach ruler defeat result");const auto king=session.save();
 check(!session.begin_defender_defeat_result().empty()&&session.save()==king,"Ruler result remains separate and unchanged");
 std::cout<<"Original remaining results "<<rows.size()<<" samples; two continuous flows, both captive sides, army defeat, ruler guard and strict replay passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
