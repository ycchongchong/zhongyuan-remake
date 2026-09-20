#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool value,const std::string &message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string &path){std::ifstream f(std::filesystem::u8path(path),std::ios::binary);check(bool(f),path);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &path){std::ifstream f(std::filesystem::u8path(path));check(bool(f),path);return Json::parse(f);}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 const auto rows=json(base+"ruler-annexation.json");
 for(const auto &row:rows){
  OriginalState state(rom,read(base+row["before"].get<std::string>()));std::uint8_t cursor=row["cursor_before"];
  const auto result=state.annex_ruler_city(row["city"],row["loser"],row["winner"],cursor);
  check(!result.contains("error")&&result["stage"]==row["stage_after"]&&result["next_city"]==row["city_after"],"Original ruler annexation step "+row["id"].dump()+": "+result.dump());
  check(state.sram()==read(base+row["after"].get<std::string>())&&cursor==row["cursor_after"],"Original full SRAM/RNG annexation "+row["id"].dump());
 }
 for(const auto *name:{"ruler-defeat-flow","ruler-attacker-flow"}){
  const auto flow=json(base+name+".json");const auto &steps=flow["steps"];
  OriginalState state(rom,read(base+steps[0]["before"].get<std::string>()));auto captives=steps[0]["captives_before"];
  for(const auto &step:steps){
   check(state.sram()==read(base+step["before"].get<std::string>())&&captives==step["captives_before"],"No omitted writes between original ruler result dispatches");
   std::uint8_t cursor=step["cursor_before"];
   if(step["stage"]=="close")state.close_battle(flow["target"]);
   else if(step["stage"]=="annex"){
    const auto result=state.annex_ruler_city(step["city"],step["loser"],step["winner"],cursor);
    check(!result.contains("error")&&result["next_city"]==step["city_after"]&&result["stage"]==step["stage_after"],"Continuous ruler land transfer");
   }else check(!state.withdrawal_result_step(flow["target"],flow["source"],step["stage"],captives,cursor).contains("error"),"Original ruler battle settlement");
   check(state.sram()==read(base+step["after"].get<std::string>())&&captives==step["captives_after"]&&cursor==step["cursor_after"],"Continuous original ruler result reaches strategy exactly with measured entry cursors");
  }
 }
 {
  auto raw=rom.initial_sram(0);raw[0]=2;raw[16]=12;raw[27]=241;OriginalState state(rom,raw);std::uint8_t cursor=255;
  check(state.annex_ruler_city(0,2,4,cursor).contains("error")&&state.sram()==raw&&cursor==255,"Invalid resident cannot partially transfer a city");
  check(state.annex_ruler_city(30,2,4,cursor).contains("error")&&state.annex_ruler_city(0,2,2,cursor).contains("error"),"Invalid annexation arguments rejected");
 }
 OriginalSession session(rom),loaded(rom);const auto entry=json(project+"/tests/clash-npc-ruler-defeat.json");
 check(session.restore(entry).empty()&&session.save()==entry,"Legal NPC ruler defeat history restores exactly");
 auto replay=[&](){const auto save=session.save();const auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Strict ruler result replay: "+error);};
 check(!session.begin_ruler_defeat_result().empty()&&!session.advance_ruler_annexation().empty()&&session.save()==entry,"Cannot transfer land before defeat confirmation");
 check(session.advance_clash_defeat().empty()&&session.advance_clash_defeat().empty(),"Confirm NPC ruler defeat");replay();
 const auto held=session.save();check(held["battle"]["tactics"]["attack"]["result"]["officer"]==2&&session.snapshot()["battle"]["ruler_defeat_result_available"]==true,"Defeated NPC Cao Cao exposes ruler branch");
 check(!session.begin_defender_defeat_result().empty()&&!session.begin_commander_defeat_result().empty()&&session.save()==held,"Cannot substitute ordinary defeat and skip annexation");
 check(session.begin_ruler_defeat_result().empty()&&session.save()["battle"]["tactics"]["turn_reason"]==131,"Enter original defender-ruler defeat reason 83");replay();
 for(int i=0;i<42;++i){check(!session.advance_ruler_annexation().empty(),"Battle settlement precedes land transfer");check(session.advance_withdrawal_result().empty(),"Resolve ruler battle");replay();if(session.save()["battle"]["tactics"]["settlement"]["stage"]==29)break;}
 const auto battle_report=session.save();check(battle_report["battle"]["tactics"]["settlement"]["stage"]==29&&!session.finish_withdrawal_result().empty(),"Final battle report cannot omit land transfer");
 for(int city=0;city<30;++city){
  const auto before=session.save();check(session.advance_ruler_annexation().empty(),"Transfer next ruler city");replay();const auto after=session.save();
  check(after["battle"]["tactics"]["annexation"]["next_city"]==city+1,"All thirty cities visited once");
  if((before["sram"][city*36].get<int>()&7)==2)check((after["sram"][city*36].get<int>()&7)==4,"Defeated ruler city transfers to winner");
  else for(int j=0;j<36;++j)check(after["sram"][city*36+j]==before["sram"][city*36+j],"Other cities remain unchanged, including already occupied target");
  if(city<29)check(!session.finish_withdrawal_result().empty()&&session.save()==after,"Cannot leave partial annexation");
 }
 const auto final=session.save();check(!session.advance_ruler_annexation().empty()&&!session.advance_withdrawal_result().empty()&&session.save()==final,"Completed transfers cannot execute twice");
 check(!final["battle"]["tactics"]["annexation"]["outcomes"].empty(),"NPC defeat transfers remaining territory");
 for(int city=0;city<30;++city)check((final["sram"][city*36].get<int>()&7)!=2,"No defeated ruler territory remains");
 for(int i=0;i<16;++i)check(final["sram"][12*36+i]==battle_report["sram"][12*36+i],"Target resources not reduced twice");
 check(final["sram"][0xd8f+4*4]==held["sram"][0xd8f+4*4],"Winning current ruler keeps command books");
 auto forged=final;forged["battle"]["tactics"]["annexation"]["next_city"]=29;check(!loaded.restore(forged).empty()&&loaded.save()==final,"Cannot forge annexation cursor to repeat resource loss");
 check(session.finish_withdrawal_result().empty()&&session.save()["phase"]=="player_commands"&&session.save()["battle"].is_null(),"NPC ruler result returns to strategy");replay();
 check(loaded.restore(held).empty()&&loaded.save()==held,"Old held NPC ruler save remains compatible");
 check(session.restore(json(project+"/tests/clash-defeat-3.json")).empty()&&session.advance_clash_defeat().empty()&&session.advance_clash_defeat().empty(),"Reach separate human-ruler failure");const auto human=session.save();
 check(session.snapshot()["battle"]["ruler_defeat_result_available"]==false&&!session.begin_ruler_defeat_result().empty()&&session.save()==human,"Human ruler failure cannot enter NPC annexation");
 std::cout<<"Original ruler annexation "<<rows.size()<<" samples; both continuous original branches, legal NPC defeat, thirty-city transfer, strict replay and strategic return passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
