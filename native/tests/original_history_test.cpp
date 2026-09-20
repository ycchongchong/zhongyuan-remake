#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool v,const std::string&m){if(!v)throw std::runtime_error(m);}
Json json(const std::string&p){std::ifstream f(std::filesystem::u8path(p));check(bool(f),p);return Json::parse(f);}
std::vector<std::uint8_t> read(const std::string&p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {std::istreambuf_iterator<char>(f),{}};}
Json game_state(Json saved){if(saved["battle"].is_object()&&saved["battle"].contains("tactics")){saved["battle"]["tactics"].erase("moves");saved["battle"]["tactics"].erase("history");}return saved;}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string p=ZHONGYUAN_PROJECT_DIR;OriginalSession game(rom),loaded(rom),legacy(rom);
 auto call=[&](std::string e){check(e.empty(),e);};auto replay=[&](){auto saved=game.save();call(loaded.restore(saved));check(loaded.save()==saved,"Segmented history restores exactly");};
 const auto start=json(p+"/tests/long-clash-start.json");call(game.restore(start));check(game.save()==start,"Old unsegmented save unchanged");
 check(!game.battle_history_needs_archive()&&!game.archive_battle_history().empty()&&game.save()==start,"Cannot create premature empty archives");
 const auto ready=json(p+"/tests/long-clash-archive-ready.json");call(game.restore(ready));call(legacy.restore(ready));
 call(game.perform_battle_action([](auto &s){return s.advance_clash();}));call(legacy.advance_clash());
 auto full=game.save();check(full==legacy.save()&&full["battle"]["tactics"]["moves"].size()==768&&game.battle_history_needs_archive(),"Pre-boundary gameplay and event history identical");
 check(!game.perform_battle_action([](auto &s){return s.begin_clash();}).empty()&&game.save()==full,"Failed action cannot commit an archive");
 call(game.perform_battle_action([](auto &s){return s.advance_clash();}));call(legacy.advance_clash());auto split=game.save();
 check(split["battle"]["tactics"]["history"].size()==1&&split["battle"]["tactics"]["history"][0]==full["battle"]["tactics"]["moves"]&&split["battle"]["tactics"]["moves"].size()==1,"Exact event prefix archived without loss or duplication");
 check(game_state(split)==game_state(legacy.save()),"Archiving preserves every game-state field including RNG/runtime");replay();
 OriginalSession explicit_archive(rom);call(explicit_archive.restore(full));call(explicit_archive.archive_battle_history());auto empty=explicit_archive.save();check(empty["battle"]["tactics"]["moves"].empty(),"Explicit archive may leave empty active segment");call(loaded.restore(empty));check(loaded.save()==empty,"Empty active segment replay");
 // Continue the same controlled long fight, with genuine AI orders, damage,
 // defeat and tactical return. No repeated dummy actions make it long.
 call(game.restore(start));int steps=0;
 for(;steps<5000;++steps){const auto saved=game.save();const auto &a=saved["battle"]["tactics"]["attack"];const std::string stage=a["stage"];
  if(stage=="clash_result")break;
  call(game.perform_battle_action([&](auto &s)->std::string{
   if(stage=="clash_orders"||stage=="clash_running")return s.advance_clash();
   if(stage=="clash_boundary"){
    if(a["boundary"]=="general_defeat")return s.advance_clash_defeat();
    if(a["boundary"]=="retreat")return s.advance_clash_retreat();
    if(a["boundary"]=="duel")return s.begin_duel();
    if(a["boundary"]=="ai_scratch"){auto e=s.resume_clash_strategy();return e.empty()?e:s.recover_clash_strategy();}
   }
   if(stage=="clash_defeat")return s.advance_clash_defeat();if(stage=="clash_retreat")return s.advance_clash_retreat();
   if(stage=="surrender_notice"||stage=="surrender_accepted")return s.advance_clash_surrender();
   if(stage=="duel_orders"){int side=a["duel"]["side"].get<int>()?1:0;return a["clash"]["players"][side]!=0?s.choose_duel_command(1):s.advance_duel();}
   if(stage.rfind("duel_",0)==0)return s.advance_duel();return "Unexpected stage "+stage;
  }));
  if(steps==1100)check(game.save()==json(p+"/tests/long-clash-segmented.json"),"Independent generator matches >1024-event continuation");
 }
 check(steps==3358&&game.save()==json(p+"/tests/long-clash-result.json"),"3358 actual combat steps produce exact recorded result");replay();const auto result=game.save();
 check(result["battle"]["tactics"]["history"].size()==4&&result["battle"]["tactics"]["attack"]["result"]["officer"]==145,"Four archived segments retain real player defeat");
 for(int kind=0;kind<9;++kind){auto forged=result;auto &t=forged["battle"]["tactics"];
  if(kind==0)t.erase("history");
  if(kind==1)t["history"][0][0]["slot"]=240;
  if(kind==2)std::swap(t["history"][0],t["history"][1]);
  if(kind==3)t["history"][0].erase(t["history"][0].begin());
  if(kind==4)t["history"]=Json::object();
  if(kind==5)t["history"]=Json::array();
  if(kind==6)t["history"]=Json(std::vector<Json>(257,t["history"][0]));
  if(kind==7)t["history"][0]=Json(std::vector<Json>(1025,t["history"][0][0]));
  if(kind==8)t["moves"]=Json(std::vector<Json>(1025,t["moves"][0]));
  check(!loaded.restore(forged).empty()&&loaded.save()==result,"Malformed or altered archived history rejected atomically");
 }
 call(game.perform_battle_action([](auto &s){return s.finish_clash_result();}));replay();check(!game.save()["battle"]["tactics"].contains("attack"),"Long clash returns to tactical map");
 // Independent round-trip stress exceeds the former file-size ceiling using
 // legal formation commands. This is storage coverage, not a natural battle.
 call(game.restore(json(p+"/tests/computer-plan-start.json")));call(game.begin_tactics());
 for(int i=0;i<24580;++i)call(game.perform_battle_action([](auto &s){return s.adjust_tactical_formation(0,0);}));
 auto large=game.save();check(large.dump().size()>1024*1024&&large.dump().size()<OriginalSession::MAX_SAVE_BYTES&&large["battle"]["tactics"]["history"].size()==32,"Valid >1MiB session has32 fully replayable archives");replay();
 // Human commander retreat closes the battle and releases all its history.
 call(game.perform_battle_action([](auto &s){return s.begin_tactical_retreat(2);}));call(game.perform_battle_action([](auto &s){return s.confirm_tactical_retreat();}));call(game.perform_battle_action([](auto &s){return s.finish_tactical_retreat();}));
 check(game.save()["battle"].is_null(),"Battle completion releases archived history");replay();
 std::cout<<"Original history:3358-step clash,4 combat archives,24580 legal formation actions,32 storage archives,>1MiB round trip,atomic failures and tampering passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
