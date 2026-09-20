#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
std::vector<std::uint8_t> fixture(const std::string &name){std::ifstream f(std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/"+name,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
void run_ai(OriginalSession &session){
    for(int i=0;i<24;++i){
        if(session.save()["phase"]!="ai_turn")return;
        auto result=session.advance();check(!result.contains("error"),result.dump().c_str());
    }
    throw std::runtime_error("AI did not finish within 24 phases");
}
int main(int argc,char **argv){try{
    if(argc!=2)throw std::runtime_error("Expected reference ROM");
    std::ifstream file(std::filesystem::u8path(argv[1]),std::ios::binary);
    OriginalRom rom(std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)),{}));
    for(int difficulty=0;difficulty<3;++difficulty)for(int ruler=0;ruler<6;++ruler){
        OriginalSession session(rom);session.start(ruler,difficulty);
        run_ai(session);auto before=session.save();
        check(before["phase"]=="player_commands"&&before["sram"][0xd8b]==ruler,"Opening AI hands control to chosen ruler");
        check(session.end_turn().empty(),"End player turn");run_ai(session);
        auto after=session.save();check(after["phase"]=="player_commands"&&after["sram"][0xd87]==2,"Six factions advance one month");
        OriginalSession restored(rom);check(restored.restore(after).empty()&&restored.save()==after,"Turn snapshot restores exactly");
    }
    OriginalSession session(rom);session.start(4,0);run_ai(session);
    check(session.search(13,145).empty(),"Search dispatch");
    check(session.end_turn().empty(),"Search turn ends");run_ai(session);
    const auto before_report=session.save();
    auto report=session.visit_city(13);check(!report.is_null()&&report["officer"]==145,"Search returns naturally after monthly settlement");
    check(!session.end_turn().empty(),"Pending report prevents losing report on turn end");
    session.finish_search(true);check(session.end_turn().empty(),"Reported search allows next turn");
    session.start(4,0,1);run_ai(session);check(session.save()["sram"][0xd8b]==1,"Second player can act earlier in faction order");
    check(session.end_turn().empty(),"Second player ends turn");run_ai(session);check(session.save()["sram"][0xd8b]==4,"First player receives same month");
    check(session.end_turn().empty(),"First player ends turn");run_ai(session);check(session.save()["sram"][0xd8b]==1&&session.save()["sram"][0xd87]==2,"Two humans share monthly settlement");
    std::ifstream af(std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/ai.json");auto cases=Json::parse(af);
    session.start(4,0);auto incoming=session.save();incoming["sram"]=fixture("ai-15-before.bin");incoming["ai"]=cases[15]["runtime"];incoming["ai"]["done"]=false;
    incoming["random_cursor"]=incoming["ai"]["random_cursor"];incoming["phase"]="ai_turn";
    check(session.restore(incoming).empty(),"Controlled human invasion loads");
    auto step=session.advance();check(!step.contains("error"),"Native invasion runs");
    auto battle=session.save();check(battle["phase"]=="battle"&&battle["sram"]==fixture("ai-15-after.bin"),"Human invasion preserves byte-exact battle ledger");
    check(!session.end_turn().empty()&&!session.move(21,13,{4}).empty(),"Strategic commands cannot bypass battle");
    OriginalSession restored(rom);check(restored.restore(battle).empty()&&restored.save()==battle,"Pending battle saves without replaying sortie");
    auto broken=battle;broken["battle"]["target"]=0;check(!restored.restore(broken).empty()&&restored.save()==battle,"Conflicting battle save preserves live state");
    session.restore(before_report);auto saved=session.save();saved["phase"]="ending";saved["ending"]={{"kind","unification"},{"winner",4}};
    check(!session.restore(saved).empty()&&session.save()==before_report,"Fabricated ending rejected atomically");
    std::cout<<"PASS: 18 opening/month scenarios, natural search return, two-player order, invasion persistence, invalid phase saves\n";
    return 0;
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
