#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool ok,const std::string &message){if(!ok)throw std::runtime_error(message);std::cout<<"PASS: "<<message<<'\n';}
std::vector<std::uint8_t> read(const std::string &path){std::ifstream f(std::filesystem::u8path(path),std::ios::binary);if(!f)throw std::runtime_error(path);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try{
    check(argc==2,"audio test ROM supplied");
    OriginalRom rom(read(argv[1]));OriginalSession game(rom);
    game.start(4,0);check(game.music_cue()=="campaign","natural opening selects strategic score");
    auto load=[&](const std::string &name){auto error=game.restore(Json::parse(read(std::string(ZHONGYUAN_PROJECT_DIR)+"/tests/"+name+".json")));check(error.empty(),"strict restore "+name+": "+error);};
    const std::pair<const char*,const char*> cases[]={
        {"computer-plan-start","tactical"},{"clash-session-start","clash_orders"},
        {"clash-strategy-boundary","clash"},{"long-clash-segmented","clash"},
        {"long-clash-result","tactical"},{"natural-campaign-defeat","defeat"}};
    for(auto [file,cue]:cases){load(file);const auto before=game.save();check(game.music_cue()==cue,std::string(file)+" selects "+cue);for(int i=0;i<100;++i)game.music_cue();check(game.save()==before,"audio reads preserve SRAM, random cursor and strict history");}
    load("clash-duel-0");check(game.begin_duel().empty()&&game.music_cue()=="duel","entering duel selects its original score");
    load("clash-second-ruler-defeat");
    check(game.advance_clash_defeat().empty()&&game.advance_clash_defeat().empty()&&game.begin_human_failure().empty(),"reach two-player failure report through native actions");
    check(game.music_cue()=="battle_result","surviving player gets battle result music rather than terminal defeat");
    check(game.finish_human_failure().empty()&&game.music_cue()=="battle_result","territory settlement keeps the result score");
    std::cout<<"Original audio cue validation passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
