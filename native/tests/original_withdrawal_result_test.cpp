#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 std::ifstream f(base+"withdrawal-result.json");const auto rows=Json::parse(f);
 for(const auto &c:rows){
  OriginalState state(rom,read(base+c["before"].get<std::string>()));auto captives=c["captives_before"];std::uint8_t cursor=c["cursor_before"];
  auto result=state.withdrawal_result_step(c["target"],c["source"],c["stage"],captives,cursor);
  check(!result.contains("error")&&result["stage"]==c["stage_after"],"Original result phase "+c["id"].dump()+": "+result.dump());
  check(state.sram()==read(base+c["after"].get<std::string>())&&captives==c["captives_after"]&&cursor==c["cursor_after"],"Original complete SRAM, captives and RNG "+c["id"].dump());
 }
 {
  std::ifstream file(base+"withdrawal-flow.json");const auto flow=Json::parse(file);const auto &steps=flow["steps"];
  OriginalState state(rom,read(base+steps[0]["before"].get<std::string>()));auto captives=steps[0]["captives_before"];
  for(const auto &step:steps){
   check(state.sram()==read(base+step["before"].get<std::string>())&&captives==step["captives_before"],"Continuous original dispatch has no missing inter-step SRAM/captive writes");
   // Renderer/input advances RNG between function calls in the original. Use
   // measured entry cursors; this is not a claim of frame synchronization.
   std::uint8_t cursor=step["cursor_before"];
   if(step["stage"]=="close")state.close_battle(flow["target"]);
   else check(!state.withdrawal_result_step(flow["target"],flow["source"],step["stage"],captives,cursor).contains("error"),"Continuous original result step");
   check(state.sram()==read(base+step["after"].get<std::string>())&&captives==step["captives_after"]&&cursor==step["cursor_after"],"Continuous dispatch matches through strategic return");
  }
 }
 {
  const auto raw=read(base+rows[0]["before"].get<std::string>());OriginalState state(rom,raw);Json captives=std::vector<int>(24,255);std::uint8_t cursor=55;
  auto bad=captives;bad[23]=std::uint64_t(1)<<32;
  check(state.withdrawal_result_step(12,13,9,bad,cursor).contains("error")&&state.sram()==raw&&cursor==55,"Oversized captive rejected atomically");
  for(int stage:{-1,8,13})check(state.withdrawal_result_step(12,13,stage,captives,cursor).contains("error")&&state.sram()==raw&&cursor==55,"Wrong settlement stage rejected");
 }
 OriginalSession session(rom),loaded(rom);std::ifstream fixture(project+"/tests/tactical-turn-start.json");const auto entry=Json::parse(fixture);
 auto replay=[&](){auto save=session.save();auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Settlement replays exactly: "+error);};
 auto retire=[&](){
  for(int slot=0;slot<12;++slot)if(session.save()["sram"][0xdaa+slot*2]!=255){
   check(session.begin_tactical_retreat(slot).empty()&&session.confirm_tactical_retreat().empty()&&session.finish_tactical_retreat().empty(),"Retire remaining defenders");
  }
 };
 auto settle=[&](){
  const auto boundary=session.save();check(!session.finish_withdrawal_result().empty()&&session.save()==boundary,"Cannot close before transfers");
  for(int i=0;i<45;++i){
   check(session.advance_withdrawal_result().empty(),"Advance one original result step");replay();
   const auto step_save=session.save();const auto &t=step_save["battle"]["tactics"];
   if(t["settlement"]["stage"]==13)break;
  }
  const auto final=session.save();const auto &t=final["battle"]["tactics"];
  check(t["settlement"]["stage"]==13&&(final["sram"][12*36].get<int>()&7)==4,"Occupation completed");
  for(const auto &id:t["captives"])check(id==255,"All captive slots resolved before report");
  check(!session.advance_withdrawal_result().empty()&&session.save()==final,"Report does not repeat resource losses");
  auto forged=final;forged["sram"][12*36+1]=0;
  check(!loaded.restore(forged).empty()&&loaded.save()==final,"Forged occupation rejected atomically");
  const auto books=final["sram"][0xd8f+4*4];
  check(session.finish_withdrawal_result().empty(),"Confirm report and return to strategy");replay();
  auto closed=session.save();check(closed["battle"].is_null()&&closed["phase"]=="player_commands"&&closed["sram"][0xd8f+4*4]==books,"Resume current ruler without resetting command books");
  for(int i=0;i<59;++i)check(closed["sram"][0xdaa+i]==255,"Original battle ledger cleanup");
  check(closed["sram"][12*36+32]==255&&closed["sram"][12*36+33]==255,"Siege markers cleared");
  check(!session.finish_withdrawal_result().empty()&&session.save()==closed,"Repeated close rejected");
 };
 check(session.restore(entry).empty(),"Restore original two-player tactical entry");
 check(!session.advance_withdrawal_result().empty()&&session.save()==entry,"No premature occupation during tactics");
 check(session.end_tactical_turn().empty(),"Defending turn");retire();replay();settle();
 // Each captive side comes from legal clash surrender, not injected save fields.
 for(int officer:{12,145}){
  std::ifstream clash_file(project+"/tests/tactical-defender-clash.json");const auto clash_entry=Json::parse(clash_file);
  check(session.restore(clash_entry).empty(),"Restore counterattack");const auto &clash=clash_entry["battle"]["tactics"]["attack"]["clash"];
  const int side=clash["first"]==officer?0:1;
  for(int i=0;i<3;++i)check(session.cycle_clash_order(side,3).empty(),"Select surrender");
  check(session.request_clash_surrender().empty()&&session.answer_clash_surrender(true).empty()&&session.advance_clash_surrender().empty()&&session.advance_clash_surrender().empty()&&session.finish_clash_result().empty(),"Generate real captive ledger");
  retire();replay();settle();
  const auto closed=session.save();bool found=false;
  for(int c=0;c<30;++c)for(int i=0;i<12;++i)if(closed["sram"][c*36+16+i]==officer){found=true;check((closed["sram"][c*36].get<int>()&7)==(officer==12?4:2),"Captive joins the capturing army's city");}
  check(found,"Settled captive remains in a city roster");
 }
 std::cout<<"ROM withdrawal settlement "<<rows.size()<<" full SRAM/captive/RNG samples; ordinary and both captive paths, replay, occupation and strategic return passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
