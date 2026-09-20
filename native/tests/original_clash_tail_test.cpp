#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string&m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string&p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string&p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
int main(int argc,char**argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string p=ZHONGYUAN_PROJECT_DIR,base=p+"/reference/fixtures/";
 const auto rows=json(base+"clash-reset-strategy.json");int ambiguous=0;
 for(const auto&r:rows){
  OriginalState state(rom,read(base+r["before"].get<std::string>()));const auto before=state.sram();std::uint8_t cursor=r["cursor_before"];
  auto exact=state.clash_strategy(r["units"],r["orders"],r["context"],cursor);
  check(exact==r["output"]&&cursor==r["cursor_after"]&&state.sram()==before,"Untouched original RAM strategy sample "+r["id"].dump());
  auto unknown=r["context"];unknown.erase("tail");cursor=r["cursor_before"];
  ambiguous+=state.resolve_clash_strategy(r["units"],r["orders"],unknown,cursor).contains("error");
 }
 check(rows.size()==155&&ambiguous>0,"Reference includes genuinely ambiguous strategies");
 OriginalSession game(rom),loaded(rom);auto call=[&](std::string e){check(e.empty(),e);};
 auto replay=[&](){auto saved=game.save();call(loaded.restore(saved));check(loaded.save()==saved,"Exact recovery history replay");};
 auto untouched=game.save();check(!game.recover_clash_strategy().empty()&&game.save()==untouched,"No recovery outside clash");
 const auto entry=json(p+"/tests/clash-tail-boundary.json");call(game.restore(entry));check(game.save()==entry,"Legacy ambiguous save remains byte identical");
 check(!game.resume_clash_strategy().empty()&&game.save()==entry,"Legacy resolver still holds ambiguous result atomically");
 call(game.recover_clash_strategy());const auto recovered=game.save();const auto &runtime=recovered["battle"]["tactics"]["attack"]["clash"]["runtime"];
 check(runtime["tail"]==Json::array({0,0,0})&&runtime["phase"]==4&&!runtime.contains("boundary"),"Derived reset memory is retained only after explicit recovery event");
 check(recovered["sram"]==entry["sram"]&&runtime["units"]==entry["battle"]["tactics"]["attack"]["clash"]["runtime"]["units"]&&recovered["battle"]["tactics"]["points"]==entry["battle"]["tactics"]["points"],"Recovery preserves armies, resources and tactical budget");
 const auto &ref=rows.back();check(ref["id"]==-1&&recovered["random_cursor"]==ref["cursor_after"],"Session RNG matches original decision entry");
 for(const auto&kv:ref["output"].items())check(runtime[kv.key()]==kv.value(),"Session decision matches original result");
 check(recovered["battle"]["tactics"]["moves"].back()["kind"]=="recover_clash_strategy","Distinct replay event");replay();
 check(!game.recover_clash_strategy().empty()&&!game.resume_clash_strategy().empty()&&game.save()==recovered,"Recovery cannot run twice");
 for(int type=0;type<5;++type){auto forged=recovered;
  if(type==0)forged["battle"]["tactics"]["attack"]["clash"]["runtime"]["tail"][0]=1;
  if(type==1)forged["battle"]["tactics"]["attack"]["clash"]["runtime"].erase("tail");
  if(type==2)forged["battle"]["tactics"]["moves"].back()["cursor"]=std::uint64_t(1)<<32;
  if(type==3)forged["battle"]["tactics"]["moves"].back()["kind"]="resume_clash_strategy";
  if(type==4)forged["battle"]["tactics"]["moves"].push_back(forged["battle"]["tactics"]["moves"].back());
  check(!loaded.restore(forged).empty()&&loaded.save()==recovered,"Forged or duplicated recovery rejected atomically");
 }
 int steps=0;for(;steps<600;++steps){auto a=game.save()["battle"]["tactics"]["attack"];if(a["stage"]!="clash_running")break;call(game.advance_clash());}
 check(steps==112&&game.save()["battle"]["tactics"]["attack"]["boundary"]=="general_defeat","Recovered computer continues to actual hit/defeat");replay();
 call(game.advance_clash_defeat());call(game.advance_clash_defeat());check(game.save()["battle"]["tactics"]["attack"]["stage"]=="clash_result","Defeat reaches existing result flow");replay();
 call(game.begin_defender_defeat_result());
 for(int i=0;i<45;++i){call(game.advance_withdrawal_result());if(game.save()["battle"]["tactics"]["settlement"]["stage"]==29)break;}
 replay();call(game.finish_withdrawal_result());replay();const auto final=game.save();
 check(final["battle"].is_null()&&final["phase"]=="player_commands"&&final["sram"][0xd8b]==4&&(final["sram"][21*36].get<int>()&7)==3,"Recovered battle settles city and resumes next ruler");
 std::cout<<"Original clash tail:155 untouched-memory decisions ("<<ambiguous<<" formerly ambiguous),112 combat steps,defeat,settlement,replay and forged saves passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
