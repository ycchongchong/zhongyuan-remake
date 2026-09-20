#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"computer-scan.json");
 for(const auto &row:rows){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));const auto result=state.tactical_ai_scan(row["side"],row["previous"],row["marker"]);
  check(!result.contains("error")&&result["kind"]==row["kind"]&&result["slot"]==row["slot"]&&result["marker"]==row["marker_after"],"Original scan "+row["id"].dump()+" "+result.dump());
  if(result["kind"]=="end_turn")check(result["command"]==row["command"],"Literal command 4");
  check(state.sram()==read(base+row["after"].get<std::string>())&&row["cursor_before"]==row["cursor_after"],"Full SRAM and unchanged RNG");
 }
 {
  auto raw=read(base+rows[0]["before"].get<std::string>());OriginalState state(rom,raw);
  for(const auto &input:std::vector<std::vector<int>>{{1,0,255},{0,-1,255},{0,12,255},{128,11,255},{0,0,-1},{0,0,16},{0,0,256}})
   check(state.tactical_ai_scan(input[0],input[1],input[2]).contains("error")&&state.sram()==raw,"Reject invalid slot, side or marker atomically");
  raw[0xdaa]=241;OriginalState invalid(rom,raw);check(invalid.tactical_ai_scan(0,0,255).contains("error")&&invalid.sram()==raw,"Reject malformed ledger");
 }
 for(const std::string name:{"scan","retry","move"}){
  OriginalSession session(rom),loaded(rom);check(session.restore(json(project+"/tests/computer-flank-"+name+".json")).empty(),"Restore old pre-flank boundary");
  const auto entry=session.save();check(!session.continue_computer_scan().empty()&&session.save()==entry,"Cannot skip unexamined current unit");
  check(session.continue_computer_flank().empty(),"Reach scan/retry/reuse");
  if(name=="retry")check(session.retry_computer_strategy().empty(),"Retry before scan");
  if(name=="move")for(int i=0;i<2;++i){
   check(session.advance_computer_tactics().empty()&&session.plan_computer_tactics(true).empty()&&session.evaluate_computer_strategy().empty()&&session.continue_computer_motion().empty()&&session.continue_computer_flank().empty(),"Two cached steps reach no-command reused-unit boundary");
  }
  const auto before=session.save();check(before["battle"]["tactics"]["computer_motion"]["kind"]=="scan","Reached actual no-command boundary");
  check(session.continue_computer_scan().empty(),"Continue old scan boundary");auto current=session.save();auto &t=current["battle"]["tactics"];
  check(t["computer_plan"]["slot"]==(name=="retry"?3:2)&&t["computer_scan"]["marker"]==(name=="move"?2:11),"Original first-candidate marker including A0 reuse");
  check(current["random_cursor"]==before["random_cursor"]&&t["points"]==before["battle"]["tactics"]["points"],"Selecting next unit spends no RNG or mobility");
  check(loaded.restore(current).empty()&&loaded.save()==current,"Replay scan-selected unit exactly");
  auto forged=current;forged["battle"]["tactics"]["computer_scan"]["marker"]=0;check(!loaded.restore(forged).empty()&&loaded.save()==current,"Reject forged scan origin atomically");
  check(!session.continue_computer_scan().empty()&&!session.advance_computer_tactics().empty()&&session.save()==current,"Cannot skip selected unit or repeat assessment");
  if(name=="scan"){
   std::vector<int> selected{2};
   for(int i=0;i<20&&session.save()["battle"]["tactics"].contains("turn_boundary");++i){
    auto now=session.save();const auto &turn=now["battle"]["tactics"];
    std::string error;
    if(!turn.contains("computer_strategy"))error=session.evaluate_computer_strategy();
    else if(!turn.contains("computer_motion"))error=session.continue_computer_motion();
    else if(turn["computer_motion"]["kind"]=="pending")error=session.continue_computer_flank();
    else if(turn["computer_motion"]["kind"]=="retry_strategy")error=session.retry_computer_strategy();
    else {error=session.continue_computer_scan();const auto after=session.save();if(after["battle"]["tactics"].contains("computer_plan"))selected.push_back(after["battle"]["tactics"]["computer_plan"]["slot"]);}
    check(error.empty(),"No-action cycle: "+error+" strategy="+turn.value("computer_strategy",Json{}).dump()+" motion="+turn.value("computer_motion",Json{}).dump()+" plan="+turn.value("computer_plan",Json{}).dump());current=session.save();check(loaded.restore(current).empty()&&loaded.save()==current,"Replay every scan/strategy/motion boundary");
   }
   current=session.save();const auto &end=current["battle"]["tactics"];
   check(selected==std::vector<int>({2,1,0}),"Visit each remaining defender in descending order");
   check(!end.contains("turn_boundary")&&end["side"]==128&&end["round"]==1&&end["points"]==30&&end["carry"]==0&&end["computer_cursor"]==11,"Full cycle ends with original unchanged carry, attacker budget and round");
   check(end["last_computer_scan"]["command"]==4&&end["last_computer_scan"]["marker"]==11&&end["last_computer_scan"]["points_before"]==24,"End command triggers at empty origin before checking occupancy");
   const auto flow=json(base+"computer-scan-flow.json");const auto &reference=flow["steps"].back();
   for(const auto *key:{"side","points","round","carry","status"})check(end[key]==reference[key],std::string("Continuous original handover ")+key);
   check(current["sram"]==Json(read(base+reference["sram"].get<std::string>())),"Full SRAM matches 43-frame original no-action cycle");
   check(current["random_cursor"]==flow["steps"][1]["cursor"],"Exact decision RNG at emitted end command (presentation later advances original clock/RNG)");
   check(current["sram"]==before["sram"],"No artificial battle mutation across no-action cycle");
   check(!session.continue_computer_scan().empty()&&session.save()==current,"Cannot repeat cycle end on player turn");
   check(session.end_tactical_turn().empty()&&session.advance_computer_tactics().empty()&&session.plan_computer_tactics().empty(),"Next computer turn remains playable");
   const auto next=session.save();check(next["battle"]["tactics"]["points"]==24&&next["battle"]["tactics"]["computer_plan"]["previous_slot"]==11&&!next["battle"]["tactics"].contains("computer_scan"),"Next turn has no invented computer carry and starts a fresh scan from retained cursor");
   check(loaded.restore(next).empty()&&loaded.save()==next,"Cross-turn strict replay");
   for(int tick=0;tick<400;++tick){
    const auto save=session.save();const auto &turn=save["battle"]["tactics"];
    if(turn.value("turn_boundary",std::string{})=="time_limit")break;
    std::string error;
    if(!turn.contains("turn_boundary"))error=session.end_tactical_turn();
    else if(!turn.contains("computer")||turn["computer"]["kind"]!="plan")error=session.advance_computer_tactics();
    else if(!turn.contains("computer_plan"))error=session.plan_computer_tactics();
    else if(!turn.contains("computer_strategy"))error=session.evaluate_computer_strategy();
    else if(!turn.contains("computer_motion"))error=session.continue_computer_motion();
    else if(turn["computer_motion"]["kind"]=="pending")error=session.continue_computer_flank();
    else if(turn["computer_motion"]["kind"]=="retry_strategy")error=session.retry_computer_strategy();
    else error=session.continue_computer_scan();
    check(error.empty(),"Repeated full-round progression: "+error);
   }
   const auto timeout=session.save();check(timeout["battle"]["tactics"]["turn_boundary"]=="time_limit"&&timeout["battle"]["tactics"]["round"]==11,"Eleven complete no-command rounds reach existing time-limit settlement boundary");
   check(loaded.restore(timeout).empty()&&loaded.save()==timeout,"Replay complete eleven-round history");
   forged=timeout;forged["battle"]["tactics"]["last_computer_scan"]["command"]=0;check(!loaded.restore(forged).empty()&&loaded.save()==timeout,"Forged completed scan rejected atomically");
  }
 }
 std::cout<<"Original computer scan "<<rows.size()<<" samples and session continuations passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
