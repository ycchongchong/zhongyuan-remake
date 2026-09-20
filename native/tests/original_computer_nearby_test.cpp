#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"computer-nearby.json");
 for(const auto &row:rows){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));const auto result=state.tactical_ai_motion(row["target"],row["slot"],row["points"],row["statuses"],true);
  check(!result.contains("error")&&result["kind"]==row["kind"]&&result["direction"]==row["direction"],"Original nearby "+row["id"].dump()+" expected "+row["kind"].dump()+" direction "+row["direction"].dump()+" got "+result.dump());
  if(result["kind"]=="move"||result["kind"]=="attack")check(result["command"]==row["command"],"Literal command");
  check(state.sram()==read(base+row["after"].get<std::string>())&&row["cursor_before"]==row["cursor_after"],"Full SRAM and unchanged RNG");
  check(state.tactical_ai_motion(row["target"],row["slot"],row["points"],row["statuses"])["kind"]=="pending","Legacy role boundary remains replayable");
 }

 for(const auto &flow:json(base+"computer-nearby-flow.json")){
  OriginalState state(rom,read(base+flow["before"].get<std::string>()));
  const auto result=state.tactical_ai_motion(flow["target"],flow["slot"],flow["points"],flow["statuses"],true);
  check(result["kind"]==flow["kind"]&&result["direction"]==flow["direction"],"Continuous original nearby command");
  const int directions[]={3,2,1,0};int points=flow["points"];const int d=directions[flow["direction"].get<int>()];
  if(flow["kind"]=="move")check(state.tactical_step(flow["slot"],d,points,0).empty(),"Execute original nearby movement");
  else{
   const auto attack=state.tactical_attack(flow["slot"],d,points,0);check(!attack.contains("error"),"Execute original nearby attack");
   const auto clash=state.prepare_clash(attack,true,flow["frame"],false);
   for(const auto &[key,value]:flow["clash"].items())check(clash[key]==value,"Continuous original clash field "+key);
  }
  check(points==flow["points_after"],"Continuous original mobility");
  auto expected=read(base+flow["after"].get<std::string>());
  if(flow["kind"]=="attack"){
   const auto before=read(base+flow["before"].get<std::string>());const int cell=before[0xdab+2*flow["slot"].get<int>()];
   check(expected[0xe1a+cell]==expected[0xf1a+cell],"Documented original initiator blink");expected[0xe1a+cell]=before[0xe1a+cell];
  }
  check(state.sram()==expected,"Continuous full SRAM (attack initiator blink explicitly normalized)");
 }
 for(int role:{1,3})for(const std::string kind:{"move","attack"}){
  OriginalSession session(rom),loaded(rom);const auto entry=json(project+"/tests/computer-nearby-"+std::to_string(role)+"-"+kind+".json");
  check(session.restore(entry).empty()&&session.save()==entry,"Restore legal legacy pending role");
  auto replay=[&](){const auto save=session.save();const auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Strict nearby replay: "+error);};
  check(session.continue_computer_nearby().empty(),"Continue original nearby role");replay();const auto acted=session.save();const auto &t=acted["battle"]["tactics"];
  check(t["computer_motion"]["kind"]==kind&&acted["random_cursor"]==entry["random_cursor"],"Expected action with no new RNG");
  check(!session.continue_computer_nearby().empty()&&!session.end_tactical_turn().empty()&&session.save()==acted,"No duplicate or human override");
  auto forged=acted;forged["battle"]["tactics"]["computer_motion"]["direction"]=99;check(!loaded.restore(forged).empty()&&loaded.save()==acted,"Reject forged direction atomically");
  forged=acted;forged["battle"]["tactics"]["moves"].back()["frame"]=256;check(!loaded.restore(forged).empty(),"Reject invalid frame");
  if(kind=="attack"){
   check(t["points"]==entry["battle"]["tactics"]["points"].get<int>()-3&&t["attack"]["stage"]=="clash_ready","Charge attack once and prepare encounter");
   check(!session.advance_computer_tactics().empty()&&!session.cancel_tactical_attack().empty(),"Cannot skip/cancel pending computer clash");
   check(session.begin_clash().empty(),"Enter nearby-initiated clash");replay();
   const auto clash=session.save()["battle"]["tactics"]["attack"]["clash"];const int human=clash["players"][0]!=0?0:1;
   for(int i=0;i<3;++i)check(session.cycle_clash_order(human,3).empty(),"Set human surrender order");
   check(session.request_clash_surrender().empty()&&session.answer_clash_surrender(true).empty()&&session.advance_clash_surrender().empty()&&session.advance_clash_surrender().empty(),"Resolve actual nearby-initiated encounter");replay();
   check(session.finish_clash_result().empty(),"Return to same computer turn");replay();
  }else check(t["points"].get<int>()<entry["battle"]["tactics"]["points"].get<int>()&&acted["sram"]!=entry["sram"],"Movement updates actual board and costs");
  check(session.advance_computer_tactics().empty()&&session.plan_computer_tactics().empty(),"Continue after nearby action");replay();
  check(session.save()["battle"]["tactics"]["computer_plan"]["slot"]==entry["battle"]["tactics"]["selected"].get<int>()-1,"Next original descending selection");
 }
 {
  auto raw=read(base+rows[0]["before"].get<std::string>());OriginalState state(rom,raw);auto status=Json(std::vector<int>(24,0));status[0]=256;
  check(state.tactical_ai_motion(12,11,40,status,true).contains("error")&&state.sram()==raw,"Invalid status rejected atomically");
  status[0]=0;check(state.tactical_ai_motion(30,11,40,status,true).contains("error")&&state.tactical_ai_motion(12,12,40,status,true).contains("error")&&state.sram()==raw,"Invalid target and unit rejected");
  OriginalSession session(rom);const auto entry=json(project+"/tests/computer-flank-scan.json");check(session.restore(entry).empty(),"Load unrelated role boundary");
  check(!session.continue_computer_nearby().empty()&&session.save()==entry,"Cannot bypass flank with nearby continuation");
 }

 for(const auto &flow:json(base+"computer-rejected-attack.json")){
  const auto &before=flow["steps"][0],after=flow["steps"].back();const auto raw=read(base+before["sram"].get<std::string>());OriginalState state(rom,raw);
  const auto decision=state.tactical_ai_motion(12,flow["slot"],flow["points"],before["status"],true);
  check(decision["kind"]=="attack"&&decision["command"]==flow["steps"][1]["command"],"Original attack issued despite insufficient mobility");
  const int directions[]={3,2,1,0};int points=flow["points"];
  check(state.tactical_attack(flow["slot"],directions[decision["direction"].get<int>()],points,0).contains("error")&&points==flow["points"],"Native attack remains atomic when budget too low");
  Json context={{"side",0},{"points",0},{"round",before["round"]},{"carry",before["carry"]},{"phase",5},{"counter",1},{"reason",0},{"status",before["status"]}};
  const auto result=state.tactical_handover(context);
  for(const auto *key:{"side","round","carry","points","status"})check(result[key]==after[key],std::string("DF27 rejection handover ")+key);
  check(state.sram()==read(base+after["sram"].get<std::string>()),"Rejected original attack leaves all SRAM unchanged through handover");
 }
 for(const std::string fixture:{"computer-exhausted-attack.json","natural-nearby-exhausted.json"}){
  OriginalSession session(rom),loaded(rom);const auto entry=json(project+"/tests/"+fixture);
  check(session.restore(entry).empty()&&session.save()==entry,"Replay legal history reaching exhausted attack: "+fixture+"");
  check(session.continue_computer_nearby()=="攻击需要 3 点机动力"&&session.save()==entry,"Historical attempted-action behavior stays atomic and compatible");
  check(session.finish_exhausted_computer_attack().empty(),"Continue original DF27 exhausted attack");const auto after=session.save();const auto &turn=after["battle"]["tactics"];
  check(!turn.contains("turn_boundary")&&!turn.contains("attack")&&turn["side"]==128&&turn["round"]==entry["battle"]["tactics"]["round"].get<int>()+1&&turn["carry"]==0,"Exhausted attack yields player turn without clash or saved bonus");
  check(after["sram"]==entry["sram"]&&after["random_cursor"]==entry["random_cursor"]&&turn["last_exhausted_attack"]["points_before"]==entry["battle"]["tactics"]["points"],"No damage, formation or RNG mutation from rejected attack");
  check(loaded.restore(after).empty()&&loaded.save()==after,"Strict replay of exhausted attack continuation");
  check(!session.finish_exhausted_computer_attack().empty()&&session.save()==after,"No repeated exhaustion on player turn");
  auto forged=after;forged["battle"]["tactics"]["last_exhausted_attack"]["slot"]=0;check(!loaded.restore(forged).empty()&&loaded.save()==after,"Forged exhaustion report rejected atomically");
  check(session.restore(json(project+"/tests/computer-nearby-1-attack.json")).empty(),"Restore funded attack");const auto funded=session.save();
  check(!session.finish_exhausted_computer_attack().empty()&&session.save()==funded,"Cannot use exhaustion to skip a funded attack");
 }
 std::cout<<"Original computer nearby "<<rows.size()<<" samples passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
