#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string&m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string&p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string&p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string p=ZHONGYUAN_PROJECT_DIR,base=p+"/reference/fixtures/";int moved=0,blocked=0,waiting=0;
 const auto rows=json(base+"computer-empty-role.json");
 for(const auto&r:rows){
  const auto steps=r["steps"];OriginalState state(rom,read(base+steps[1]["sram"].get<std::string>()));const auto before=state.sram();
  const auto result=state.tactical_empty_role_return(r["argument"],r["points"],steps[0]["statuses"]);
  check(!result.contains("error")&&result["points_after"]==steps.back()["points"]&&state.sram()==read(base+steps.back()["sram"].get<std::string>()),"Original full SRAM and points "+r["id"].dump()+" "+result.dump());
  check(state.sram()[0xdaa]==255,"Empty command cannot resurrect officer");
  if(r["status"]!=0){++waiting;check(result["kind"]=="waiting"&&steps.back()["phase"]==8&&state.sram()==before,"Original status wait preserved");}
  else {check(steps.back()["phase"]==5&&steps.back()["counter"]==1,"Empty command returns to assessment");if(result["kind"]=="move")++moved;else ++blocked;}
  for(int i=0;i<256;++i)check(state.sram()[0xe1a+i]==before[0xe1a+i],"BA32 returns before painting a nonexistent unit");
 }
 check(moved==37&&blocked==135&&waiting==144,"All316 empty-slot original branches covered");
 for(int argument=0;argument<4;++argument){
  const auto flow=json(base+"empty-role-flow-"+std::to_string(argument)+".json");const auto steps=flow["steps"];
  OriginalState exact(rom,read(base+steps[0]["sram"].get<std::string>()));std::uint8_t cursor=steps[0]["cursor"];
  check(exact.tactical_ai_assessment(0,steps[0]["round"],steps[0]["points"],cursor)["role_slot"]==5&&exact.sram()==read(base+steps[1]["sram"].get<std::string>()),"Continuous original role assessment");
  const auto result=exact.tactical_empty_role_return(argument,steps[1]["points"],steps[1]["statuses"]);
  check(result["kind"]=="blocked"&&exact.sram()==read(base+steps.back()["sram"].get<std::string>())&&result["points_after"]==steps.back()["points"],"All four inherited directions match continuous original SRAM");
  check(flow["argument_writes"].empty()&&steps[steps.size()-2]["cursor"]==steps.back()["cursor"]&&steps.back()["phase"]==5,"Original empty command preserves direction and logic RNG then returns to assessment");
 }
 auto raw=read(base+rows[0]["steps"][1]["sram"].get<std::string>());auto status=Json(std::vector<int>(24,0));
 OriginalState state(rom,raw);check(state.tactical_empty_role_return(-1,40,status).contains("error")&&state.sram()==raw,"Unknown direction with distinct raw movements holds atomically");
 raw[0xc36]=255;OriginalState blocked_state(rom,raw);auto r=blocked_state.tactical_empty_role_return(-1,40,status);
 check(r["kind"]=="blocked"&&r["direction"]==-1&&blocked_state.sram()==raw,"Unknown direction may resume only unanimous blocked outcomes");
 for(int bad:{-2,256})check(blocked_state.tactical_empty_role_return(bad,24,status).contains("error")&&blocked_state.sram()==raw,"Invalid direction rejected");
 for(int bad:{0,41})check(blocked_state.tactical_empty_role_return(0,bad,status).contains("error")&&blocked_state.sram()==raw,"Invalid budget rejected");
 status[0]=std::uint64_t(1)<<32;check(blocked_state.tactical_empty_role_return(0,24,status).contains("error")&&blocked_state.sram()==raw,"Oversized status rejected before narrowing");status[0]=0;
 for(int id:{0,130,241,254}){auto occupied=raw;occupied[0xdaa]=id;OriginalState bad(rom,occupied);check(bad.tactical_empty_role_return(0,24,status).contains("error")&&bad.sram()==occupied,"Dedicated API cannot bypass normal officer validation");}
 OriginalSession game(rom),loaded(rom);const auto entry=json(p+"/tests/computer-empty-role.json");check(game.restore(entry).empty()&&game.save()==entry,"Real empty-slot retreat history restores unchanged");
 check(!game.continue_computer_role().empty()&&game.save()==entry,"Legacy event still preserves unsupported empty slot");
 check(game.can_continue_computer_role()&&game.save()==entry,"New availability check is read only");
 auto e=game.continue_empty_computer_role();check(e.empty(),e);auto after=game.save();const auto &t=after["battle"]["tactics"];
 check(after["sram"]==entry["sram"]&&after["random_cursor"]==entry["random_cursor"]&&t["points"]==entry["battle"]["tactics"]["points"]&&t["computer_motion"]["kind"]=="blocked"&&t["computer_motion"]["direction"]==-1,"Actual empty-slot continuation preserves SRAM,points and RNG without inventing direction");
 check(t["selected"]==0&&t.value("computer_cursor",Json(nullptr))==entry["battle"]["tactics"].value("computer_cursor",Json(nullptr))&&t["moves"].back()["kind"]=="computer_empty_role_return","Stale slot zero does not alter AI scan cursor");
 check(loaded.restore(after).empty()&&loaded.save()==after,"New event strictly replays");
 check(!game.continue_empty_computer_role().empty()&&!game.can_continue_computer_role()&&game.save()==after,"Cannot repeat empty-slot command");
 for(int i=0;i<4;++i){auto forged=after;
  if(i==0)forged["sram"][0xdaa]=130;
  if(i==1)forged["battle"]["tactics"]["computer_motion"]["argument"]=2;
  if(i==2)forged["battle"]["tactics"]["moves"].back()["kind"]="computer_role_return";
  if(i==3)forged["battle"]["tactics"]["moves"].push_back(forged["battle"]["tactics"]["moves"].back());
  check(!loaded.restore(forged).empty()&&loaded.save()==after,"Forged or repeated empty-slot result rejected atomically");
 }
 check(game.advance_computer_tactics().empty(),"Next assessment resumes after empty-slot command");
 check(game.plan_computer_tactics().empty(),"Surviving unit planning resumes");
 check(game.save()["battle"]["tactics"]["computer_plan"]["slot"]!=0,"Scan skips departed first defender");
 check(loaded.restore(game.save()).empty()&&loaded.save()==game.save(),"Full continuation remains replayable");
 std::cout<<"Original empty role:316 flows,37 position changes,135 blocked,144 waits;real retreat/role/scan continuation and strict replay passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
