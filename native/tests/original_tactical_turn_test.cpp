#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string &m){if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
 std::ifstream f(base+"tactical-turns.json");auto turns=Json::parse(f);
 for(const auto &c:turns){
  auto raw=rom.initial_sram(0);raw[0xde3]=0x42;raw[0x438+2*8]&=223;raw[0x438+4*8]&=223;
  for(int i=0;i<12;++i)raw[0xdaa+2*i]=i<c["count"].get<int>()?12:255;
  for(int i=0;i<11;++i){raw[0xdc2+3*i]=i<c["count"].get<int>()?145:255;raw[0xdc4+3*i]=i==0?128:0;}
  const auto ending=c.value("ending",std::string{});
  if(ending=="defender_ruler"||ending=="both_rulers")raw[0x438+2*8]|=32;
  if(ending=="attacker_ruler"||ending=="both_rulers")raw[0x438+4*8]|=32;
  if(ending=="no_defenders"||ending=="last_defender")for(int i=0;i<12;++i)raw[0xdaa+i*2]=ending=="last_defender"&&i==11?12:255;
  if(ending=="no_commander")for(int i=0;i<11;++i)raw[0xdc4+i*3]=0;
  if(ending=="no_attackers")for(int i=0;i<11;++i)raw[0xdc2+i*3]=255;
  OriginalState state(rom,raw);auto context=c["context"];context["status"]=c["status_before"];
  auto actual=c["mode"]=="end"?state.tactical_end_turn(context):state.tactical_handover(context);
  auto expected=c["output"];expected["status"]=c["status_after"];
  check(actual==expected&&state.sram()==raw&&c["cursor_before"]==c["cursor_after"],"ROM turn sample "+c["id"].dump()+": "+actual.dump());
 }
 std::ifstream commands(base+"defender-commands.json");auto cases=Json::parse(commands);
 for(const auto &c:cases){
  const auto raw=read(base+c["before"].get<std::string>());OriginalState state(rom,raw);int points=c["points_before"],slot=c["selected"];const auto mode=c["mode"];
  if(mode=="move"){
   const auto error=state.tactical_step(slot,c["direction"],points,0);
   const bool moved=raw[0xdab+slot*2]!=read(base+c["after"].get<std::string>())[0xdab+slot*2];
   check(error.empty()==moved,"ROM defender move acceptance "+c["id"].dump());
  }else if(mode=="formation")check(state.tactical_formation(slot,c["direction"],0).empty(),"Defender formation");
  else if(mode=="scout"){
   const int cell=0x10+c["target_slot"].get<int>()*11;auto result=state.tactical_scout(slot,cell,points,0);
   check(!result.contains("error")==c["accepted"].get<bool>(),"Defender scouting acceptance");
   if(!result.contains("error"))check(result["officer"]==c["officer"],"Defender scouts attacking officer");
  }else{
   auto result=state.tactical_attack(slot,c["direction"],points,0);
   check(!result.contains("error")==c["accepted"].get<bool>(),"Defender attack acceptance "+c["id"].dump());
   if(!result.contains("error"))check(result["attacker"]==c["attack"]["0x688"]&&result["defender"]==c["attack"]["0x689"]&&result["terrain_pair"]==c["attack"]["0x68e"]&&result["defender_token"]==c["attack"]["0x692"],"Defender counterattack preserves original officer order and terrain pair");
  }
  check(points==c["points_after"]&&state.sram()==read(base+c["after"].get<std::string>()),"Defender complete SRAM and mobility sample "+c["id"].dump());
 }
 {
  OriginalState state(rom,rom.initial_sram(0));auto bad=turns[0]["context"];bad["status"]=Json(std::vector<int>(24,0));bad["side"]=1;
  check(state.tactical_handover(bad).contains("error"),"Invalid tactical side is rejected");
  bad["side"]=0;bad["status"][23]=std::uint64_t(1)<<32;
  check(state.tactical_end_turn(bad).contains("error"),"Oversized status is rejected before narrowing");
 }
 OriginalSession session(rom),loaded(rom);
 std::ifstream fixture(project+"/tests/tactical-turn-start.json");const auto entry=Json::parse(fixture);
 check(session.restore(entry).empty()&&session.save()==entry,"Legacy two-player tactical entry restores");
 check(session.end_tactical_turn().empty(),"Pass to human defender");auto save=session.save();
 check(save["battle"]["tactics"]["side"]==0&&save["battle"]["tactics"]["round"]==0&&save["battle"]["tactics"]["carry"]==160,"End-turn banks at most ten, defender acts in same round");
 check(loaded.restore(save).empty()&&loaded.save()==save,"Defender turn restores exactly");
 check(session.begin_tactical_retreat(0).empty()&&session.cancel_tactical_retreat().empty(),"Defender can open and cancel withdrawal");
 check(session.adjust_tactical_formation(0,0).empty()&&session.scout_tactical(0,0).empty(),"Defender formation and enemy scouting are connected");
 const auto scouting=session.save();check(!session.end_tactical_turn().empty()&&session.save()==scouting,"Cannot skip scouting report by ending turn");
 check(session.close_tactical_scout().empty(),"Close defending report");
 bool moved=false;for(int d=0;d<4&&!moved;++d)moved=session.move_tactical(0,d).empty();check(moved,"Defender can move from deployment");
 save=session.save();check(loaded.restore(save).empty()&&loaded.save()==save,"Defending commands replay with the active ledger");
 check(session.end_tactical_turn().empty(),"Defender ends first round");save=session.save();
 check(save["battle"]["tactics"]["side"]==128&&save["battle"]["tactics"]["round"]==1&&save["battle"]["tactics"]["points"]==30,"Attacker receives base mobility plus saved ten");
 for(int i=0;i<20;++i){if(i==19)check(session.select_tactical_unit(1).empty(),"Free selection before final handover");check(session.end_tactical_turn().empty(),"Continue two-player rounds");save=session.save();check(loaded.restore(save).empty()&&loaded.save()==save,"Every handover restores");}
 check(save["battle"]["tactics"]["turn_boundary"]=="time_limit"&&save["battle"]["tactics"]["round"]==11&&save["battle"]["tactics"]["selected"]==1,"Original round eleven enters unresolved time limit result");
 check(!session.end_tactical_turn().empty()&&!session.move_tactical(0,0).empty()&&!session.attack_tactical(0,0).empty()&&session.save()==save,"Cannot skip final battle boundary");
 auto forged=save;forged["battle"]["tactics"]["carry"]=255;check(!loaded.restore(forged).empty()&&loaded.save()==save,"Forged carry rejected atomically");
 std::ifstream single(project+"/tests/tactical-turn-single.json");check(session.restore(Json::parse(single)).empty()&&session.end_tactical_turn().empty(),"Single player hands over to computer");save=session.save();
 check(save["battle"]["tactics"]["turn_boundary"]=="computer"&&!session.end_tactical_turn().empty()&&!session.move_tactical(0,0).empty()&&session.save()==save,"Computer turn is never silently skipped");
 check(loaded.restore(save).empty()&&loaded.save()==save,"Pending computer turn saves exactly");
 {
  std::ifstream counterattack(project+"/tests/tactical-defender-clash.json");const auto entry=Json::parse(counterattack);
  check(session.restore(entry).empty()&&session.save()==entry,"Defender counterattack follows legal movement history");
  const auto &clash=entry["battle"]["tactics"]["attack"]["clash"];const int army=clash["first"]==12?0:1;
  check(clash["tactical_side"]==0&&clash["players"][army]==2&&clash["factions"][army]==2,"Defender remains player two after terrain swaps the clash armies");
  check(!session.end_tactical_turn().empty()&&session.save()==entry,"Cannot end tactical turn during clash");
  for(int i=0;i<3;++i)check(session.cycle_clash_order(army,3).empty(),"Select defending officer surrender order");
  check(session.request_clash_surrender().empty()&&session.answer_clash_surrender(true).empty(),"Defender counterattack can enter ordinary surrender");
  for(int i=0;i<2;++i){check(session.advance_clash_surrender().empty(),"Advance counterattacking defender surrender");save=session.save();check(loaded.restore(save).empty()&&loaded.save()==save,"Defender clash result replays exactly");}
  check(save["battle"]["tactics"]["attack"]["result"]["can_continue"]==true&&save["battle"]["tactics"]["side"]==0&&save["battle"]["tactics"]["points"]==entry["battle"]["tactics"]["points"],"Counterattack settlement keeps defending turn and mobility");
  check(session.finish_clash_result().empty()&&session.adjust_tactical_formation(0,0).empty(),"Remaining defenders resume their own tactical actions");
  save=session.save();check(loaded.restore(save).empty()&&loaded.save()==save,"Post-counterattack defender map saves exactly");
 }
 std::cout<<"ROM turns "<<turns.size()<<", defender commands "<<cases.size()<<"; session rounds, carry, side guards and replay passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
