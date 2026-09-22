#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
namespace {
int checks=0;
void check(bool ok,const std::string &message){++checks;if(!ok)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string &path){
    std::ifstream f(std::filesystem::u8path(path),std::ios::binary);
    if(!f)throw std::runtime_error(path);
    return {std::istreambuf_iterator<char>(f),{}};
}
Json json(const std::string &path){return Json::parse(read(path));}
}
int main(int argc,char **argv){try{
    check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));
    const std::string root=ZHONGYUAN_PROJECT_DIR,base=root+"/reference/fixtures/";
    const auto stopped=json(root+"/tests/natural-no-war-target.json");
    const auto original=json(base+"strategic-no-target.json");
    check(stopped["sram"]==read(base+"strategic-no-target-before.bin"),"Original entry is the unmodified natural stop");
    OriginalState state(rom,stopped["sram"].get<std::vector<std::uint8_t>>());
    const auto result=state.ai_phase(original["runtime"]);
    check(state.sram()==read(base+"strategic-no-target-after.bin"),"No-target continuation matches all 8192 original SRAM bytes");
    for(auto it=original["expected"].begin();it!=original["expected"].end();++it)
        if(it.key()!="argument")check(result.at(it.key())==it.value(),"Original AI field "+it.key());
    OriginalSession game(rom),loaded(rom);
    check(game.restore(stopped).empty(),"Natural stop strictly restores");
    check(!game.advance().contains("error")&&game.snapshot()["phase"]=="player_commands","Lone capital skips unavailable war and returns player control");
    check(game.save()["command_argument"]==original["expected"]["argument"],"Skipping unavailable war preserves command byte");
    auto save=game.save();check(loaded.restore(save).empty()&&loaded.save()==save,"Resumed campaign round trips exactly");
    const auto victory=json(root+"/tests/natural-unification.json");
    check(game.restore(victory).empty(),"Natural victory strictly restores");
    auto snapshot=game.snapshot();
    check(snapshot["phase"]=="ending"&&snapshot["ending"]["kind"]=="unification"&&snapshot["ending"]["winner"]==3,"Sun Quan is the actual winner");
    for(int city=0;city<30;++city)check((victory["sram"][city*36].get<int>()&7)==3,"Winner owns city "+std::to_string(city));
    check(victory["battle"].is_null(),"Last battle has settled before victory");
    save=game.save();check(loaded.restore(save).empty()&&loaded.save()==save,"Terminal save round trips without second settlement");
    check(!game.end_turn().empty()&&game.advance()["phase"]=="ending"&&game.save()==save,"Terminal campaign cannot resume strategic turns");
    const int score=snapshot["unification"]["score"];
    check(rom.unification_score_rgb(snapshot["year"],snapshot["month"],victory["sram"][0xd88],3,score).size()==256*240*3,"Natural score produces its full original picture");
    std::cout<<"Original natural progress: "<<checks<<" checks; "<<snapshot["year"]<<'/'<<snapshot["month"]<<", score "<<score<<"\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
