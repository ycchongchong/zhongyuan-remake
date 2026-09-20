#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"computer-flank.json");
 for(const auto &row:rows){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));const auto result=state.tactical_ai_flank(row["target"],row["slot"],row["points"],row["statuses"]);
  check(!result.contains("error")&&result["kind"]==row["kind"]&&result["direction"]==row["direction"]&&result["repeat_unit"]==row["repeat_unit"]&&result["cached_target"]==row["cached_target"],"Original flank "+row["id"].dump()+" expected "+row["kind"].dump()+" got "+result.dump());
  check(state.sram()==read(base+row["after"].get<std::string>())&&row["cursor_before"]==row["cursor_after"],"Full SRAM and unchanged RNG "+row["id"].dump());
 }

 for(const auto &row:json(base+"computer-reuse.json")){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));auto status=Json(std::vector<int>(24,0));status[row["slot"].get<int>()]=row["status"];
  const auto result=state.tactical_ai_fort(row["target"],row["previous"],row["points"],status,true);
  check(!result.contains("error")&&result["kind"]==row["kind"]&&result["slot"]==row["selected"]&&result["direction"]==row["direction"]&&result["reuse_unit"]==true,"Original negative-A0 same-unit selection "+row["id"].dump()+" "+result.dump());
  check(state.sram()==read(base+row["after"].get<std::string>())&&row["cursor_before"]==row["cursor_after"],"Original reuse full SRAM and no RNG");
 }
 for(const auto &row:json(base+"computer-retry.json")){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));std::uint8_t cursor=row["cursor_before"];
  const auto result=state.tactical_ai_strategy(row["target"],row["slot"],row["points"],row["statuses"],cursor,true);
  check(!result.contains("error")&&result["kind"]==row["kind"]&&result["command"]==row["command"]&&result["argument"]==row["argument"]&&result["threshold"]==-1,"Original direct B61D retry "+row["id"].dump()+" "+result.dump());
  check(state.sram()==read(base+row["after"].get<std::string>())&&cursor==row["cursor_after"],"Original retry full SRAM and exact RNG");
 }

 {
  const auto flow=json(base+"computer-flank-flow.json");const auto &steps=flow["steps"];OriginalState state(rom,read(base+steps[0]["before"].get<std::string>()));
  check(flow["reuse"].size()>=2,"Original negative-A0 resumes same unit twice");
  for(const auto &reuse:flow["reuse"])check(reuse["slot"]==flow["slot"]&&reuse["marker"]==254,"Original reuse slot and literal marker");
  for(const auto &step:steps){
   check(state.sram()==read(base+step["before"].get<std::string>()),"No omitted SRAM mutation between original flank steps");
   const auto result=state.tactical_ai_flank(flow["target"],flow["slot"],step["points"],step["statuses"]);
   check(result["kind"]==step["kind"]&&result["direction"]==step["direction"]&&result["repeat_unit"]==step["repeat_unit"],"Continuous original flank decision");
   int points=step["points"];
   if(result["kind"]=="move"){
    check(state.sram()==read(base+step["decision"].get<std::string>()),"Original cached destination write before movement");
    const int directions[]={3,2,1,0};check(state.tactical_step(flow["slot"],directions[result["direction"].get<int>()],points,0).empty(),"Execute original flank step");
   }
   check(points==step["points_after"]&&state.sram()==read(base+step["after"].get<std::string>()),"Continuous original full SRAM and mobility");
  }
 }
 {
  const auto raw=read(base+rows[0]["before"].get<std::string>());OriginalState state(rom,raw);auto status=Json(std::vector<int>(24,0));
  check(state.tactical_ai_flank(30,11,40,status).contains("error")&&state.tactical_ai_flank(12,12,40,status).contains("error")&&state.sram()==raw,"Invalid flank parameters are atomic");
  status[0]=256;check(state.tactical_ai_flank(12,11,40,status).contains("error")&&state.sram()==raw,"Invalid statuses cannot clear a cache");
  auto broken=raw;broken[0xdec+22]=0;OriginalState invalid(rom,broken);status[0]=0;check(invalid.tactical_ai_flank(12,11,40,status).contains("error")&&invalid.sram()==broken,"Pursuit cannot enter the flank continuation");
 }
 for(const std::string name:{"move","retry","retry_selected","scan"}){
  OriginalSession session(rom),loaded(rom);const auto entry=json(project+"/tests/computer-flank-"+name+".json");
  check(session.restore(entry).empty()&&session.save()==entry,"Restore legal pre-flank history "+name);
  auto replay=[&](){const auto save=session.save();const auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Strict flank replay: "+error);};
  check(!session.retry_computer_strategy().empty()&&!session.plan_computer_tactics(true).empty()&&session.save()==entry,"Cannot retry or reuse before original branch requires it");
  check(session.continue_computer_flank().empty(),"Continue original flank boundary");replay();const auto action=session.save();
  check(action["random_cursor"]==entry["random_cursor"]&&!session.continue_computer_flank().empty()&&session.save()==action,"Flank is once-only and consumes no RNG");
  auto forged=action;forged["battle"]["tactics"]["computer_motion"]["cached_target"]=0;check(!loaded.restore(forged).empty()&&loaded.save()==action,"Forged cache report rejected atomically");
  if(name=="move"){
   check(action["battle"]["tactics"]["computer_reuse"]==3&&action["sram"][0xdab+6]==0x43&&action["sram"][0xded+6]==0x53&&action["battle"]["tactics"]["points"]==18,"First flank step caches goal and remembers same unit");
   forged=action;forged["battle"]["tactics"]["computer_reuse"]=2;check(!loaded.restore(forged).empty(),"Forged reuse marker rejected");
   for(int step=0;step<2;++step){
    check(session.advance_computer_tactics().empty(),"Assess after successful flank move");replay();const auto assessed=session.save();
    check(!session.plan_computer_tactics().empty()&&session.save()==assessed,"Descending scan cannot replace required same-unit reuse");
    check(session.plan_computer_tactics(true).empty(),"Reuse same unit");replay();
    check(session.save()["battle"]["tactics"]["computer_plan"]["slot"]==3&&!session.save()["battle"]["tactics"].contains("computer_reuse"),"A0 is cleared at reused unit entry");
    check(session.evaluate_computer_strategy().empty()&&session.continue_computer_motion().empty(),"Reused unit resumes original strategy and cached-target entry");replay();
    check(session.continue_computer_flank().empty(),"Continue cached goal");replay();
    if(step==0)check(session.save()["sram"][0xdab+6]==0x53&&session.save()["battle"]["tactics"]["computer_reuse"]==3,"Second step reaches cached fort neighbor");
    else check(session.save()["sram"][0xded+6]==255&&session.save()["battle"]["tactics"]["computer_motion"]["kind"]=="scan","Reached target clears cache before next-unit scan");
   }
  }else if(name=="scan"){
   check(action["battle"]["tactics"]["computer_motion"]["kind"]=="scan"&&action["sram"]==entry["sram"]&&!session.advance_computer_tactics().empty()&&!session.retry_computer_strategy().empty(),"No enemy ray preserves explicit unfinished scan, not a fake end-turn");
  }else{
   check(action["battle"]["tactics"]["computer_motion"]["kind"]=="retry_strategy"&&session.retry_computer_strategy().empty(),"Original blocked flank triggers exactly one direct retry");replay();const auto retry=session.save();
   check(!session.retry_computer_strategy().empty()&&session.save()==retry,"Cannot reroll direct retry");
   forged=retry;forged["battle"]["tactics"]["moves"].back()["cursor"]=256;check(!loaded.restore(forged).empty(),"Invalid retry RNG cursor rejected");
   if(name=="retry_selected"){
    check(retry["battle"]["tactics"]["computer_strategy"]["kind"]=="strategy"&&!retry["battle"]["tactics"].contains("computer_motion"),"Retry selection reaches existing strategy execution");
    check(session.execute_computer_strategy().empty(),"Execute strategy selected by retry");replay();
    check(session.finish_computer_strategy().empty(),"Finish retry strategy result");replay();
   }else check(retry["battle"]["tactics"]["computer_motion"]["kind"]=="scan","Unsuccessful retry reaches scan boundary without repeated probability gate");
  }
 }
 std::cout<<"Original computer flank "<<rows.size()<<" samples passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
