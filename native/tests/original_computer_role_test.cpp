#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string&m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string&p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string&p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"computer-role.json");int moved=0,blocked=0,waiting=0,held=0;
 for(const auto &r:rows){
  const auto steps=r["steps"];OriginalState state(rom,read(base+steps[0]["sram"].get<std::string>()));std::uint8_t cursor=steps[0]["cursor"];
  const auto assessment=state.tactical_ai_assessment(0,0,r["points"],cursor);
  check(assessment["kind"]=="plan"&&assessment["role_slot"]==3&&cursor==steps[1]["cursor"]&&state.sram()==read(base+steps[1]["sram"].get<std::string>()),"Exact original role update "+r["id"].dump());
  check(steps[2]["ai_status"]==255&&steps[2]["command"]==0&&steps[2]["argument"]==r["argument"]&&steps[2]["scan_slot"]==7,"Original return retains stale direction and scan cursor");
  const auto before=state.sram();const auto result=state.tactical_role_return(r["argument"],r["points"],steps[0]["statuses"]);
  if(r["absent"]){check(result.contains("error")&&state.sram()==before,"Absent slot stays explicitly held");++held;continue;}
  check(!result.contains("error")&&result["points_after"]==steps.back()["points"]&&state.sram()==read(base+steps.back()["sram"].get<std::string>()),"Original complete SRAM and movement cost "+r["id"].dump()+" "+result.dump());
  if(r["status"]!=0){check(result["kind"]=="waiting"&&steps.back()["phase"]==8&&steps.back()["counter"]==1,"Status lock preserves original movement wait");++waiting;continue;}
  check(steps.back()["phase"]==5&&steps.back()["counter"]==1&&steps.back()["scan_slot"]==7,"Rejected or accepted command returns for assessment without changing scan slot");
  result["kind"]=="move"?++moved:++blocked;
 }
 const auto flow=json(base+"computer-role-flow.json");const auto steps=flow["steps"];
 check(flow["argument_writes"].empty()&&flow["frames"]==129,"Continuous original retreat preserves the movement argument");
 OriginalState continuous(rom,read(base+steps[0]["sram"].get<std::string>()));std::uint8_t rng=steps[0]["cursor"];
 const auto retreat=continuous.tactical_ai_assessment(0,0,steps[0]["points"],rng);
 check(retreat["kind"]=="retreat"&&retreat["slot"]==4&&rng==steps[1]["cursor"],"Original weak-unit retreat decision");
 check(continuous.retreat_defender(4,12,false,rng)["departed"]==true&&continuous.sram()==read(base+steps[2]["sram"].get<std::string>()),"All SRAM after actual retreat matches continuous original");
 // Presentation consumes RNG between decisions; feed the observed second
 // decision cursor rather than claiming full frame/RNG scheduling parity.
 rng=steps[2]["cursor"];
 check(continuous.tactical_ai_assessment(0,0,steps[2]["points"],rng)["role_slot"]==5&&continuous.sram()==read(base+steps[3]["sram"].get<std::string>()),"Next assessment promotes slot five");
 const auto action=continuous.tactical_role_return(steps[3]["argument"],steps[3]["points"],steps[3]["statuses"]);
 check(action["kind"]=="move"&&action["points_after"]==steps.back()["points"]&&continuous.sram()==read(base+steps.back()["sram"].get<std::string>()),"All8192 bytes through inherited movement, without normalization");
 const auto entry=json(project+"/tests/computer-role-start.json");OriginalSession session(rom),loaded(rom);
 check(session.restore(entry).empty()&&session.save()==entry,"Legal movement/retreat/role history loads unchanged");
 check(!session.plan_computer_tactics().empty()&&session.save()==entry,"Legacy planning does not skip the return");
 check(session.can_continue_computer_role()&&session.save()==entry,"Availability query is read only");
 const auto error=session.continue_computer_role();check(error.empty(),"Role continuation: "+error);const auto after=session.save();const auto &t=after["battle"]["tactics"];
 check(after["sram"]==continuous.sram(),"PC resumed movement matches continuous original SRAM");
 check(t["computer_motion"]["argument"]==2&&t["computer_motion"]["kind"]=="move"&&t["points"]==26&&after["sram"][0xdab]==83,"Inherited left movement charges three points");
 check(t["computer_cursor"]==5&&t["selected"]==0&&after["random_cursor"]==entry["random_cursor"],"Command selects zero but preserves scan slot and decision RNG");
 check(loaded.restore(after).empty()&&loaded.save()==after,"Resume event strict replay");
 check(!session.can_continue_computer_role()&&!session.continue_computer_role().empty()&&session.save()==after,"No duplicate role action");
 auto forged=after;forged["battle"]["tactics"]["computer_motion"]["argument"]=1;check(!loaded.restore(forged).empty()&&loaded.save()==after,"Forged residual direction rejected atomically");
 check(session.advance_computer_tactics().empty()&&session.plan_computer_tactics().empty(),"Next decision proceeds after role return");
 check(session.save()["battle"]["tactics"]["computer_plan"]["slot"]==3,"Next scan starts from five and skips departed fourth slot");
 check(loaded.restore(session.save()).empty()&&loaded.save()==session.save(),"Further scan and movement replay");
 const auto blocked_entry=json(project+"/tests/computer-role-blocked.json");
 check(loaded.restore(blocked_entry).empty()&&loaded.can_continue_computer_role(),"Load legal inherited-right blocked movement");
 check(loaded.continue_computer_role().empty(),"Blocked command returns for another assessment");const auto blocked_after=loaded.save();
 check(blocked_after["sram"]==blocked_entry["sram"]&&blocked_after["battle"]["tactics"]["points"]==29&&blocked_after["battle"]["tactics"]["computer_motion"]["kind"]=="blocked"&&blocked_after["battle"]["tactics"]["computer_motion"]["argument"]==3,"Blocked right move consumes no points and preserves every SRAM byte");
 const auto blocked_flow=json(base+"computer-role-flow-blocked.json");
 check(blocked_flow["argument_writes"].empty()&&blocked_after["sram"]==read(base+blocked_flow["steps"].back()["sram"].get<std::string>())&&blocked_flow["steps"].back()["phase"]==5,"Continuous original confirms blocked command returns to assessment");
 check(session.restore(blocked_after).empty()&&session.save()==blocked_after,"Blocked-command event replays");
 check(loaded.advance_computer_tactics().empty()&&loaded.plan_computer_tactics().empty()&&loaded.save()["battle"]["tactics"]["computer_plan"]["slot"]==3,"Blocked command does not repeat the role update or reset the scan");
 const auto unknown=json(project+"/tests/computer-role-unknown.json");check(loaded.restore(unknown).empty(),"Load old unknown-argument boundary");
 check(!loaded.can_continue_computer_role()&&!loaded.continue_computer_role().empty()&&loaded.save()==unknown,"No stale direction leaks between loaded histories");
 auto raw=read(base+rows[0]["steps"][1]["sram"].get<std::string>());OriginalState state(rom,raw);auto status=Json(std::vector<int>(24,0));
 check(state.tactical_role_return(-1,24,status).contains("error")&&state.sram()==raw,"Unknown direction with different outcomes is atomic");
 auto same=state.tactical_role_return(-1,1,status);check(same["kind"]=="blocked"&&same["direction"]==-1&&same["points_after"]==1&&state.sram()==raw,"Equivalent blocked outcomes need no invented direction");
 status[0]=256;check(state.tactical_role_return(0,24,status).contains("error")&&state.sram()==raw,"Invalid status is atomic");status[0]=0;
 for(int arg:{-2,256})check(state.tactical_role_return(arg,24,status).contains("error")&&state.sram()==raw,"Invalid residual byte rejected");
 for(int points:{0,41})check(state.tactical_role_return(0,points,status).contains("error")&&state.sram()==raw,"Invalid budget rejected");
 for(const std::string suffix:{"","-next"}){
  const int expected_argument=suffix.empty()?1:2;
  const auto natural=json(project+"/tests/natural-role-handover"+suffix+".json");
  check(loaded.restore(natural).empty()&&loaded.save()==natural,"Natural eight-battle history restores at role handover");
  check(!loaded.continue_computer_role().empty()&&loaded.save()==natural,"Legacy unknown-argument event retains its meaning");
  check(loaded.can_continue_computer_role()&&loaded.save()==natural,"Verified handover recovery is available without mutation");
  check(loaded.resume_computer_role_after_handover().empty(),"Recover direction from prior exhausted attack across archived histories");
  const auto recovered=loaded.save();const auto &motion=recovered["battle"]["tactics"]["computer_motion"];
  check(motion["argument"]==expected_argument&&recovered["random_cursor"]==natural["random_cursor"],"Recovered direction has no invented RNG draw");
  const auto reference=json(base+"natural-role-handover"+suffix+".json")["cases"][expected_argument];
  check(recovered["sram"]==read(base+reference["after"].get<std::string>())&&recovered["battle"]["tactics"]["points"]==reference["points_after"],"Recovered action matches all original SRAM bytes and mobility");
  check(session.restore(recovered).empty()&&session.save()==recovered,"New recovery event strictly replays with all archived segments");
  check(!loaded.resume_computer_role_after_handover().empty()&&loaded.save()==recovered,"Recovery cannot be applied twice");
  auto forged=recovered;forged["battle"]["tactics"]["computer_motion"]["argument"]=0;
  check(!session.restore(forged).empty()&&session.save()==recovered,"Forged recovered direction rejected atomically");
  check(loaded.restore(unknown).empty()&&!loaded.resume_computer_role_after_handover().empty()&&loaded.save()==unknown,"Unknown history cannot borrow a direction from another save");
 }
 std::cout<<"Original computer role "<<rows.size()<<" flows: "<<moved<<" moved, "<<blocked<<" blocked, "<<waiting<<" status waits, "<<held<<" absent-slot boundaries held\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
