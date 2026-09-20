#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"computer-castle.json");
 for(const auto &row:rows){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));auto status=Json(std::vector<int>(24,0));status[row["slot"].get<int>()]=row["status"];
  const auto result=state.tactical_ai_fort(row["target"],row["previous"],row["points"],status);
  check(!result.contains("error")&&result["kind"]==row["kind"]&&result["slot"]==row["selected"]&&result["fort"]==row["fort"]&&result["enemy_commander"]==row["enemy_commander"]&&result["direction"]==row["direction"],"Original fort decision "+row["id"].dump()+" "+result.dump());
  check(state.sram()==read(base+row["after"].get<std::string>())&&row["cursor_before"]==row["cursor_after"],"Original full SRAM and no RNG in fort rule "+row["id"].dump());
 }
 {
  const auto flow=json(base+"computer-fort-flow.json");OriginalState state(rom,read(base+flow["before"].get<std::string>()));
  const auto decision=state.tactical_ai_fort(flow["target"],flow["previous"],flow["points"],Json(std::vector<int>(24,0)));
  check(decision["kind"]=="move"&&decision["slot"]==flow["slot"]&&decision["direction"]==flow["direction"],"Continuous original chooses same unit and direction");
  int points=flow["points"];const int directions[]={3,2,1,0};
  check(state.tactical_step(decision["slot"],directions[decision["direction"].get<int>()],points,0).empty()&&points==flow["points_after"]&&state.sram()==read(base+flow["after"].get<std::string>()),"Continuous original AI-to-movement full SRAM and mobility");
 }
 {
  auto raw=read(base+rows[0]["before"].get<std::string>());OriginalState state(rom,raw);auto status=Json(std::vector<int>(24,0));
  check(state.tactical_ai_fort(30,0,40,status).contains("error")&&state.tactical_ai_fort(12,12,40,status).contains("error")&&state.sram()==raw,"Invalid fort arguments preserve SRAM");
  status[11]=256;check(state.tactical_ai_fort(12,0,40,status).contains("error")&&state.sram()==raw,"Invalid status effect preserves SRAM");
  for(int i=0;i<11;++i)raw[0xdc4+i*3]&=127;OriginalState no_commander(rom,raw);status[11]=0;
  check(no_commander.tactical_ai_fort(12,0,40,status).contains("error")&&no_commander.sram()==raw,"Missing commander cannot clear roles before validation");
 }
 OriginalSession session(rom),loaded(rom);const auto entry=json(project+"/tests/computer-fort-start.json");
 check(session.restore(entry).empty()&&session.save()==entry,"Legal ruler-withdrawal history reaches fort planning");
 auto replay=[&](){const auto save=session.save();const auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Fort strict replay: "+error);};
 check(!session.move_tactical(3,2).empty()&&!session.end_tactical_turn().empty()&&session.save()==entry,"Human cannot steer or skip computer move");
 check(session.plan_computer_tactics().empty(),"Computer performs adjacent fort move");replay();auto moved=session.save();const auto plan=moved["battle"]["tactics"]["computer_plan"];
 check(plan["kind"]=="move"&&plan["slot"]==3&&plan["position"]==68&&plan["destination"]==84&&plan["points_after"]==19&&moved["sram"][0xdec+6]==15,"Exact selected unit, direction, mobility and defender role");
 check(moved["random_cursor"]==entry["random_cursor"]&&moved["battle"]["tactics"]["computer_cursor"]==3,"Fort rule consumes no RNG and remembers scan position");
 check(!session.plan_computer_tactics().empty()&&session.save()==moved,"Movement cannot be repeated without a new decision");
 auto forged=moved;forged["battle"]["tactics"]["computer_cursor"]=2;check(!loaded.restore(forged).empty()&&loaded.save()==moved,"Forged scan cursor rejected");
 forged=moved;forged["battle"]["tactics"]["computer_plan"]["direction"]=0;check(!loaded.restore(forged).empty(),"Forged movement direction rejected");
 check(session.advance_computer_tactics().empty()&&session.plan_computer_tactics().empty(),"Next decision resumes original descending unit scan");replay();const auto held=session.save();
 check(held["battle"]["tactics"]["computer_plan"]["slot"]==2&&held["battle"]["tactics"]["computer_plan"]["kind"]=="role_plan","Occupied fort reaches role-specific planning for next unit");
 check(!session.plan_computer_tactics().empty()&&!session.advance_computer_tactics().empty()&&session.save()==held,"Unported role planning is not skipped or rerolled");
 check(loaded.restore(entry).empty()&&loaded.save()==entry,"Previous assessment save remains compatible");
 std::cout<<"Original computer fort "<<rows.size()<<" samples passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
