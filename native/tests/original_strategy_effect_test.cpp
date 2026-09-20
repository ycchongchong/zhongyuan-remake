#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"tactical-strategy-effect.json");
 for(const auto &row:rows){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));std::uint8_t cursor=row["cursor_before"];
  const auto result=state.resolve_tactical_strategy(12,row["side"],0,0,row["strategy"],row["frame"],row["status_before"],cursor);
  check(!result.contains("error")&&result["success"]==row["success"]&&result["status"]==row["status_after"]&&cursor==row["cursor_after"],"Original effect "+row["id"].dump()+" "+result.dump()+" cursor "+std::to_string(cursor)+" expected "+row["cursor_after"].dump());
  const auto expected=read(base+row["after"].get<std::string>());
  if(state.sram()!=expected){for(int i=0;i<8192;++i)if(state.sram()[i]!=expected[i])std::cerr<<std::hex<<i<<":"<<int(state.sram()[i])<<"/"<<int(expected[i])<<" ";throw std::runtime_error("Full SRAM mismatch "+row["id"].dump());}
 }
 {
  const auto flow=json(base+"strategy-execution-flow.json");OriginalState state(rom,read(base+flow["before"].get<std::string>()));std::uint8_t cursor=flow["cursor_before"];
  const auto result=state.resolve_tactical_strategy(flow["city"],flow["side"],flow["slot"],flow["target_slot"],flow["strategy"],flow["frame"],flow["status_before"],cursor);
  check(result["success"]==flow["success"]&&result["status"]==flow["status_after"]&&cursor==flow["cursor_after"]&&state.sram()==read(base+flow["after"].get<std::string>()),"Continuous original effect matches full SRAM, status and measured entry RNG/frame");
  check(flow["points_before"].get<int>()-result["cost"].get<int>()==flow["points_after"],"Continuous original charges the same cost before failure");
 }
 {
  const auto raw=read(base+rows[0]["before"].get<std::string>());OriginalState state(rom,raw);std::uint8_t cursor=255;auto status=Json(std::vector<int>(24,0));
  check(state.resolve_tactical_strategy(30,0,0,0,7,0,status,cursor).contains("error")&&state.resolve_tactical_strategy(12,1,0,0,7,0,status,cursor).contains("error")&&state.sram()==raw&&cursor==255,"Invalid effect context preserves SRAM/RNG");
  status[0]=-1;check(state.resolve_tactical_strategy(12,0,0,0,7,0,status,cursor).contains("error")&&state.sram()==raw&&cursor==255,"Invalid status cannot partially execute");
 }
 for(const auto &name:{"strategy-effect-start","strategy-effect-failure","strategy-effect-commander"}){
  OriginalSession session(rom),loaded(rom);const auto entry=json(project+"/tests/"+name+".json");check(session.restore(entry).empty()&&session.save()==entry,"Restore selected strategy fixture");
  check(!session.finish_computer_strategy().empty()&&session.save()==entry,"Cannot confirm before execution");
  check(session.execute_computer_strategy().empty(),"Execute selected computer strategy");const auto report=session.save();const auto result=report["battle"]["tactics"]["strategy_result"];
  const bool success=std::string(name)!="strategy-effect-failure";
  check(result["success"]==success&&result["points_before"]==24&&result["points_after"]==14,"Success and failure both spend original mobility");
  check(loaded.restore(report).empty()&&loaded.save()==report,"Execution report strictly replays");
  check(!session.execute_computer_strategy().empty()&&!session.advance_computer_tactics().empty()&&!session.end_tactical_turn().empty()&&session.save()==report,"No duplicate charge, reroll, override or skipped result");
  if(success)check(report["sram"][0xdc2]==255&&report["sram"][0xdaa+8]==145&&report["sram"][0xe1a+0x47]==0x14,"Conversion updates source, destination and occupancy");
  auto forged=report;forged["battle"]["tactics"]["strategy_result"]["success"]=!success;
  check(!loaded.restore(forged).empty()&&loaded.save()==report,"Forged effect result rejected atomically");
  forged=report;forged["battle"]["tactics"]["moves"].back()["frame"]=256;check(!loaded.restore(forged).empty(),"Invalid execution frame rejected");
  check(session.finish_computer_strategy().empty(),"Confirm result");auto finished=session.save();check(loaded.restore(finished).empty()&&loaded.save()==finished,"Post-effect transition reloads exactly");
  check(!session.finish_computer_strategy().empty()&&session.save()==finished,"Result cannot be confirmed twice");
  if(std::string(name)=="strategy-effect-commander"){
   check(finished["battle"]["tactics"]["turn_boundary"]=="battle_result"&&finished["battle"]["tactics"]["turn_reason"]==20,"Converted attacking commander enters original reason20 battle result");
   for(int i=0;i<25;++i){check(session.advance_withdrawal_result().empty(),"Settle converted commander battle result");auto step=session.save();check(loaded.restore(step).empty()&&loaded.save()==step,"Battle settlement replays after strategy conversion");if(step["battle"]["tactics"]["settlement"]["stage"]==4)break;}
   check(session.finish_withdrawal_result().empty()&&session.save()["phase"]=="player_commands"&&session.save()["battle"].is_null(),"Commander conversion settles back to strategy map");
  }else check(session.advance_computer_tactics().empty(),"Computer resumes assessment after success or failure");
 }
 std::cout<<"Original strategy effects "<<rows.size()<<" samples, continuous flow and session settlement passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
