#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 std::ifstream f(base+"defender-retreat.json");const auto rows=Json::parse(f);
 for(const auto &c:rows){
  const auto raw=read(base+c["before"].get<std::string>());OriginalState state(rom,raw);std::uint8_t cursor=c["cursor_before"];
  auto result=state.retreat_defender(c["slot"],c["target"],c["human"],cursor);
  check(!result.contains("error"),"Withdrawal rejected "+c["id"].dump()+": "+result.dump());
  check(result["departed"]==(c["counter_after"]==3),"Original acceptance "+c["id"].dump());
  check(c["points_after"].get<int>()==c["points"].get<int>()-(result["departed"].get<bool>()?1:0),"Original mobility");
  check(state.sram()==read(base+c["after"].get<std::string>())&&cursor==c["cursor_after"],"Complete SRAM/RNG sample "+c["id"].dump());
 }
 const auto raw=read(base+rows[0]["before"].get<std::string>());OriginalState state(rom,raw);std::uint8_t cursor=42;
 for(int slot:{-1,12,255})check(state.retreat_defender(slot,12,true,cursor).contains("error")&&state.sram()==raw&&cursor==42,"Invalid slots are atomic");
 check(state.retreat_defender(0,30,true,cursor).contains("error")&&state.sram()==raw,"Invalid city rejected");
 auto corrupt=raw;corrupt[0xdab]=255;OriginalState invalid(rom,corrupt);
 check(invalid.retreat_defender(0,12,true,cursor).contains("error")&&invalid.sram()==corrupt&&cursor==42,"Invalid position rejected before writes");
 std::ifstream fixture(project+"/tests/tactical-turn-start.json");const auto entry=Json::parse(fixture);
 OriginalSession session(rom),loaded(rom);
 auto replay=[&](){const auto save=session.save();const auto error=loaded.restore(save);check(error.empty()&&loaded.save()==save,"Every withdrawal step replays exactly: "+error);};
 check(session.restore(entry).empty()&&session.end_tactical_turn().empty(),"Begin human defender turn");
 auto start=session.save();check(session.begin_tactical_retreat(1).empty(),"Begin ordinary withdrawal");replay();
 check(!session.end_tactical_turn().empty()&&!session.move_tactical(1,0).empty(),"Pending retreat holds other actions");
 check(session.cancel_tactical_retreat().empty()&&session.save()["sram"]==start["sram"]&&session.save()["battle"]["tactics"]["points"]==start["battle"]["tactics"]["points"],"Cancellation changes neither SRAM nor mobility");replay();
 for(int slot:{1,0,2,3}){
  const auto before=session.save();const int points=before["battle"]["tactics"]["points"],id=before["sram"][0xdaa+slot*2];
  check(session.begin_tactical_retreat(slot).empty(),"Select defending officer");replay();
  check(session.confirm_tactical_retreat().empty(),"Confirm defending withdrawal");const auto save=session.save();replay();
  const auto &t=save["battle"]["tactics"];check(t["points"]==points-1&&t["retreat"]["outcomes"][0]["returned"]==true,"Success costs exactly one");
  check(save["sram"][0xdaa+slot*2]==255&&save["random_cursor"]==before["random_cursor"],"Only selected defender departs without a random draw");
  for(int i=0;i<11;++i)check(save["sram"][0xdc2+i*3]==before["sram"][0xdc2+i*3],"Attacker ledger retained");
  if(id==2)check(save["sram"][0xdae]==168&&save["sram"][0xdb0]==128,"Ruler retreat does not evacuate other defenders");
  check(session.finish_tactical_retreat().empty(),"Acknowledge result");replay();
 }
 auto final=session.save();check(final["battle"]["tactics"]["turn_boundary"]=="battle_result"&&final["battle"]["tactics"]["turn_reason"]==2,"Last defender reaches D39D reason 02");
 check(final["sram"][12*36]==entry["sram"][12*36]&&final["sram"][0xdc2]==entry["sram"][0xdc2],"Unported final settlement never clears ownership or attacker ledger");
 check(!session.end_tactical_turn().empty()&&!session.begin_tactical_retreat(0).empty()&&session.save()==final,"Final boundary cannot be bypassed");
 auto forged=final;forged["battle"]["tactics"]["last_retreat"]["outcomes"][0]["city"]=0;
 check(!loaded.restore(forged).empty()&&loaded.save()==final,"Forged destination rejected atomically");
 for(const auto &layout:{"full","none"}){
  auto trial=entry;
  for(int city=0;city<30;++city)if(city!=12&&(trial["sram"][city*36].get<int>()&7)==2){
   if(std::string(layout)=="none")trial["sram"][city*36]=(trial["sram"][city*36].get<int>()&248)|4;
   else for(int i=0;i<12;++i)trial["sram"][city*36+16+i]=100+i;
  }
  trial["battle"]["tactics"]["deployment_sram"]=trial["sram"];
  check(session.restore(trial).empty()&&session.end_tactical_turn().empty(),"Controlled no-capacity session");
  start=session.save();check(session.begin_tactical_retreat(1).empty()&&session.confirm_tactical_retreat().empty(),"No-capacity withdrawal produces result");
  auto failed=session.save();check(failed["sram"]==start["sram"]&&failed["random_cursor"]==start["random_cursor"]&&failed["battle"]["tactics"]["points"]==start["battle"]["tactics"]["points"]&&!failed["battle"]["tactics"]["retreat"]["outcomes"][0]["departed"].get<bool>(),"Failure preserves complete SRAM, cursor and mobility");replay();
  check(session.finish_tactical_retreat().empty()&&session.adjust_tactical_formation(1,0).empty(),"Failed withdrawal returns to defender commands");replay();
 }
 {
  // Full defending ledger tests the save schema's twelfth slot and BEFE's
  // first-eleven count using a valid deployment, not forged action history.
  auto trial=entry;std::vector<std::uint8_t> bytes=entry["sram"].get<std::vector<std::uint8_t>>();
  OriginalState geometry(rom,bytes);const auto area=geometry.deployment_area(12,true);
  for(int i=0;i<12;++i)if(bytes[0xdaa+i*2+1]<160)bytes[0xe1a+bytes[0xdaa+i*2+1]]=bytes[0xf1a+bytes[0xdaa+i*2+1]];
  Json roster=Json::array();int cell=0;
  for(int i=0;i<12;++i){
   while(cell<160&&(!area[cell].get<bool>()||(bytes[0xe1a+cell]&16)))++cell;
   check(cell<160,"Twelve legal defender deployment cells");
   const int id=i==0?2:150+i;bytes[12*36+16+i]=id;bytes[0xdaa+i*2]=id;bytes[0xdab+i*2]=cell;bytes[0xe1a+cell]=16|i;roster.push_back(id);++cell;
  }
  trial["sram"]=bytes;trial["battle"]["defenders"]=roster;trial["battle"]["tactics"]["deployment_sram"]=bytes;
  check(session.restore(trial).empty()&&session.end_tactical_turn().empty(),"Twelve defenders enter a legal tactical turn");
  check(session.begin_tactical_retreat(11).empty(),"Twelfth defender is selectable");replay();
  check(session.confirm_tactical_retreat().empty()&&session.finish_tactical_retreat().empty(),"Twelfth defender withdrawal completes");replay();
  check(session.restore(trial).empty()&&session.end_tactical_turn().empty(),"Restore full defenders for count boundary");
  for(int i=0;i<11;++i)check(session.begin_tactical_retreat(i).empty()&&session.confirm_tactical_retreat().empty()&&session.finish_tactical_retreat().empty(),"Withdraw first eleven defenders");
  const auto save=session.save();check(save["sram"][0xdc0]==161&&save["battle"]["tactics"]["turn_boundary"]=="battle_result","Original first-eleven quirk keeps twelfth defender in unresolved result");replay();
 }
 std::cout<<"ROM defender withdrawal "<<rows.size()<<" complete SRAM/RNG samples; cancellation, capacity, ruler, final boundary and strict replay passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
