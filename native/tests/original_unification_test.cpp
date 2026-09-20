// Controlled original-ROM ending boundaries, not a natural winning playthrough.
#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
static void check(bool ok,const std::string &message){if(!ok)throw std::runtime_error(message);}
static std::vector<std::uint8_t> read(const std::string &path){
    std::ifstream f(std::filesystem::u8path(path),std::ios::binary);check(bool(f),path);
    return {std::istreambuf_iterator<char>(f),{}};
}
int main(int argc,char **argv){try{
    check(argc==2,"Expected reference ROM");OriginalRom rom(read(argv[1]));
    const std::string base=std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/unification-scores/";
    const auto cases=Json::parse(read(base+"cases.json"));
    for(const auto &row:cases){
        auto raw=read(base+row["sram"].get<std::string>());OriginalState state(rom,raw);
        auto summary=state.unification_summary();
        for(const auto *key:{"score","variant","adviser"})check(summary[key]==row[key],row["name"].get<std::string>()+" "+key+": "+summary.dump());
        check(state.sram()==raw,"Scoring does not mutate SRAM");
        OriginalSession session(rom),loaded(rom);auto save=session.save();
        save["sram"]=raw;save["phase"]="ending";save["ai"]=nullptr;save["battle"]=nullptr;save["ending"]=state.ending();
        auto error=session.restore(save);check(error.empty(),"Controlled terminal restore: "+error);
        const auto before=session.save();
        check(session.snapshot()["unification"]==summary,"Snapshot exposes native summary");
        for(int n=0;n<3;++n)session.snapshot();
        check(session.save()==before,"Summary reads leave serialized state untouched");
        check(loaded.restore(before).empty()&&loaded.snapshot()["unification"]==summary,"Score recomputes after load without save-format changes");
        check(!session.end_turn().empty()&&session.advance()["phase"]=="ending"&&session.save()==before,"Terminal state locks strategic actions");
        for(int city=0;city<30;++city){
            auto incomplete=raw;incomplete[city*36]=(incomplete[city*36]&248)|6;
            OriginalState partial(rom,incomplete);
            check(partial.ending().is_null()&&partial.unification_summary().is_null(),"29 cities cannot receive a unification score");
        }
    }
    auto raw=read(base+"one-officer.bin");
    for(int c=0;c<30;++c)for(int slot=0;slot<12;++slot)raw[c*36+16+slot]=255;
    check(OriginalState(rom,raw).unification_summary().contains("error"),"Empty roster cannot divide by zero");
    raw[16]=241;check(OriginalState(rom,raw).unification_summary().contains("error"),"Malformed officer cannot index beyond data");
    for(int variant:{0,2,1}){
        OriginalSession session(rom),loaded(rom);
        const std::string tests=std::string(ZHONGYUAN_PROJECT_DIR)+"/tests/";
        auto before=Json::parse(read(tests+"unification-final-battle-"+std::to_string(variant)+".json"));
        check(session.restore(before).empty(),"Restore controlled final-city battle through strict replay");
        int cities=0;for(int c=0;c<30;++c)cities+=(before["sram"][c*36].get<int>()&7)==4;
        check(cities==29&&session.snapshot()["ending"].is_null(),"Final battle begins with exactly 29 cities and no ending");
        auto call=[&](const std::string &e){check(e.empty(),"Final battle: "+e);};
        const auto &clash=before["battle"]["tactics"]["attack"]["clash"];
        const int side=clash["first"]==12?0:1;
        for(int i=0;i<3;++i)call(session.cycle_clash_order(side,3));
        call(session.request_clash_surrender());call(session.answer_clash_surrender(true));
        call(session.advance_clash_surrender());call(session.advance_clash_surrender());
        call(session.begin_defender_defeat_result());
        const auto pending=session.save();
        check(!session.finish_withdrawal_result().empty()&&session.save()==pending,"Cannot skip final-city captive and resource settlement");
        for(int i=0;i<60&&session.save()["battle"]["tactics"].value("settlement",Json::object()).value("stage",-1)!=29;++i)
            call(session.advance_withdrawal_result());
        call(loaded.restore(session.save()));
        check(loaded.save()==session.save()&&session.snapshot()["ending"].is_null(),"Pending final report replays without prematurely ending campaign");
        call(session.finish_withdrawal_result());
        const auto terminal=session.save();
        check(terminal==Json::parse(read(tests+"unification-ending-"+std::to_string(variant)+".json")),"Final-city actions reproduce exact terminal fixture");
        check(terminal["phase"]=="ending"&&terminal["battle"].is_null()&&terminal["ending"]["winner"]==4,"Last-city settlement enters unification and clears battle");
        check(session.snapshot()["unification"]["variant"]==variant&&session.music_cue()=="unification_"+std::to_string(variant),"Native score selects matching original ending music");
        call(loaded.restore(terminal));check(loaded.save()==terminal,"Terminal save round trips exactly");
        check(!session.end_turn().empty()&&session.save()==terminal,"Completed unification cannot advance turns");
    }
    std::cout<<"Original unification: "<<cases.size()<<" controlled ROM scores, 810 missing-city boundaries, immutable summaries, three final-city settlements, terminal save/load and invalid rosters passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
