#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"computer-motion.json");
 for(const auto &row:rows){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));const auto result=state.tactical_ai_motion(row["target"],row["slot"],row["points"],row["statuses"]);
  check(!result.contains("error")&&result["kind"]==row["kind"]&&result["direction"]==row["direction"],"Original motion "+row["id"].dump()+" expected "+row["kind"].dump()+" "+row["direction"].dump()+" got "+result.dump());
  if(result["kind"]=="move"||result["kind"]=="attack")check(result["command"]==row["command"],"Original command byte");
  check(state.sram()==read(base+row["after"].get<std::string>())&&row["cursor_before"]==row["cursor_after"],"Original full SRAM and no RNG in motion "+row["id"].dump());
 }

 for(const auto &flow:json(base+"computer-motion-flow.json")){
  OriginalState state(rom,read(base+flow["before"].get<std::string>()));
  const auto result=state.tactical_ai_motion(flow["target"],flow["slot"],flow["points"],flow["statuses"]);
  check(result["kind"]==flow["kind"]&&result["direction"]==flow["direction"],"Continuous original command");
  const int directions[]={3,2,1,0};int points=flow["points"];const int d=directions[flow["direction"].get<int>()];
  if(flow["kind"]=="move")check(state.tactical_step(flow["slot"],d,points,0).empty(),"Execute original movement");
  else{
   const auto attack=state.tactical_attack(flow["slot"],d,points,0);check(!attack.contains("error"),"Execute original attack");
   const auto clash=state.prepare_clash(attack,true,flow["frame"],false);
   for(const auto &[key,value]:flow["clash"].items())check(clash[key]==value,"Continuous original clash field "+key);
  }
  check(points==flow["points_after"],"Continuous original mobility");
  auto expected=read(base+flow["after"].get<std::string>());
  if(flow["kind"]=="attack"){
   // The NES hides the caster sprite while entering the clash. PC folds this
   // presentation wait and retains canonical tactical occupancy throughout.
   const auto before=read(base+flow["before"].get<std::string>());const int cell=before[0xdab+2*flow["slot"].get<int>()];
   check(expected[0xe1a+cell]==expected[0xf1a+cell]&&before[0xe1a+cell]==(16|flow["slot"].get<int>()),"Explicit original initiator blink boundary");
   expected[0xe1a+cell]=before[0xe1a+cell];
  }
  if(state.sram()!=expected){for(int i=0;i<8192;++i)if(state.sram()[i]!=expected[i])std::cerr<<std::hex<<i<<":"<<int(state.sram()[i])<<"/"<<int(expected[i])<<" ";throw std::runtime_error("Continuous full SRAM mismatch "+flow["kind"].dump());}
 }
 {
  const auto raw=read(base+rows[0]["before"].get<std::string>());OriginalState state(rom,raw);auto status=Json(std::vector<int>(24,0));
  check(state.tactical_ai_motion(30,11,40,status).contains("error")&&state.tactical_ai_motion(12,12,40,status).contains("error")&&state.tactical_ai_motion(12,11,41,status).contains("error")&&state.sram()==raw,"Invalid public inputs are atomic");
  status[0]=-1;check(state.tactical_ai_motion(12,11,40,status).contains("error")&&state.sram()==raw,"Invalid statuses are atomic");
  auto broken=raw;broken[0xe1a+0x88]=0x3f;OriginalState invalid(rom,broken);status[0]=0;
  check(invalid.tactical_ai_motion(12,11,40,status).contains("error")&&invalid.sram()==broken,"Malformed enemy tokens are rejected before indexing ledger");
 }
 for(const std::string name:{"attack","held"}){
  OriginalSession session(rom),loaded(rom);const auto entry=json(project+"/tests/computer-motion-"+name+".json");
  check(session.restore(entry).empty()&&session.save()==entry,"Load legal player movement/AI history "+name);
  auto replay=[&](){const auto save=session.save();const auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Strict motion replay: "+error);};
  check(session.continue_computer_motion().empty(),"Continue after no strategy");replay();const auto acted=session.save();
  check(acted["random_cursor"]==entry["random_cursor"],"Post-strategy motion does not consume extra RNG");
  check(!session.continue_computer_motion().empty()&&!session.end_tactical_turn().empty()&&!session.cancel_tactical_attack().empty()&&session.save()==acted,"No duplicate AI action or human override");
  auto forged=acted;forged["battle"]["tactics"]["computer_motion"]["direction"]=0;check(!loaded.restore(forged).empty()&&loaded.save()==acted,"Forged AI direction rejected atomically");
  forged=acted;forged["battle"]["tactics"]["moves"].back()["frame"]=256;check(!loaded.restore(forged).empty(),"Invalid AI frame rejected");
  if(name=="held"){
   check(acted["battle"]["tactics"]["computer_motion"]["branch"]=="fort_flank"&&acted["sram"]==entry["sram"]&&acted["battle"]["tactics"]["points"]==24,"Unported flanking is explicit and has no fabricated movement");
   check(!session.advance_computer_tactics().empty()&&session.save()==acted,"Unported branch cannot reroll or pass");continue;
  }
  const auto &t=acted["battle"]["tactics"];
  check(t["computer_motion"]["kind"]=="attack"&&t["computer_motion"]["slot"]==3&&t["computer_motion"]["direction"]==3&&t["points"]==21&&t["attack"]["stage"]=="clash_ready","Computer declares rightward attack and pays three points once");
  check(t["attack"]["clash"]["first"]==128&&t["attack"]["clash"]["second"]==145&&(acted["sram"][0x438+128*8+4].get<int>()&3)==(acted["frame_counter"].get<int>()&3),"NPC initiator gets original frame-based formation");
  check(!session.advance_computer_tactics().empty()&&session.save()==acted,"AI cannot continue while clash is pending");
  check(session.begin_clash().empty(),"Enter computer-initiated clash");replay();
  const auto started=session.save();const auto clash=started["battle"]["tactics"]["attack"]["clash"];const int human=clash["first"]==145?0:1;
  check(clash["players"][human]!=0&&clash["players"][human^1]==0&&!session.cycle_clash_order(human^1,3).empty(),"Only the human target receives player orders");
  for(int i=0;i<3;++i)check(session.cycle_clash_order(human,3).empty(),"Set human general surrender order");
  check(session.request_clash_surrender().empty()&&session.answer_clash_surrender(true).empty()&&session.advance_clash_surrender().empty()&&session.advance_clash_surrender().empty(),"Computer attack can complete an actual encounter");replay();
  check(session.finish_clash_result().empty(),"Return from computer-initiated clash");replay();
  check(session.save()["battle"]["tactics"]["computer_cursor"]==3&&session.save()["battle"]["tactics"]["turn_boundary"]=="computer","Clash preserves computer scan position and turn");
  check(session.advance_computer_tactics().empty()&&session.plan_computer_tactics().empty(),"Computer continues after clash result");replay();
  check(session.save()["battle"]["tactics"]["computer_plan"]["slot"]==2,"Next original scan selects previous slot");
 }
 std::cout<<"Original computer motion "<<rows.size()<<" samples passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
