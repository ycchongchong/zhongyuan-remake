#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"computer-strategy.json");
 for(const auto &row:rows){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));std::uint8_t cursor=row["cursor_before"];
  const auto result=state.tactical_ai_strategy(row["target"],row["slot"],row["points"],row["statuses"],cursor);
  check(!result.contains("error")&&result["kind"]==row["kind"]&&result["command"]==row["command"]&&result["argument"]==row["argument"],"Original strategy decision "+row["id"].dump()+" "+result.dump());
  check(state.sram()==read(base+row["after"].get<std::string>())&&cursor==row["cursor_after"],"Original full SRAM/RNG "+row["id"].dump()+" cursor "+std::to_string(cursor)+" expected "+row["cursor_after"].dump());
 }
 {
  auto raw=read(base+rows[0]["before"].get<std::string>());OriginalState state(rom,raw);std::uint8_t cursor=255;auto status=Json(std::vector<int>(24,0));
  check(state.tactical_ai_strategy(30,11,40,status,cursor).contains("error")&&state.tactical_ai_strategy(12,12,40,status,cursor).contains("error")&&state.sram()==raw&&cursor==255,"Invalid arguments cannot mutate SRAM/RNG");
  status[12]=256;check(state.tactical_ai_strategy(12,11,40,status,cursor).contains("error")&&state.sram()==raw&&cursor==255,"Invalid status cannot consume RNG");
  status[12]=0;raw[0xe1a+0x56]=0x3f;OriginalState malformed(rom,raw);
  check(malformed.tactical_ai_strategy(12,11,40,status,cursor).contains("error")&&malformed.sram()==raw&&cursor==255,"Invalid enemy slot rejected atomically before role changes");
 }
 OriginalSession session(rom),loaded(rom);auto entry=json(project+"/tests/computer-strategy-start.json");
 check(session.restore(entry).empty()&&session.save()==entry,"Legal movement and handover history reaches role planning");
 check(!session.move_tactical(3,0).empty()&&!session.end_tactical_turn().empty()&&session.save()==entry,"Human cannot override computer strategy selection");
 check(session.evaluate_computer_strategy().empty(),"Select strategy using original probability and priority");auto chosen=session.save();const auto &decision=chosen["battle"]["tactics"]["computer_strategy"];
 check(decision["kind"]=="strategy"&&decision["strategy"]==7&&decision["slot"]==3&&decision["target_slot"]==0&&decision["target_cell"]==0x47,"Original choice reaches enemy moved by player");
 check(chosen["battle"]["tactics"]["points"]==entry["battle"]["tactics"]["points"],"No strategy cost or effects invented before execution");
 check(loaded.restore(chosen).empty()&&loaded.save()==chosen,"Selected strategy and RNG replay exactly");
 check(!session.evaluate_computer_strategy().empty()&&!session.advance_computer_tactics().empty()&&session.save()==chosen,"Selection cannot be rerolled or bypassed");
 auto forged=chosen;forged["battle"]["tactics"]["computer_strategy"]["target_slot"]=1;
 check(!loaded.restore(forged).empty()&&loaded.save()==chosen,"Forged target rejected without changing live save");
 forged=chosen;forged["battle"]["tactics"]["computer_strategy"]["strategy"]=0;check(!loaded.restore(forged).empty(),"Forged strategy rejected");
 forged=chosen;forged["battle"]["tactics"]["moves"].back()["cursor"]=256;check(!loaded.restore(forged).empty(),"Out-of-range RNG event rejected");
 check(loaded.restore(entry).empty()&&loaded.save()==entry,"Previous role-plan save remains compatible");
 check(session.restore(json(project+"/tests/computer-fort-start.json")).empty()&&session.plan_computer_tactics().empty()&&session.advance_computer_tactics().empty()&&session.plan_computer_tactics().empty(),"Fort movement reaches next unit strategy evaluation");
 check(session.evaluate_computer_strategy().empty(),"No-target strategy assessment advances once");auto held=session.save();
 check(held["battle"]["tactics"]["computer_strategy"]["kind"]=="no_strategy"&&loaded.restore(held).empty()&&loaded.save()==held,"No-strategy continuation reloads exactly");
 check(!session.evaluate_computer_strategy().empty()&&session.save()==held,"No-target assessment cannot reroll");
 std::cout<<"Original computer strategy "<<rows.size()<<" samples and session replay passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
