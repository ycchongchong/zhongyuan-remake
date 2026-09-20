#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"attacker-ai.json");
 for(const auto &row:rows){
  const auto &s=row["stages"];const auto label=row["id"].dump();
  OriginalState state(rom,read(base+s["plan"]["sram"].get<std::string>()));
  auto plan=state.tactical_ai_attacker_plan(row["target"],row["actual_previous"],row["reuse"]);
  check(!plan.contains("error")&&plan["slot"]==s["strategy"]["slot"]&&plan["enemy_commander"]==s["strategy"]["anchor"],"Original attacker plan "+label+" "+plan.dump());
  check(state.sram()==read(base+s["strategy"]["sram"].get<std::string>()),"Original attacker plan full SRAM "+label);
  std::uint8_t cursor=s["strategy"]["cursor"];
  auto decision=state.tactical_ai_strategy(row["target"],plan["slot"],row["points"],s["strategy"]["statuses"],cursor,false,nullptr,128,row["round"]);
  const bool selected=row["kind"]=="strategy";
  check(!decision.contains("error")&&decision["kind"]==(selected?"strategy":"no_strategy"),"Original attacker strategy "+label+" "+decision.dump());
  if(selected)check(decision["command"]==s["strategy_after"]["command"]&&decision["argument"]==s["strategy_after"]["argument"],"Original attacker strategy target "+label);
  check(state.sram()==read(base+s["strategy_after"]["sram"].get<std::string>())&&cursor==s["strategy_after"]["cursor"],"Original attacker strategy full SRAM/RNG "+label);
  if(!selected){
   auto motion=state.tactical_ai_motion(row["target"],plan["slot"],row["points"],s["motion"]["statuses"],true,nullptr,128,plan["enemy_commander"]);
   check(!motion.contains("error")&&motion["kind"]==row["kind"],"Original attacker motion "+label+" "+motion.dump());
   if(row["kind"]!="scan")check(motion["command"]==s["motion_after"]["command"]&&motion["direction"]==s["motion_after"]["argument"],"Original attacker command "+label+" "+motion.dump());
   check(state.sram()==read(base+s["motion_after"]["sram"].get<std::string>())&&cursor==s["motion_after"]["cursor"],"Original attacker motion full SRAM/RNG "+label);
  }
 }
 {
  const auto opening=json(base+"attacker-opening.json");check(opening["flows"].size()==4&&opening["frames"]==192,"Continuous original opening recorded");
  for(const auto &flow:opening["flows"]){const auto &r=flow["steps"];
   OriginalState state(rom,read(base+r["assessment"]["sram"].get<std::string>()));std::uint8_t cursor=r["assessment"]["cursor"];
   auto assessment=state.tactical_ai_assessment(128,r["assessment"]["round"],r["assessment"]["points"],cursor);
   check(assessment["kind"]=="plan"&&state.sram()==read(base+r["plan"]["sram"].get<std::string>())&&cursor==r["plan"]["cursor"],"Continuous invasion assessment");
   auto plan=state.tactical_ai_attacker_plan(flow["target"],r["plan"]["slot"],r["plan"]["reuse"]);
   check(!plan.contains("error")&&state.sram()==read(base+r["strategy"]["sram"].get<std::string>())&&plan["enemy_commander"]==r["strategy"]["anchor"],"Continuous invasion target selection");
   cursor=r["strategy"]["cursor"];auto strategy=state.tactical_ai_strategy(flow["target"],plan["slot"],r["strategy"]["points"],r["strategy"]["status"],cursor,false,nullptr,128,r["strategy"]["round"]);
   check(strategy["kind"]=="no_strategy"&&cursor==r["motion"]["cursor"]&&state.sram()==read(base+r["motion"]["sram"].get<std::string>()),"Continuous invasion strategy probability");
   auto motion=state.tactical_ai_motion(flow["target"],plan["slot"],r["motion"]["points"],r["motion"]["status"],true,nullptr,128,plan["enemy_commander"]);
   check(motion["kind"]=="move"&&motion["command"]==r["command"]["command"]&&motion["direction"]==r["command"]["argument"]&&state.sram()==read(base+r["move"]["sram"].get<std::string>()),"Continuous invasion selected movement, no SRAM normalization");
   int points=r["move"]["points"];const int directions[]={3,2,1,0};
   check(state.tactical_step(plan["slot"],directions[motion["direction"].get<int>()],points,128).empty()&&points==r["after"]["points"]&&state.sram()==read(base+r["after"]["sram"].get<std::string>()),"Continuous invasion executed movement full SRAM and cost");
  }
 }
 for(const auto &name:{"start","far","strategy"}){
  OriginalSession session(rom),loaded(rom);const auto entry=json(project+"/tests/attacker-invasion-"+name+".json");
  auto replay=[&](){const auto save=session.save();auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Attacking computer strict replay: "+error);};
  check(session.restore(entry).empty(),"Legacy completed defense restores");
  check(!session.advance_computer_attack().empty()&&session.save()==entry,"Cannot skip tactical initialization");
  check(session.begin_tactics().empty(),"Begin original first invading turn");replay();
  const auto initial=session.save();check(initial["battle"]["tactics"]["side"]==128&&initial["battle"]["tactics"]["round"]==1&&initial["battle"]["tactics"]["points"]==24,"Original invasion starts at round1,24 mobility");
  check(!session.begin_tactics().empty()&&!session.end_tactical_turn().empty()&&!session.move_tactical(0,0).empty()&&session.save()==initial,"Human cannot reset or override first computer turn");
  bool complete=false;int effects=0,moves=0,scans=0;
  for(int i=0;i<120;++i){
   const auto before=session.save();const auto stage=before["battle"]["tactics"].value("attacker_ai",Json::object()).value("stage",std::string("assess"));
   check(session.advance_computer_attack().empty(),"Advance original invasion action "+stage);replay();
   const auto save=session.save();const auto &t=save["battle"]["tactics"];const auto &ai=t["attacker_ai"];
   if(stage=="strategy_execute"){
    ++effects;check(ai["stage"]=="strategy_result"&&ai["result"]["points_before"].get<int>()-ai["result"]["points_after"].get<int>()==rom.tactical_strategy_tables()["cost"][ai["strategy"]["strategy"].get<int>()].get<int>(),"Computer strategy charges original cost");
   }
   if(stage=="strategy_result")check(save["sram"]==before["sram"]&&save["random_cursor"]==before["random_cursor"],"Computer result acknowledgement does not reapply effect");
   if(stage=="motion"&&ai.contains("motion")&&ai["motion"]["kind"]=="move")++moves;
   if(stage=="scan")++scans;
   auto forged=save;forged["battle"]["tactics"]["attacker_ai"]["stage"]="invented";
   check(!loaded.restore(forged).empty()&&loaded.save()==save,"Forged invading AI state rejected atomically");
   if(t.contains("attack")){
    check(std::string(name)!="far"&&t["attack"]["computer"]==true&&t["attack"]["stage"]=="clash_ready","Invading computer enters real clash preparation");
    check(!session.advance_computer_attack().empty()&&session.save()==save,"Pending attack prevents new AI decisions");
    check(session.begin_clash().empty(),"Invading attack initializes original combat units");replay();
    const auto clash=session.save()["battle"]["tactics"]["attack"]["clash"];int human=-1;
    for(int side=0;side<2;++side)if(clash["players"][side]!=0)human=side;
    check(human>=0&&session.cycle_clash_order(human,3).empty(),"Human defender can issue combat orders");replay();
    const auto locked=session.save();check(!session.cycle_clash_order(human^1,3).empty()&&session.save()==locked,"Human cannot alter invading AI combat orders");
    check(session.advance_clash().empty(),"Invading combat advances native unit actions");replay();complete=true;break;
   }
   if(!t.contains("turn_boundary")){
    check(t["side"]==0&&t["points"]==15&&t["round"]==1,"Computer completion hands15 mobility to defender without an extra round");
    check(!session.advance_computer_attack().empty()&&session.save()==save,"Computer cannot act during human defense");
    bool moved=false;for(int direction=0;direction<4&&!moved;++direction)moved=session.move_tactical(t["selected"],direction).empty();check(moved,"Human defender can move after invasion");replay();
    check(session.scout_tactical(session.save()["battle"]["tactics"]["selected"],0).empty()&&session.close_tactical_scout().empty(),"Human defense scouts invading troops");replay();
    check(session.end_tactical_turn().empty(),"Human defense hands back to invading computer");replay();
    check(session.save()["battle"]["tactics"]["round"]==2&&session.advance_computer_attack().empty(),"Next invading round resumes");replay();complete=true;break;
   }
  }
  check(complete&&moves>0,"Invading session reaches attack or human control");
  if(std::string(name)=="strategy")check(effects>0,"Strategy fixture executes at least one original strategy");
 }
 std::cout<<"Original attacker AI "<<rows.size()<<" records passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
