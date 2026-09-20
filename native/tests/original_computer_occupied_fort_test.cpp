#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"computer-occupied-fort.json");
 for(const auto &row:rows){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));std::uint8_t cursor=row["cursor_before"];
  const auto result=state.tactical_ai_occupied_fort(row["target"],row["slot"],row["points"],row["statuses"],cursor,row["scratch_before"]);
  const auto label="Original occupied fort "+row["id"].dump()+" expected "+row.dump()+" got "+result.dump();
  check(!result.contains("error")&&result["kind"]==row["kind"]&&result["command"]==row["command"]&&result["argument"]==row["argument"],label);
  check(cursor==row["cursor_after"],"RNG "+label+" cursor "+std::to_string(cursor));
  check(result["scratch21"]==row["scratch_after"],"Scratch "+label);
  const auto after=read(base+row["after"].get<std::string>());
  if(state.sram()!=after){std::string diff;for(int i=0;i<8192;++i)if(state.sram()[i]!=after[i])diff+=" "+std::to_string(i)+":"+std::to_string(state.sram()[i])+"/"+std::to_string(after[i]);check(false,"SRAM "+label+diff);}
 }
 for(const auto &flow:json(base+"computer-occupied-fort-flow.json")){
  OriginalState state(rom,read(base+flow["before"].get<std::string>()));std::uint8_t cursor=flow["cursor_before"];
  const auto result=state.tactical_ai_occupied_fort(flow["target"],flow["slot"],flow["points"],flow["statuses"],cursor,flow["scratch_before"]);
  check(result["kind"]==flow["kind"]&&result["direction"]==flow["direction"]&&cursor==flow["decision_cursor"],"Continuous original occupied-fort command and decision RNG");
  const int directions[]={3,2,1,0};int points=flow["points"];const int d=directions[flow["direction"].get<int>()];
  if(flow["kind"]=="move")check(state.tactical_step(flow["slot"],d,points,0).empty(),"Execute occupied-fort movement");
  else{
   const auto attack=state.tactical_attack(flow["slot"],d,points,0);check(!attack.contains("error"),"Execute occupied-fort attack");const auto clash=state.prepare_clash(attack,true,flow["frame"],false);
   for(const auto &[key,value]:flow["clash"].items())check(clash[key]==value,"Continuous occupied-fort clash field "+key);
  }
  auto expected=read(base+flow["after"].get<std::string>());
  if(flow["kind"]=="attack"){
   const auto before=read(base+flow["before"].get<std::string>());const int cell=before[0xdab+2*flow["slot"].get<int>()];
   check(expected[0xe1a+cell]==expected[0xf1a+cell],"Documented original initiator blink");expected[0xe1a+cell]=before[0xe1a+cell];
  }
  check(points==flow["points_after"]&&state.sram()==expected,"Continuous occupied-fort full SRAM and mobility, with only attack blink normalized");
 }
 // Unknown scratch is accepted only if the full native decision is invariant.
 int known=0,held=0;
 for(const auto &row:rows){
  auto raw=read(base+row["before"].get<std::string>());OriginalState state(rom,raw);std::uint8_t cursor=row["cursor_before"];
  const auto result=state.tactical_ai_occupied_fort(row["target"],row["slot"],row["points"],row["statuses"],cursor);
  if(result.contains("error")){++held;check(state.sram()==raw&&cursor==row["cursor_before"],"Unresolved scratch preserves all SRAM and RNG");}
  else{++known;check(result["kind"]==row["kind"]&&result["argument"]==row["argument"]&&state.sram()==read(base+row["after"].get<std::string>())&&cursor==row["cursor_after"],"Invariant unknown scratch still reproduces original");}
 }
 check(known>0&&held>0,"Exercise both invariant and scratch-dependent branches");
 for(const auto &file:std::filesystem::directory_iterator(project+"/tests")){
  const auto name=file.path().filename().string();if(name.rfind("computer-occupied-fort-",0)!=0||file.path().extension()!=".json")continue;
  const auto entry=json(file.path().string());OriginalSession session(rom),loaded(rom);check(session.restore(entry).empty()&&session.save()==entry,"Restore legal occupied-fort entry history "+name);
  auto replay=[&](){const auto save=session.save();const auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Strict occupied-fort replay: "+error);};
  check(session.continue_computer_occupied_fort().empty(),"Resume hostile-fort branch "+name);replay();const auto acted=session.save();const auto &t=acted["battle"]["tactics"];
  const std::string kind=t.contains("computer_motion")?t["computer_motion"]["kind"].get<std::string>():t["computer_strategy"]["kind"].get<std::string>();
  check(name=="computer-occupied-fort-"+kind+".json","Expected session outcome");
  check(!session.continue_computer_occupied_fort().empty()&&session.save()==acted,"Cannot reroll/repeat occupied-fort action");
  auto forged=acted;forged["battle"]["tactics"]["moves"].back()["cursor"]=256;check(!loaded.restore(forged).empty()&&loaded.save()==acted,"Invalid occupied-fort cursor rejected atomically");
  forged=acted;forged["battle"]["tactics"]["moves"].back()["frame"]=-1;check(!loaded.restore(forged).empty(),"Invalid occupied-fort frame rejected");
  forged=acted;forged["sram"][0xded+entry["battle"]["tactics"]["selected"].get<int>()*2]=255;check(!loaded.restore(forged).empty(),"Forged direction cache rejected");
  if(kind=="strategy"){
   check(session.execute_computer_strategy().empty(),"Execute occupied-fort strategy");replay();check(session.finish_computer_strategy().empty(),"Finish occupied-fort strategy");replay();
  }else if(kind=="attack"){
   check(t["attack"]["stage"]=="clash_ready"&&t["points"]==entry["battle"]["tactics"]["points"].get<int>()-3,"Attack charges and prepares real clash");
   check(session.begin_clash().empty(),"Enter occupied-fort clash");replay();const auto clash=session.save()["battle"]["tactics"]["attack"]["clash"];const int human=clash["players"][0]!=0?0:1;
   for(int i=0;i<3;++i)check(session.cycle_clash_order(human,3).empty(),"Human surrender order");
   check(session.request_clash_surrender().empty()&&session.answer_clash_surrender(true).empty()&&session.advance_clash_surrender().empty()&&session.advance_clash_surrender().empty()&&session.finish_clash_result().empty(),"Resolve occupied-fort clash");replay();
  }else if(kind=="move")check(t["points"].get<int>()<entry["battle"]["tactics"]["points"].get<int>(),"Movement charged");
  else{check(session.continue_computer_scan().empty(),"Scan after occupied-fort no command");replay();continue;}
  check(session.advance_computer_tactics().empty()&&session.plan_computer_tactics().empty(),"Next AI selection after occupied-fort action");replay();
 }
 {
  const auto &row=rows[0];auto raw=read(base+row["before"].get<std::string>());OriginalState state(rom,raw);std::uint8_t cursor=23;auto status=row["statuses"];
  status[0]=256;check(state.tactical_ai_occupied_fort(row["target"],row["slot"],row["points"],status,cursor,0).contains("error")&&state.sram()==raw&&cursor==23,"Invalid status atomic");
  check(state.tactical_ai_occupied_fort(row["target"],row["slot"],row["points"],row["statuses"],cursor,256).contains("error")&&state.sram()==raw&&cursor==23,"Invalid scratch atomic");
  OriginalSession session(rom);const auto other=json(project+"/tests/computer-flank-scan.json");check(session.restore(other).empty()&&!session.continue_computer_occupied_fort().empty()&&session.save()==other,"Cannot bypass unrelated AI boundary");
 }
 std::cout<<"Original occupied-fort "<<rows.size()<<" samples passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
