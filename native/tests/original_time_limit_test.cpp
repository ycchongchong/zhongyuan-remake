#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool value,const std::string &message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string &path){std::ifstream f(std::filesystem::u8path(path),std::ios::binary);check(bool(f),path);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &path){std::ifstream f(std::filesystem::u8path(path));check(bool(f),path);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"time-limit.json");
 for(const auto &row:rows){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));
  const auto result=state.time_limit_step(row["stage"],row["mask"],row["speaker_before"]);
  check(!result.contains("error")&&result["stage"]==row["stage_after"],"Original time-limit phase "+row["id"].dump()+": "+result.dump());
  for(const auto *key:{"speaker","faction","phase","reason"})check(result[key]==row[key],"Original time-limit field "+std::string(key)+": "+row["id"].dump());
  check(state.sram()==read(base+row["after"].get<std::string>())&&row["cursor_before"]==row["cursor_after"],"Time-limit notice preserves full SRAM and consumes no RNG");
 }
 const auto flow=json(base+"time-limit-flow.json");const auto &steps=flow["steps"];
 OriginalState state(rom,read(base+steps[0]["before"].get<std::string>()));auto captives=steps[0]["captives_before"];
 for(const auto &step:steps){
  check(state.sram()==read(base+step["before"].get<std::string>())&&captives==step["captives_before"],"No omitted writes between original time-limit dispatches");
  std::uint8_t cursor=step["cursor_before"];
  if(step["stage"]=="close")state.close_battle(flow["target"]);
  else if(step["stage"].is_string()){
   auto result=state.time_limit_step(step["counter_before"],flow["human_mask"],step["speaker_before"]);
   check(!result.contains("error")&&result["stage"]==step["stage_after"]&&result["speaker"]==step["speaker"]&&result["phase"]==step["phase"]&&result["reason"]==step["reason"],"Original notice transitions continuously into forced withdrawal");
  }else check(!state.withdrawal_result_step(flow["target"],flow["source"],step["stage"],captives,cursor).contains("error"),"Continuous forced-withdrawal step");
  check(state.sram()==read(base+step["after"].get<std::string>())&&captives==step["captives_after"]&&cursor==step["cursor_after"],"Continuous original time-limit flow reaches strategy with measured entry cursors");
 }
 check(state.time_limit_step(3,18,145).contains("error")&&state.time_limit_step(0,49,145).contains("error")&&state.time_limit_step(1,18,241).contains("error"),"Invalid notice inputs rejected");
 OriginalSession session(rom),loaded(rom);
 auto replay=[&](){const auto save=session.save();const auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Strict time-limit replay: "+error);};
 for(int prisoner:{0,12,145}){
  check(session.restore(json(project+(prisoner?"/tests/tactical-defender-clash.json":"/tests/tactical-turn-start.json"))).empty(),"Load real tactical history");
  const auto start=session.save();check(!session.advance_time_limit_result().empty()&&session.save()==start,"Cannot force early time-limit result");
  if(prisoner){
   const int side=start["battle"]["tactics"]["attack"]["clash"]["first"]==prisoner?0:1;
   for(int i=0;i<3;++i)check(session.cycle_clash_order(side,3).empty(),"Set surrender order");
   check(session.request_clash_surrender().empty()&&session.answer_clash_surrender(true).empty()&&session.advance_clash_surrender().empty()&&session.advance_clash_surrender().empty()&&session.finish_clash_result().empty(),"Real surrender creates battle captive");
  }
  for(int i=0;i<22&&!session.save()["battle"]["tactics"].contains("turn_boundary");++i)check(session.end_tactical_turn().empty(),"Play both sides through original eleven-round limit");
  const auto held=session.save();check(held["battle"]["tactics"]["turn_boundary"]=="time_limit"&&held["battle"]["tactics"]["round"]==11,"Reach original time-limit boundary");replay();
  for(int step=0;step<3;++step){
   const auto before=session.save();check(!session.advance_withdrawal_result().empty()&&!session.finish_withdrawal_result().empty()&&session.save()==before,"Cannot skip limit announcement");
   check(session.advance_time_limit_result().empty(),"Advance original limit notice");replay();
   check(session.save()["sram"]==held["sram"]&&session.save()["random_cursor"]==held["random_cursor"],"All limit notices preserve SRAM and RNG");
   if(step<2)check(session.save()["battle"]["tactics"]["time_limit"]["stage"]==step+1,"Notice saves at each acknowledgement");
  }
  const auto opened=session.save();check(opened["battle"]["tactics"]["turn_reason"]==36&&opened["battle"]["tactics"]["turn_boundary"]=="battle_result","Limit leads to attacker withdrawal, not occupation or commander capture");
  check(!session.advance_time_limit_result().empty()&&session.save()==opened,"Cannot repeat confirmed notice");
  auto forged=opened;forged["battle"]["tactics"]["time_limit"]["speaker"]=0;check(!loaded.restore(forged).empty()&&loaded.save()==opened,"Forged spokesperson rejected atomically");
  for(int i=0;i<42;++i){check(session.advance_withdrawal_result().empty(),"Settle forced withdrawal");replay();if(session.save()["battle"]["tactics"]["settlement"]["stage"]==17)break;}
  const auto report=session.save();check(report["battle"]["tactics"]["settlement"]["stage"]==17,"Limit result reaches complete report");
  for(int i=0;i<16;++i)check(report["sram"][12*36+i]==held["sram"][12*36+i],"Limit preserves defending city ownership and resources");
  check(!session.advance_withdrawal_result().empty()&&session.save()==report,"No repeated settlement");
  check(session.finish_withdrawal_result().empty()&&session.save()["phase"]=="player_commands"&&session.save()["battle"].is_null(),"Limit result returns to strategic commands");replay();
  const auto closed=session.save();
  for(int i=0;i<30;++i)for(int j:{30,31})check(closed["sram"][i*36+j]==held["sram"][i*36+j],"Current command books survive limit settlement");
  for(int i=0;i<11;++i){const int id=held["sram"][0xdc2+i*3];if(id==255)continue;const int city=held["sram"][0xdc4+i*3].get<int>()&31;bool found=false;for(int j=0;j<12;++j)found|=closed["sram"][city*36+16+j]==id;check(found,"Every surviving attacker returns to its source");}
  if(prisoner){const int city=prisoner==12?13:12;bool found=false;for(int i=0;i<12;++i)found|=closed["sram"][city*36+16+i]==prisoner;check(found,"Limit captive destination follows forced withdrawal branch");}
  check(loaded.restore(held).empty()&&loaded.save()==held,"Previously held limit save remains exactly compatible");
 }
 check(session.restore(json(project+"/tests/tactical-turn-single.json")).empty()&&session.end_tactical_turn().empty(),"Enter unsupported computer turn");const auto ai=session.save();
 check(!session.advance_time_limit_result().empty()&&session.save()==ai,"Cannot bypass computer turn via time-limit result");
 std::cout<<"Original time-limit "<<rows.size()<<" samples; continuous notice/withdrawal flow, eleven actual rounds, both captive destinations, strict replay and strategic return passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
