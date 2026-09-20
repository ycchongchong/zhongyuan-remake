#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool value,const std::string &message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string &path){std::ifstream f(std::filesystem::u8path(path),std::ios::binary);check(bool(f),path);return {(std::istreambuf_iterator<char>(f)),{}};}
int main(int argc,char **argv){try{
    check(argc==2,"Expected reference ROM");OriginalRom rom(read(argv[1]));
    const std::string project=ZHONGYUAN_PROJECT_DIR;const auto base=project+"/reference/fixtures/";
    {
        OriginalState state(rom,rom.initial_sram(0));const auto raw=state.sram();auto units=Json(std::vector<int>(66,98)),captives=Json(std::vector<int>(24,255));const auto original=units;std::uint8_t cursor=255;
        Json context={{"first",145},{"second",12},{"side",1},{"command",0},{"flags",0},{"last_damage",0},{"frame",0},{"orientation",0},{"tactical_side",128},{"first_token",48},{"second_token",17},{"target",12},{"kind","defeat"}};
        check(state.duel_strike(units,context,cursor).contains("error")&&units==original&&cursor==255,"Invalid duel side is atomic");
        context["side"]=0;context["frame"]=256;
        check(state.duel_ai(units,context,cursor).contains("error")&&cursor==255,"Invalid AI frame cannot consume randomness");
        context["kind"]=7;
        check(state.duel_finish(units,captives,context,cursor).contains("error")&&state.sram()==raw&&units==original&&cursor==255,"Invalid duel result kind rejected atomically");
        context["kind"]="defeat";
        check(state.duel_finish(units,captives,context,cursor).contains("error")&&state.sram()==raw,"Living duelist cannot trigger defeat settlement");
    }
    {
        std::ifstream file(base+"duel-strike.json");
        for(const auto &c:Json::parse(file)){
            auto raw=rom.initial_sram(0);raw[0x438+c["first"].get<int>()*8+3]=c["strength"][0];raw[0x438+c["second"].get<int>()*8+3]=c["strength"][1];
            OriginalState state(rom,raw);auto units=Json(std::vector<int>(66,255));units[0]=3;units[33]=131;units[1]=c["hp"][0];units[34]=c["hp"][1];
            const auto before=units;std::uint8_t cursor=c["cursor_before"];
            auto result=state.duel_strike(units,c,cursor);auto expected=before;expected[1]=c["hp_after"][0];expected[34]=c["hp_after"][1];
            check(!result.contains("error")&&units==expected&&cursor==c["cursor_after"]&&result["flags"]==c["flags_after"]&&result["last_damage"]==c["damage"]&&state.sram()==raw,"Original duel strike "+c["id"].dump()+": "+result.dump());
        }
    }
    {
        std::ifstream file(base+"duel-ai.json");OriginalState state(rom,rom.initial_sram(0));const auto raw=state.sram();
        for(const auto &c:Json::parse(file)){
            auto units=Json(std::vector<int>(66,255));units[1]=c["hp"][0];units[34]=c["hp"][1];std::uint8_t cursor=c["cursor_before"];
            const auto result=state.duel_ai(units,c,cursor);
            check(!result.contains("error")&&result["command"]==c["command"]&&cursor==c["cursor_after"]&&state.sram()==raw,"Original duel AI "+c["id"].dump()+": "+result.dump());
        }
    }
    {
        std::ifstream file(base+"duel-finish.json");
        for(const auto &c:Json::parse(file)){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));auto units=c["units"],captives=c["captives"];std::uint8_t cursor=c["cursor_before"];
            const auto result=state.duel_finish(units,captives,c["context"],cursor);
            check(!result.contains("error")&&units==c["units_after"]&&captives==c["captives_after"]&&cursor==c["cursor_after"]&&result["phase"]==c["phase"]&&result["counter"]==c["counter"],"Original duel finish fields "+c["id"].dump()+": "+result.dump());
            check(state.sram()==read(base+c["after"].get<std::string>()),"Original duel finish full SRAM "+c["id"].dump());
        }
    }
    {
        std::ifstream reference(base+"duel-retreat.json");
        for(const auto &c:Json::parse(reference)){
            check(c["returned"]==true&&c["units"]==c["units_after"]&&read(base+c["before"].get<std::string>())==read(base+c["after"].get<std::string>()),"Original duel retreat preserves SRAM and all units");
            auto expected=c["entry"];expected.update({{"global_phase",10},{"phase",1},{"counter",15},{"sequential",1},{"direction",139}});
            auto orders=c["orders"];orders[c["side"]==0?3:7]=1;
            check(c["runtime"]==expected&&c["orders_after"]==orders,"Original duel retreat restores order menus and sequential continuation");
        }
    }
    for(int variant:{0,3,4}){
        std::ifstream fixture(project+"/tests/clash-duel-"+std::to_string(variant)+".json");const auto entry=Json::parse(fixture);
        OriginalSession session(rom),loaded(rom);check(session.restore(entry).empty()&&session.save()==entry,"Duel fixture replays legal movement and combat to legacy boundary");
        check(session.begin_duel().empty(),"Enter native duel");const auto start=session.save();
        check(loaded.restore(start).empty()&&loaded.save()==start,"Duel entry restores exactly");
        check(!session.begin_duel().empty()&&!session.advance_clash().empty()&&session.save()==start,"Duel entry is one-shot and holds ordinary clash execution");
        int exchanges=0;
        for(int step=0;step<80;++step){
            auto before=session.save();auto &a=before["battle"]["tactics"]["attack"];const auto stage=a["stage"];
            if(stage=="clash_result")break;
            const int side=a["duel"]["side"],player=a["clash"]["players"][side?1:0];std::string error;
            if(stage=="duel_orders"&&player){
                if(a["clash"][side?"second":"first"]<6)check(!session.choose_duel_command(4).empty()&&session.save()==before,"Human ruler cannot surrender in duel");
                // Exercise all three attacks, including possible counterdamage.
                error=session.choose_duel_command(exchanges%3);++exchanges;
            }else error=session.advance_duel();
            check(error.empty(),"Advance duel path: "+error);
            const auto after=session.save();
            check(after["battle"]["tactics"]["points"]==entry["battle"]["tactics"]["points"],"Duel cannot charge tactical mobility");
            check(loaded.restore(after).empty()&&loaded.save()==after,"Every duel selection, exchange, confirmation and result replays exactly");
            auto forged=after;forged["battle"]["tactics"]["attack"]["duel"]["flags"]=255;
            check(!loaded.restore(forged).empty()&&loaded.save()==after,"Forged duel effects rejected atomically");
            if(after["battle"]["tactics"]["attack"]["stage"]=="clash_orders")break; // A verified NPC retreat may return to ordinary fighting.
            if(after["battle"]["tactics"]["attack"]["stage"]=="duel_surrender_confirm")check(session.answer_duel_surrender(true).empty(),"Accept selected human surrender");
        }
        const auto final=session.save();const auto stage=final["battle"]["tactics"]["attack"]["stage"];
        check(stage=="clash_result"||stage=="clash_orders","Duel reaches an original exit without a fake victory");
        std::cout<<"Duel session "<<variant<<": "<<stage.dump()<<"\n";
        if(variant==0){
            check(session.restore(start).empty(),"Restore duel before voluntary retreat");const auto before=session.save();
            check(session.choose_duel_command(3).empty()&&session.advance_duel().empty(),"Duel retreat returns to existing army orders");
            const auto returned=session.save();const auto &r=returned["battle"]["tactics"]["attack"]["clash"]["runtime"];
            check(returned["sram"]==before["sram"]&&r["units"]==before["battle"]["tactics"]["attack"]["clash"]["runtime"]["units"]&&r["orders"][3]==1&&r["sequential"]==1,"Retreat preserves troops and sets original retreat order and sequential flag");
            check(loaded.restore(returned).empty()&&loaded.save()==returned&&session.advance_clash().empty(),"Resume and restore clash after duel retreat");
            check(session.restore(start).empty(),"Restore duel before surrender");
            check(session.choose_duel_command(4).empty()&&session.answer_duel_surrender(false).empty(),"Cancel human duel surrender");
            check(session.save()["sram"]==start["sram"]&&session.save()["random_cursor"]==start["random_cursor"],"Cancelled surrender changes no SRAM or RNG");
            check(session.choose_duel_command(4).empty()&&session.answer_duel_surrender(true).empty(),"Confirm human duel surrender");
            for(int step=0;step<2;++step){check(session.advance_duel().empty(),"Advance original surrender notices");const auto saved=session.save();check(loaded.restore(saved).empty()&&loaded.save()==saved,"Duel surrender result restores exactly");}
            const auto surrendered=session.save();const auto &result=surrendered["battle"]["tactics"]["attack"]["result"];
            check(result["kind"]=="duel"&&result["outcome"]=="surrender"&&result["can_continue"]==true,"Duel surrender preserves original captive result and continuation");
            check(!session.advance_duel().empty()&&session.save()==surrendered&&session.finish_clash_result().empty(),"Duel result cannot settle twice and can resume tactics");
        }
    }
    std::cout<<"PASS: original duel kernels and sessions\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
