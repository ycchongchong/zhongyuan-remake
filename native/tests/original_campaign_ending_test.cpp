// Untouched dual-player start, fixed zero clock, passive turns/retreat policy.
// Native acceptance only, not a synchronized original-ROM or unification playthrough.
#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
std::vector<std::uint8_t> read(const std::string&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){if(argc!=2)return 1;OriginalRom rom(read(argv[1]));OriginalSession game(rom),loaded(rom);game.start(4,0,5);int battles=0, failures=0;
 auto fixture=[&](const std::string&name){std::ifstream f(std::string(ZHONGYUAN_PROJECT_DIR)+"/tests/natural-dual-"+name+".json");return Json::parse(f);};
try{for(int step=0;step<7300;++step){const auto saved=game.save();const std::string phase=saved["phase"];std::string action,error;
 auto stop=[&](const std::string &why){throw std::runtime_error(why);};
 if(phase=="ending"){
  if(step!=7269||battles!=7||failures!=1||saved["ending"]["kind"]!="year_limit"||saved["sram"][0xd85]!=250||saved["sram"][0xd87]!=1)stop("Unexpected natural ending");
  if(saved!=fixture("year-limit"))stop("Terminal fixture differs");
  auto e=loaded.restore(saved);if(!e.empty()||loaded.save()!=saved)stop("Terminal strict replay "+e);
  if(game.end_turn().empty())stop("Terminal player turn accepted");
  if(game.advance()["phase"]!="ending"||game.save()!=saved)stop("Terminal transition mutated state");
  std::cout<<"PASS:7269 natural operations,7 invasions,first human defeated at3169,250/1 year limit,strict replay and terminal lock\n";return 0;
 }
 if(phase=="ai_turn"){auto r=game.advance();if(r.contains("error"))stop(r["error"]);continue;}
 if(phase=="player_commands"){action="end_turn";error=game.end_turn();}
 else if(phase=="battle"){
  const auto &b=saved["battle"];
  if(!b.contains("deployment")){++battles;action="prepare_deployment";error=game.prepare_deployment();}
  else if(b["deployment"]["current"]!=-1){action="confirm_deployment";error=game.confirm_deployment();}
  else if(!b.contains("tactics")){action="begin_tactics";error=game.begin_tactics();}
  else error=game.perform_battle_action([&](auto&s)->std::string{
   const auto &t=b["tactics"];
   const bool failed_retreat=t.contains("last_retreat")&&!t["last_retreat"]["outcomes"].empty()&&!t["last_retreat"]["outcomes"][0].value("departed",false);
   if(t.contains("human_failure")){
    if(!t["human_failure"]["can_continue"].get<bool>()||step!=3169||++failures!=1||t["human_failure"]["loser"]!=4||saved!=fixture("first-defeat"))return "Unexpected first player failure";
    auto e=loaded.restore(saved);if(!e.empty()||loaded.save()!=saved)return "First player failure strict replay "+e;
    action="finish_human_failure";return s.finish_human_failure();
   }
   if(t.contains("attack")){
    const auto&a=t["attack"];const std::string stage=a["stage"];action=stage;
    if(stage=="clash_ready")return s.begin_clash();
    if(stage=="clash_orders"&&!failed_retreat)for(int side=0;side<2;++side)if(a["clash"]["players"][side]!=0&&a["clash"]["runtime"]["orders"][side*4+3]==0){action="retreat_general_order";return s.cycle_clash_order(side,3);}
    if(stage=="clash_orders"||stage=="clash_running")return s.advance_clash();
    if(stage=="clash_boundary"){
     if(a["boundary"]=="ai_scratch"){auto e=s.resume_clash_strategy();return e.empty()?e:s.recover_clash_strategy();}
     if(a["boundary"]=="retreat")return s.advance_clash_retreat();
     if(a["boundary"]=="general_defeat")return s.advance_clash_defeat();
     if(a["boundary"]=="duel")return s.begin_duel();
    }
    if(stage=="clash_retreat")return s.advance_clash_retreat();
    if(stage=="clash_defeat")return s.advance_clash_defeat();
    if(stage=="clash_result"){
     if(s.snapshot()["battle"].value("human_failure_available",false))return s.begin_human_failure();
     if(s.snapshot()["battle"].value("ruler_defeat_result_available",false))return s.begin_ruler_defeat_result();
     if(s.snapshot()["battle"].value("commander_defeat_result_available",false))return s.begin_commander_defeat_result();
     if(s.snapshot()["battle"].value("army_defeat_result_available",false))return s.begin_defender_defeat_result();
     return s.finish_clash_result();
    }
    if(stage=="surrender_notice"||stage=="surrender_accepted")return s.advance_clash_surrender();
    if(stage=="duel_orders"){int side=a["duel"]["side"].get<int>()?1:0;return a["clash"]["players"][side]!=0?s.choose_duel_command(1):s.advance_duel();}
    if(stage=="duel_confirm")return s.answer_duel_surrender(false);
    if(stage.rfind("duel_",0)==0)return s.advance_duel();
    return "Unhandled clash "+stage;
   }
   if(t.contains("retreat")){action="finish_retreat";return s.finish_tactical_retreat();}
   if(t.value("turn_boundary",std::string{})=="battle_result"){
    int stage=t.value("settlement",Json::object()).value("stage",-1);
    action="settlement";if((t.value("turn_reason",0)&128)&&t.value("annexation",Json::object()).value("next_city",0)<30&&(stage==4||stage==29))return s.advance_ruler_annexation();return stage==4||stage==13||stage==17||stage==29?s.finish_withdrawal_result():s.advance_withdrawal_result();
   }
   if(t.value("turn_boundary",std::string{})=="time_limit"){action="time_limit";return s.advance_time_limit_result();}
   if(t.value("turn_boundary",std::string{})=="computer"){
    action="computer_attack";return t.value("attacker_ai",Json::object()).value("stage",std::string{})=="held_retreat"?s.resume_computer_attacker_retreat():s.advance_computer_attack();
   }
   if(failed_retreat){action="end_tactical_turn";return s.end_tactical_turn();}
   action="human_retreat";auto e=s.begin_tactical_retreat(t["selected"]);return e.empty()?s.confirm_tactical_retreat():e;
  });
 }else stop("Unhandled phase "+phase);
 if(!error.empty())stop(action+": "+error);
 if(step%1000==0){error=loaded.restore(game.save());if(!error.empty()||loaded.save()!=game.save())stop("Replay "+error);}
}throw std::runtime_error("No ending in7300 actions");}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
