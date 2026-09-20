#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string&m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string&p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string&p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto groups=rom.player_tactical_strategy_tables()["groups"];
 for(const auto &r:json(base+"player-strategy-menu.json")){
  const auto raw=read(base+r["sram"].get<std::string>());OriginalState state(rom,raw);const auto result=state.player_tactical_strategies(r["side"],r["slot"]);Json choices=Json::array();
  for(const auto &v:result["choices"])if(v["page"]==r["page"])choices.push_back(v["strategy"]);
  check(result["intelligence"]==r["intelligence"]&&choices==groups[r["group"].get<int>()]&&state.sram()==raw,"Original player menu "+r["intelligence"].dump()+" page "+r["page"].dump());
 }
 for(const auto &r:json(base+"player-strategy-gate.json")){
  auto raw=read(base+r["sram"].get<std::string>());int id=raw[r["side"]==128?0xdc2:0xdaa];raw[0x438+id*8+2]=100;OriginalState state(rom,raw);
  const auto quote=state.quote_tactical_strategy(r["side"],0,0,r["strategy"],40);
  check((!quote.contains("error"))==r["allowed"]&&state.sram()==raw,"Original shared target terrain "+r["id"].dump());
  if(r["allowed"]){int cost=quote["cost"];check(state.quote_tactical_strategy(r["side"],0,0,r["strategy"],cost-1).contains("error")&&!state.quote_tactical_strategy(r["side"],0,0,r["strategy"],cost).contains("error"),"Exact cost boundary");}
 }
 for(const auto &r:json(base+"player-strategy-execution.json")){
  OriginalState state(rom,read(base+r["before"].get<std::string>()));const auto quote=state.quote_tactical_strategy(r["side"],r["slot"],r["target_slot"],r["strategy"],r["points_before"]);check(!quote.contains("error"),"Human confirmed target is eligible");
  std::uint8_t cursor=r["cursor_before"];const auto result=state.resolve_tactical_strategy(r["city"],r["side"],r["slot"],r["target_slot"],r["strategy"],r["frame"],r["status_before"],cursor);
  check(!result.contains("error")&&result["success"]==r["success"]&&result["status"]==r["status_after"]&&cursor==r["cursor_after"]&&state.sram()==read(base+r["after"].get<std::string>()),"Full original human effect SRAM/RNG/status "+r["id"].dump());
  check(r["points_before"].get<int>()-quote["cost"].get<int>()==r["points_after"],"Original confirmation charges once");
 }
 for(bool dual:{false,true}){
  OriginalSession session(rom),loaded(rom);const auto entry=json(project+"/tests/tactical-turn-"+(dual?"start":"single")+".json");check(session.restore(entry).empty(),"Load canonical deployment history");
  if(dual)check(session.end_tactical_turn().empty(),"Second human takes defending side");
  auto replay=[&](){auto s=session.save();const auto error=loaded.restore(s);check(error.empty()&&loaded.save()==s,"Human strategy strict replay: "+error);};
  const auto before=session.save();const int side=dual?0:128;OriginalState board(rom,before["sram"].get<std::vector<std::uint8_t>>());Json proposal;
  const auto options=session.player_tactical_strategies(0);
  for(const auto &v:options["choices"])for(int target=0;target<(side?12:11);++target){auto q=board.quote_tactical_strategy(side,0,target,v["strategy"],before["battle"]["tactics"]["points"]);if(!q.contains("error")&&proposal.is_null())proposal=q;}
  check(!proposal.is_null(),"A legal strategy exists for either human army");
  check(session.prepare_tactical_strategy(0,proposal["target_slot"],proposal["strategy"]).empty(),"Prepare player strategy");const auto pending=session.save();replay();
  check(pending["sram"]==before["sram"]&&pending["random_cursor"]==before["random_cursor"]&&pending["battle"]["tactics"]["points"]==before["battle"]["tactics"]["points"],"Proposal consumes no resources or RNG");
  check(!session.move_tactical(0,0).empty()&&!session.attack_tactical(0,0).empty()&&!session.end_tactical_turn().empty()&&!session.select_tactical_unit(1).empty()&&!session.prepare_tactical_strategy(0,0,0).empty()&&session.save()==pending,"Pending strategy locks all other actions");
  check(session.cancel_tactical_strategy().empty(),"Cancel pending proposal");replay();
  check(session.save()["sram"]==before["sram"]&&session.save()["random_cursor"]==before["random_cursor"],"Cancellation has no effect or random draw");
  check(session.prepare_tactical_strategy(0,proposal["target_slot"],proposal["strategy"]).empty()&&session.confirm_tactical_strategy().empty(),"Confirm player strategy");const auto applied=session.save();replay();
  check(applied["battle"]["tactics"]["points"].get<int>()==before["battle"]["tactics"]["points"].get<int>()-proposal["cost"].get<int>(),"Cost deducted exactly once even if effect fails");
  check(!session.confirm_tactical_strategy().empty()&&!session.cancel_tactical_strategy().empty()&&!session.end_tactical_turn().empty()&&session.save()==applied,"Result cannot reroll, cancel or skip");
  auto forged=applied;forged["battle"]["tactics"]["human_strategy"]["result"]["points_after"]=40;check(!loaded.restore(forged).empty()&&loaded.save()==applied,"Forged report is rejected atomically");
  forged=applied;forged["battle"]["tactics"]["moves"].back()["frame"]=256;check(!loaded.restore(forged).empty(),"Reject invalid event frame");
  check(session.finish_tactical_strategy().empty(),"Acknowledge strategy result");replay();
  check(!session.finish_tactical_strategy().empty(),"Cannot repeat result acknowledgement");
 }
 {
  OriginalSession session(rom),loaded(rom);check(session.restore(json(project+"/tests/tactical-turn-single.json")).empty(),"Load mobility exhaustion context");
  for(int i=0;i<5;++i){check(session.prepare_tactical_strategy(0,1,0).empty()&&session.confirm_tactical_strategy().empty()&&session.finish_tactical_strategy().empty(),"Repeat four-point strategy with explicit confirmation");}
  auto final=session.save();check(final["battle"]["tactics"]["turn_boundary"]=="computer"&&final["battle"]["tactics"]["side"]==0,"Exhausted human mobility hands control to computer");
  check(loaded.restore(final).empty()&&loaded.save()==final,"Zero-mobility strategy handover replays");
 }
 {
  auto base_save=json(project+"/tests/tactical-turn-start.json");
  for(auto *raw:{&base_save["sram"],&base_save["battle"]["tactics"]["deployment_sram"]}){(*raw)[0x438+2*8+2]=100;(*raw)[0x438+143*8+5]=0;}
  bool converted=false;
  for(int cursor=0;cursor<256&&!converted;++cursor){
   auto input=base_save;input["random_cursor"]=cursor;OriginalSession session(rom),loaded(rom);check(session.restore(input).empty()&&session.end_tactical_turn().empty(),"Controlled two-player conversion setup");
   check(session.prepare_tactical_strategy(0,2,7).empty()&&session.confirm_tactical_strategy().empty(),"Human defender attempts commander conversion");
   if(!session.save()["battle"]["tactics"]["human_strategy"]["result"]["success"].get<bool>())continue;
   check(loaded.restore(session.save()).empty()&&loaded.save()==session.save(),"Converted roster replays before acknowledgement");
   check(session.finish_tactical_strategy().empty(),"Confirm conversion result");const auto result=session.save();
   check(result["battle"]["tactics"]["turn_boundary"]=="battle_result"&&result["battle"]["tactics"]["turn_reason"]==20,"Conversion of attacking commander enters original defeat boundary");
   check(loaded.restore(result).empty()&&loaded.save()==result,"Conversion ending replays strictly");converted=true;
  }
  check(converted,"At least one deterministic successful human commander conversion");
 }
 std::cout<<"Player tactical strategy:452 menus,112 terrain gates,16 original execution flows and both human sides passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
