#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
std::vector<std::uint8_t> read(const std::string &path){std::ifstream f(std::filesystem::u8path(path),std::ios::binary);if(!f)throw std::runtime_error(path);return {(std::istreambuf_iterator<char>(f)),{}};}
void check(bool ok,const std::string &message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char **argv){try{
    check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string base=std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/";
    std::ifstream f(base+"commands.json");auto cases=Json::parse(f);
    for(const auto &c:cases){
        OriginalState state(rom,read(base+c["before"].get<std::string>()));std::uint8_t cursor=c["random_before"];
        auto result=state.execute_command(c["city"],c["kind"],c,cursor,c["frame"]);check(!result.contains("error"),"Case "+c["id"].dump()+": "+result.dump());
        const auto expected=read(base+c["after"].get<std::string>());
        for(int i=0;i<8192;++i)if(state.sram()[i]!=expected[i])throw std::runtime_error("Case "+c["id"].dump()+" mismatch "+std::to_string(i)+" expected "+std::to_string(expected[i])+" actual "+std::to_string(state.sram()[i]));
        check(cursor==c["random_after"],"Random cursor");
        if(result.contains("delta"))check(result["delta"]==c["delta"],"Reported delta "+c.dump()+" "+result.dump());
        if(result.contains("outcome"))check(result["outcome"]==c["result"],"Gift outcome");
    }
    std::ifstream sf(base+"scout.json");const auto scouts=Json::parse(sf);
    for(const auto &c:scouts){
        OriginalState state(rom,read(base+c["before"].get<std::string>()));std::uint8_t cursor=0;
        const auto result=state.execute_command(13,"scout",c,cursor,0);
        check(!result.contains("error"),"Scout accepted "+result.dump());
        const auto expected=read(base+c["after"].get<std::string>());
        for(int i=0;i<8192;++i)if(state.sram()[i]!=expected[i])throw std::runtime_error("Scout "+c["target"].dump()+" mismatch "+std::to_string(i)+" expected "+std::to_string(expected[i])+" got "+std::to_string(state.sram()[i]));
        check(cursor==0,"Scout consumes no RNG");
        check(rom.battlefield_pixels(c["target"])==read(base+"battlefield-"+c["target"].dump()+".indices"),"Battlefield pixels "+c["target"].dump());
    }
    OriginalState state(rom,read(base+"command-0-before.bin"));std::uint8_t cursor=4;const auto before=state.sram();
    for(const auto &command:Json::array({Json{{"kind","buy"},{"quantity",999999}},Json{{"kind","award_gold"},{"officer",240},{"quantity",20}},Json{{"kind","transport"},{"target",0},{"items",{1,0,0,0,0}}},Json{{"kind","gift"},{"target",14},{"items",{-1,0,0,0,0}}},Json{{"kind","buy"},{"quantity",1.5}},Json{{"kind","unknown"}}})){
        check(state.execute_command(13,command["kind"],command,cursor,0).contains("error"),"Invalid command rejected");check(state.sram()==before&&cursor==4,"Rejected command atomic");
    }
    OriginalSession session(rom);session.start(4,0);for(int i=0;i<24&&session.save()["phase"]=="ai_turn";++i)session.advance();
    check(!session.execute_command(13,"buy",{{"quantity",1}}).contains("error"),"Session command");auto save=session.save();OriginalSession restored(rom);check(restored.restore(save).empty()&&restored.save()==save,"Command save roundtrip");
    check(session.recruit(13,0).empty(),"Enter formation");check(session.execute_command(13,"buy",{{"quantity",1}}).contains("error"),"Formation prevents treasury order");
    std::cout<<"PASS: "<<cases.size()<<" treasury fixtures and "<<scouts.size()<<" scout SRAM/maps; atomic failures, session guards and save roundtrip\n";
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
