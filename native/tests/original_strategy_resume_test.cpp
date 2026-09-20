#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
#include <map>
using namespace zhongyuan;
void check(bool value,const std::string &message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string &path){std::ifstream f(std::filesystem::u8path(path),std::ios::binary);check(bool(f),path);return {(std::istreambuf_iterator<char>(f)),{}};}
int main(int argc,char **argv){try{
    check(argc==2,"Expected reference ROM");OriginalRom rom(read(argv[1]));
    const std::string project=ZHONGYUAN_PROJECT_DIR,base=project+"/reference/fixtures/";
    std::ifstream file(base+"clash-strategy.json");const auto cases=Json::parse(file);
    int independent=0,resolved=0,held=0,exhaustive=0;bool tested_resolved=false,tested_held=false,cursor_only=false;
    for(const auto &c:cases){
        OriginalState state(rom,read(base+c["before"].get<std::string>()));const auto raw=state.sram();
        auto context=c["context"];std::uint8_t exact_cursor=c["cursor_before"];
        const auto exact=state.resolve_clash_strategy(c["units"],c["orders"],context,exact_cursor);
        check(exact==c["output"]&&exact_cursor==c["cursor_after"],"Explicit scratch matches original ROM sample "+c["id"].dump());
        context.erase("tail");std::uint8_t legacy_cursor=c["cursor_before"],cursor=legacy_cursor;
        const auto legacy=state.clash_strategy(c["units"],c["orders"],context,legacy_cursor);
        const auto result=state.resolve_clash_strategy(c["units"],c["orders"],context,cursor);
        const bool missing=legacy.value("boundary",std::string{})=="ai_scratch",ambiguous=result.contains("error");
        if(!missing){++independent;check(result==legacy&&cursor==legacy_cursor,"Unaffected paths remain identical");}
        else if(ambiguous){++held;check(result["boundary"]=="ai_scratch"&&cursor==c["cursor_before"],"Ambiguous decisions do not consume RNG");}
        else{++resolved;check(result==exact&&cursor==exact_cursor,"Universal decision agrees with captured tail");}
        check(state.sram()==raw,"Analysis cannot mutate battle SRAM");
        if(missing&&((ambiguous&&(!tested_held||!cursor_only))||(!ambiguous&&!tested_resolved))){
            Json first;std::uint8_t first_cursor=0;bool all_equal=true;std::map<std::string,int> outcomes;
            // Independent oracle enumerates every position, not terrain classes.
            for(int kind=0;kind<4;++kind)for(int position=0;position<256;++position){
                auto input=context;input["tail"]=Json::array({kind|252,(position*37)&255,position});
                std::uint8_t trial_cursor=c["cursor_before"];const auto trial=state.clash_strategy(c["units"],c["orders"],input,trial_cursor);
                check(!trial.contains("error"),"Every tail representative is valid");++exhaustive;
                const auto key=trial.dump();auto inserted=outcomes.emplace(key,trial_cursor);
                if(!inserted.second&&inserted.first->second!=trial_cursor)cursor_only=true;
                if(first.is_null()){first=trial;first_cursor=trial_cursor;}
                else if(first!=trial||first_cursor!=trial_cursor)all_equal=false;
            }
            check(all_equal!=ambiguous,"All 1024 type/position combinations confirm decision classification");
            if(all_equal)check(first==result&&first_cursor==cursor,"Exhaustive consensus matches resolver");
            if(ambiguous)tested_held=true;else tested_resolved=true;
        }
    }
    check(resolved>0&&held>0&&tested_resolved&&tested_held,"Both safe continuation and genuinely ambiguous states covered");
    {
        OriginalState state(rom,rom.initial_sram(0));const auto &c=cases.front();auto context=c["context"];context["tail"]=Json::array({0,0,std::uint64_t(1)<<32});std::uint8_t cursor=255;
        check(state.resolve_clash_strategy(c["units"],c["orders"],context,cursor).contains("error")&&cursor==255,"Invalid explicit scratch never replaced with a guessed value");
        check(state.resolve_clash_strategy(Json::array(),c["orders"],Json::object(),cursor).contains("error")&&cursor==255,"Malformed input is rejected atomically");
    }
    check(cursor_only,"Identical orders with different RNG consumption must still hold");
    {
        std::ifstream file(project+"/tests/clash-strategy-boundary.json");const auto entry=Json::parse(file);
        OriginalSession session(rom),loaded(rom);
        check(!session.resume_clash_strategy().empty(),"Cannot resume strategy outside a battle");
        check(session.restore(entry).empty()&&session.save()==entry,"Legacy unknown-scratch save remains exactly compatible");
        check(!session.advance_clash().empty()&&session.save()==entry,"Legacy advance cannot bypass the held boundary");
        int resumed=0,steps=0;
        for(;steps<600;++steps){
            const auto before=session.save();const auto &attack=before["battle"]["tactics"]["attack"];std::string error;
            if(attack["stage"]=="clash_boundary"&&attack["boundary"]=="ai_scratch"){
                error=session.resume_clash_strategy();
                if(!error.empty()){check(session.save()==before,"Ambiguous session remains entirely unchanged");break;}
                ++resumed;const auto after=session.save();
                check(after["sram"]==before["sram"]&&after["battle"]["tactics"]["points"]==before["battle"]["tactics"]["points"]&&after["battle"]["tactics"]["attack"]["clash"]["runtime"]["units"]==attack["clash"]["runtime"]["units"],"Strategy continuation preserves troops, occupancy, resources and mobility");
                check(!after["battle"]["tactics"]["attack"].contains("boundary")&&!after["battle"]["tactics"]["attack"]["clash"]["runtime"].contains("tail"),"Continuation stores neither stale pause nor invented scratch");
                check(loaded.restore(after).empty()&&loaded.save()==after,"Continuation event restores exactly");
                check(!session.resume_clash_strategy().empty()&&session.save()==after,"Cannot execute the same strategy twice");
                auto forged=after;forged["battle"]["tactics"]["attack"]["clash"]["runtime"]["orders"][0]=3;
                check(!loaded.restore(forged).empty()&&loaded.save()==after,"Forged result rejected atomically");
                forged=after;forged["battle"]["tactics"]["moves"].back()["cursor"]=std::uint64_t(1)<<32;
                check(!loaded.restore(forged).empty()&&loaded.save()==after,"Oversized strategy cursor rejected without narrowing");
            }else if(attack["stage"]=="clash_running")error=session.advance_clash();
            else break;
            check(error.empty(),"Continue combat after strategy: "+error);
        }
        check(resumed>0&&steps>2&&steps<600,"Mirrored NPC battle advances to another genuine boundary");
        check(loaded.restore(session.save()).empty()&&loaded.save()==session.save(),"Full continuation history reloads exactly");
        std::cout<<"Session continuation: "<<resumed<<" decisions, "<<steps<<" steps\n";
    }
    std::cout<<"Original strategy samples: "<<cases.size()<<", independent: "<<independent<<", resolved: "<<resolved<<", held: "<<held<<", exhaustive: "<<exhaustive<<"\n";
    std::cout<<"Original strategy resume passed\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
