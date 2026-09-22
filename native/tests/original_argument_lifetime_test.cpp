#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
int checks=0;
void check(bool value,const std::string &message){++checks;if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string &path){std::ifstream f(std::filesystem::u8path(path),std::ios::binary);if(!f)throw std::runtime_error(path);return {std::istreambuf_iterator<char>(f),{}};}
Json json(const std::string &path){return Json::parse(read(path));}
// Controlled entry metadata for old reference fixtures whose subsequent
// events predate any command-argument write. This is not an automatic migration.
Json controlled_entry(Json saved,int argument){
 saved["format"]="native-original-v3";saved["command_argument"]=argument;
 if(saved["battle"].is_object()&&saved["battle"].contains("tactics"))saved["battle"]["tactics"]["entry_argument"]=argument;
 return saved;
}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM path");OriginalRom rom(read(argv[1]));
 const std::string root=ZHONGYUAN_PROJECT_DIR,base=root+"/reference/fixtures/";
 OriginalSession game(rom),loaded(rom);
 auto call=[&](const std::string &e){check(e.empty(),e);};
 auto replay=[&](){const auto saved=game.save();call(loaded.restore(saved));check(loaded.save()==saved,"All v3 state and history round trip");};
 for(int ruler=0;ruler<6;++ruler)for(int difficulty=0;difficulty<3;++difficulty){
  game.start(ruler,difficulty);const auto s=game.save();
  check(s["format"]=="native-original-v3"&&s["command_argument"]==0,"Fresh power-on command byte starts at zero");replay();
 }
 const auto initial=game.save();
 for(const auto &value:Json::array({-1,256,1.5,"3",nullptr,std::uint64_t(1)<<40})){
  auto bad=initial;bad["command_argument"]=value;
  check(!loaded.restore(bad).empty()&&loaded.save()==initial,"Invalid command byte rejected atomically");
 }
 auto bad=initial;bad.erase("command_argument");check(!loaded.restore(bad).empty()&&loaded.save()==initial,"v3 cannot omit the byte");
 bad=initial;bad["format"]="native-original-v2";check(!loaded.restore(bad).empty()&&loaded.save()==initial,"Legacy save cannot borrow a supplied byte");
 const auto ai_cases=json(base+"ai.json");int wars=0;
 for(const auto &row:ai_cases){
  const int outcome=row["expected"].value("battle_result",0);if(outcome!=1&&outcome!=2)continue;
  const auto raw=read(base+row["before"].get<std::string>());
  if((raw[0xd89]&7)==raw[0xd8b]||((raw[0xd8a]&128)&&(raw[0xd8a]&7)==raw[0xd8b]))continue;
  game.start(4,0);auto entry=game.save();entry["sram"]=read(base+row["before"].get<std::string>());
  entry["ai"]=row["runtime"];entry["ai"]["done"]=false;entry["phase"]="ai_turn";
  entry["random_cursor"]=entry["ai"]["random_cursor"];entry["command_argument"]=201;
  call(game.restore(entry));check(!game.advance().contains("error"),"Original strategic battle completes");
  check(game.save()["command_argument"]==row["expected"]["winner"],"C6C3 writes the original automatic-war winner");replay();++wars;
 }
 check(wars>0,"Reference automatic-war writer exercised");
 const auto natural=json(root+"/tests/natural-argument-lifetime.json");call(game.restore(natural));
 check(game.save()==natural&&natural["command_argument"]==3&&natural["battle"]["tactics"]["entry_argument"]==3,"Natural first AI turn retains pre-battle byte through three archived clash segments");
 check(natural["battle"]["tactics"]["round"]==0&&!natural["battle"]["tactics"].contains("computer_cursor"),"Regression is the first defender turn, not a within-battle recovery");
 const auto references=json(base+"natural-argument-lifetime.json")["cases"];
 for(const auto &r:references){
  OriginalState state(rom,natural["sram"].get<std::vector<std::uint8_t>>());
  auto result=state.tactical_role_return(r["argument"],24,natural["battle"]["tactics"]["status"]);
  check(!result.contains("error")&&state.sram()==read(base+r["after"].get<std::string>())&&result["points_after"]==r["points_after"],"All four controlled original C7EF outcomes, full8192-byte comparison");
 }
 check(game.can_continue_computer_role()&&game.save()==natural,"Availability is read only");call(game.continue_computer_role());
 const auto after=game.save();
 check(after["sram"]==read(base+references[3]["after"].get<std::string>())&&after["battle"]["tactics"]["points"]==21&&after["command_argument"]==3,"Natural resume matches original right movement and preserves byte");replay();
 for(int kind=0;kind<4;++kind){bad=after;
  if(kind==0)bad["command_argument"]=0;
  if(kind==1)bad["battle"]["tactics"].erase("entry_argument");
  if(kind==2)bad["battle"]["tactics"]["entry_argument"]=256;
  if(kind==3)bad["battle"]["tactics"]["entry_argument"]=0;
  check(!loaded.restore(bad).empty()&&loaded.save()==after,"Changed final byte or invalid/inconsistent entry cannot bypass strict replay");
 }
 // A human move, handover and no-command AI assessment preserve any byte.
 const auto strategy=controlled_entry(json(root+"/tests/computer-strategy-start.json"),201);
 call(game.restore(strategy));check(game.save()==strategy,"Non-writers preserve full byte, not only low two bits");
 call(game.evaluate_computer_strategy());
 check(game.save()["command_argument"]==7&&game.save()["battle"]["tactics"]["computer_strategy"]["argument"]==7,"Original strategy target0/strategy7 overwrites command byte");replay();
 call(game.execute_computer_strategy());check(game.save()["command_argument"]==7,"Strategy effects preserve selected argument");replay();
 call(game.restore(controlled_entry(json(root+"/tests/computer-fort-start.json"),201)));call(game.plan_computer_tactics());
 check(game.save()["command_argument"]==game.save()["battle"]["tactics"]["computer_plan"]["direction"],"Original fort movement overwrites command byte");replay();
 // A new battle begins with campaign metadata, without inventing it on load.
 auto deployment=controlled_entry(json(root+"/tests/computer-plan-start.json"),254);
 call(game.restore(deployment));call(game.begin_tactics());
 check(game.save()["battle"]["tactics"]["entry_argument"]==254,"Deployment carries strategic byte into immutable replay entry");replay();
 const auto legacy=json(root+"/tests/computer-role-unknown.json");call(game.restore(legacy));
 check(game.save()==legacy&&!game.can_continue_computer_role(),"v2 unknown history is unchanged, without guessed migration");
 game.start(4,0);check(game.save()["command_argument"]==0&&game.save()["format"]=="native-original-v3","Fresh game clears legacy mode and previous campaign state");replay();
 const auto blocked=json(root+"/tests/natural-blocked-fort.json");call(game.restore(blocked));
 call(game.continue_computer_scan());const auto stopped=game.save();
 const auto original=json(base+"natural-blocked-fort.json");const auto &plan=stopped["battle"]["tactics"]["computer_plan"];
 check(original["phase"]==5&&original["counter"]==1&&original["points_after"]==3,"Original rejected command returns to assessment");
 check(plan["blocked"]==true&&plan["slot"]==original["slot"]&&stopped["command_argument"]==original["argument"],"Insufficient-mobility fort decision preserves role and command argument");
 check(stopped["sram"]==read(base+original["after"].get<std::string>())&&stopped["battle"]["tactics"]["points"]==3,"Full original SRAM and mobility after blocked fort movement");replay();
 check(stopped["random_cursor"]==blocked["random_cursor"],"Planning and blocked movement consume no logic RNG");
 check(!game.continue_computer_scan().empty()&&game.save()==stopped,"Cannot repeat the completed scan");
 call(game.advance_computer_tactics());replay();
 std::cout<<"Original argument lifetime: "<<checks<<" checks passed; natural first-turn boundary, original SRAM, writers, v2 compatibility and atomic v3 replay\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
