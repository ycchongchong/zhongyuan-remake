#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string&m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string&p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string&p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";int count=0;
 for(const std::string kind:{"motion","nearby","strategy"})for(const auto &r:json(base+"computer-scratch-"+kind+".json")){
  const auto raw=read(base+r["before"].get<std::string>());OriginalState state(rom,raw),legacy(rom,raw);std::uint8_t cursor=r["cursor_before"],oldcursor=cursor;int scratch=r["scratch_before"];
  const auto result=kind=="strategy"?state.tactical_ai_strategy(r["target"],r["slot"],r["points"],r["statuses"],cursor,false,&scratch):state.tactical_ai_motion(r["target"],r["slot"],r["points"],r["statuses"],kind=="nearby",&scratch);
  const auto old=kind=="strategy"?legacy.tactical_ai_strategy(r["target"],r["slot"],r["points"],r["statuses"],oldcursor):legacy.tactical_ai_motion(r["target"],r["slot"],r["points"],r["statuses"],kind=="nearby");
  const auto label=kind+" "+r["id"].dump();
  check(scratch==r["scratch_after"],"Original $21 "+label+" expected "+r["scratch_after"].dump()+" got "+std::to_string(scratch)+" decision "+result.dump());
  check(result==old&&state.sram()==legacy.sram()&&cursor==oldcursor,"Tracking does not change historical result "+label);
  check(state.sram()==read(base+r["after"].get<std::string>())&&cursor==r["cursor_after"],"Full SRAM and decision RNG "+label);++count;
 }

 const auto flows=json(base+"computer-scratch-flow.json");check(flows.size()==2,"Two original scan-and-resume flows");
 for(const auto &flow:flows){
  const auto steps=flow["steps"];OriginalState state(rom,read(base+steps[0]["sram"].get<std::string>()));std::uint8_t cursor=steps[0]["cursor"];int points=steps[0]["points"],scratch=-1;
  const auto assessment=state.tactical_ai_assessment(0,steps[0]["round"],points,cursor);check(assessment["kind"]=="plan"&&assessment["score"].get<int>()>=0,"Original strength assessment executes");
  for(int i=10;i>=0;--i){const int id=state.sram()[0xdc2+3*i];if(id<241){scratch=(0x6438+8*id)>>8;break;}}
  auto plan=state.tactical_ai_fort(flow["target"],steps[0]["slot"],points,steps[0]["statuses"]);
  for(int j:{1,3}){
   const auto &before=steps[j],after=steps[j+1];
   check(plan["kind"]=="role_plan"&&plan["slot"]==before["slot"]&&scratch==before["scratch21"]&&cursor==before["cursor"]&&state.sram()==read(base+before["sram"].get<std::string>()),"Original inherited scratch and full state at occupied-fort entry");
   check(state.tactical_ai_strategy(flow["target"],plan["slot"],points,before["statuses"],cursor,false,&scratch)["kind"]=="role_reassignment","Fort diversion preserves scratch");
   const auto result=state.tactical_ai_occupied_fort(flow["target"],plan["slot"],points,before["statuses"],cursor,scratch);scratch=result["scratch21"];
   check(result["kind"]==after["stage"]&&scratch==after["scratch21"]&&cursor==after["cursor"]&&state.sram()==read(base+after["sram"].get<std::string>()),"Original no-command/move decision matches full state and RNG");
   if(result["kind"]=="scan"){
    const auto scan=state.tactical_ai_scan(0,plan["slot"],before["marker"]);check(scan["kind"]=="select"&&scan["slot"]==steps[j+2]["slot"],"Scan skips empty slots and selects next unit");
    plan=state.tactical_ai_fort(flow["target"],scan["slot"],points,before["statuses"],true);
   }else{
    const int directions[]={3,2,1,0};check(state.tactical_step(plan["slot"],directions[result["direction"].get<int>()],points,0).empty(),"Execute recovered original move");
    check(points==steps.back()["points"]&&state.sram()==read(base+steps.back()["sram"].get<std::string>()),"All8192 bytes through continuous original movement, no normalization");
   }
  }
  const std::string variant=flow["variant"];const auto entry=json(project+"/tests/computer-scratch-resume-"+variant+".json");OriginalSession session(rom),loaded(rom);
  check(session.restore(entry).empty()&&session.save()==entry,"Load old legal history with unresolved scratch");
  check(!session.continue_computer_occupied_fort().empty()&&session.save()==entry,"Old API remains atomic and retains legacy event semantics");
  check(session.continue_computer_occupied_fort(true).empty(),"Recovered API resumes previously blocked unit");const auto after=session.save();
  check(after["sram"]==state.sram()&&after["random_cursor"]==steps[4]["cursor"]&&after["battle"]["tactics"]["points"]==0,"Session move matches original and charges once");
  check(after["battle"]["tactics"]["moves"].back()["kind"]=="computer_occupied_fort_resume"&&after["battle"]["tactics"]["computer_motion"]["scratch21"]==105,"Separate event stores derived report");
  check(loaded.restore(after).empty()&&loaded.save()==after,"New resume event replays exactly");
  check(loaded.restore(entry).empty()&&loaded.continue_computer_occupied_fort(true).empty()&&loaded.save()==after,"Loading old history reconstructs same future continuation");
  check(!session.continue_computer_occupied_fort(true).empty()&&session.save()==after,"Cannot duplicate resumed move");
  auto forged=after;forged["battle"]["tactics"]["computer_motion"]["scratch21"]=0;check(!loaded.restore(forged).empty()&&loaded.save()==after,"Cannot forge inherited scratch report");
  for(const auto *key:{"cursor","frame"}){forged=after;forged["battle"]["tactics"]["moves"].back()[key]=256;check(!loaded.restore(forged).empty()&&loaded.save()==after,"Reject invalid resume event byte");}
  check(session.advance_computer_tactics().empty(),"Exhausted movement hands over to player");const auto handover=session.save();
  check(!handover["battle"]["tactics"].contains("turn_boundary")&&handover["battle"]["tactics"]["side"]==128&&loaded.restore(handover).empty()&&loaded.save()==handover,"Player turn and save after recovered move");
  // Loading a new one-city context must discard tracker data from the
  // previous save. This controlled ownership change skips AD03 entirely.
  auto unknown=json(project+"/tests/computer-occupied-fort-move.json");
  for(int c=0;c<30;++c)if(c!=12&&(unknown["sram"][c*36].get<int>()&7)==2){
   for(auto *raw:{&unknown["sram"],&unknown["battle"]["tactics"]["deployment_sram"]})(*raw)[c*36]=((*raw)[c*36].get<int>()&248)|4;
  }
  unknown["battle"]["tactics"]["computer"]["score"]=-1;
  check(loaded.restore(unknown).empty(),"Load controlled skipped-assessment context");
  check(!loaded.continue_computer_occupied_fort(true).empty()&&loaded.save()==unknown,"No stale scratch leak or invented byte after restore");
 }
 {
  const auto row=json(base+"computer-scratch-nearby.json")[0];const auto raw=read(base+row["before"].get<std::string>());OriginalState state(rom,raw);int scratch=73;std::uint8_t cursor=17;auto status=row["statuses"];status[0]=256;
  check(state.tactical_ai_motion(row["target"],row["slot"],row["points"],status,true,&scratch).contains("error")&&scratch==73,"Invalid motion preserves scratch");
  check(state.tactical_ai_strategy(row["target"],row["slot"],row["points"],status,cursor,false,&scratch).contains("error")&&scratch==73&&cursor==17&&state.sram()==raw,"Invalid strategy preserves SRAM, RNG and scratch");
 }
 std::cout<<"Original computer scratch "<<count<<" samples passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
