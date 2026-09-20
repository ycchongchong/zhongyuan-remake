#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"tactical-ai-assessment.json");
 for(const auto &row:rows){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));std::uint8_t cursor=row["cursor_before"];
  const auto result=state.tactical_ai_assessment(row["side"],row["turn"],row["points"],cursor);
  check(!result.contains("error")&&result["kind"]==row["kind"]&&result["slot"]==row["slot"],"Original AI decision "+row["id"].dump()+" "+result.dump());
  if(result["score"]!=-1)check(result["score"]==row["score"],"Original quantized strength "+row["id"].dump()+" "+result.dump()+" expected "+row["score"].dump());
  check(state.sram()==read(base+row["after"].get<std::string>())&&cursor==row["cursor_after"],"Original full SRAM/RNG AI assessment "+row["id"].dump());
 }
 {
  const auto flow=json(base+"computer-retreat-flow.json");OriginalState state(rom,read(base+flow["before"].get<std::string>()));std::uint8_t cursor=flow["cursor_before"];
  const auto decision=state.tactical_ai_assessment(flow["side"],flow["round"],flow["points"],cursor);
  check(decision["kind"]=="retreat"&&decision["slot"]==flow["slot"],"Continuous original selects same withdrawing ruler");
  cursor=flow["retreat_cursor"];const auto result=state.retreat_defender(decision["slot"],flow["target"],false,cursor);
  check(result["departed"]==true&&flow["points_after"].get<int>()==flow["points"].get<int>()-1&&state.sram()==read(base+flow["after"].get<std::string>())&&cursor==flow["cursor_after"],"Continuous original AI-to-retreat full SRAM and measured entry RNG");
 }
 {
  auto raw=rom.initial_sram(0);raw[0xde3]=0x42;raw[0xdaa]=241;OriginalState state(rom,raw);std::uint8_t cursor=255;
  check(state.tactical_ai_assessment(0,0,20,cursor).contains("error")&&state.sram()==raw&&cursor==255,"Invalid roster cannot consume RNG or update roles");
  check(state.tactical_ai_assessment(1,0,20,cursor).contains("error")&&state.tactical_ai_assessment(0,0,41,cursor).contains("error"),"Invalid AI context rejected");
 }
 OriginalSession session(rom),loaded(rom);const auto entry=json(project+"/tests/computer-retreat-start.json");
 check(session.restore(entry).empty(),"Load controlled legal computer turn");
 auto replay=[&](){const auto save=session.save();auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Computer tactical strict replay: "+error);};
 check(!session.end_tactical_turn().empty()&&!session.move_tactical(0,0).empty()&&session.save()==entry,"Human cannot override computer decisions");
 int departures=0;
 for(int i=0;i<12;++i){
  auto before=session.save();check(session.advance_computer_tactics().empty(),"Computer assesses and withdraws");replay();auto report=session.save();const auto &t=report["battle"]["tactics"];
  check(t["computer"]["kind"]=="retreat"&&t.contains("retreat")&&t["retreat"]["computer"]==true&&t["points"].get<int>()==before["battle"]["tactics"]["points"].get<int>()-1,"Computer retreat records result and spends one mobility");
  const auto outcome=t["retreat"]["outcomes"][0];check(outcome["departed"]==true&&outcome["returned"]==true,"Computer chooses another city with vacancy");
  if(!departures)check(outcome["officer"]==2&&t["computer"]["slot"]==0,"Non-easy difficulty withdraws defending ruler first");
  ++departures;
  check(!session.advance_computer_tactics().empty()&&!session.cancel_tactical_retreat().empty()&&session.save()==report,"Pending computer report cannot be rerolled or cancelled");
  auto forged=report;forged["battle"]["tactics"]["computer"]["slot"]=11;check(!loaded.restore(forged).empty()&&loaded.save()==report,"Forged AI target rejected atomically");
  check(session.finish_tactical_retreat().empty(),"Acknowledge computer withdrawal");replay();
  if(session.save()["battle"]["tactics"]["turn_boundary"]=="battle_result")break;
 }
 check(departures==4&&session.save()["battle"]["tactics"]["turn_reason"]==2,"All four original defenders leave and enter defender withdrawal result");
 for(int i=0;i<42;++i){check(session.advance_withdrawal_result().empty(),"Resolve computer army withdrawal result");replay();if(session.save()["battle"]["tactics"]["settlement"]["stage"]==13)break;}
 check(session.finish_withdrawal_result().empty()&&session.save()["battle"].is_null()&&session.save()["phase"]=="player_commands","Computer withdrawal returns to human strategic turn");replay();
 check(loaded.restore(entry).empty()&&loaded.save()==entry,"Old computer-boundary save remains compatible");
 // A normal army reaches the honest planning boundary once, never a fake pass.
 auto start=json(project+"/tests/clash-session-start.json");start["sram"]=start["battle"]["tactics"]["deployment_sram"];start["battle"].erase("tactics");
 check(session.restore(start).empty()&&session.begin_tactics().empty()&&session.end_tactical_turn().empty()&&session.advance_computer_tactics().empty(),"Normal computer army evaluates retreat");replay();
 const auto plan=session.save();check(plan["battle"]["tactics"]["computer"]["kind"]=="plan"&&plan["battle"]["tactics"]["turn_boundary"]=="computer","Movement planning stays explicit, not an invented end turn");
 check(!session.advance_computer_tactics().empty()&&!session.end_tactical_turn().empty()&&session.save()==plan,"Unsupported planning cannot repeatedly consume RNG or skip computer turn");
 std::cout<<"Original tactical AI assessment "<<rows.size()<<" samples passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
