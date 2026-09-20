#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool value,const std::string &message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(std::filesystem::u8path(p),std::ios::binary);check(bool(f),p);return {(std::istreambuf_iterator<char>(f)),{}};}
int main(int argc,char **argv){try{
    check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const auto base=std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/";
    std::ifstream f(base+"expedition.json");const auto cases=Json::parse(f);
    for(const auto &c:cases){
        const auto before=read(base+c["before"].get<std::string>());OriginalState state(rom,before);
        auto quote=state.expedition_quote(c["city"],c["target"],c["officers"].get<std::vector<int>>(),c["leader"]);
        check(!quote.contains("error")&&quote["cost"]==c["cost"]&&state.sram()==before,"Non-mutating original sortie quote");
        auto result=state.dispatch_expedition(c["city"],c["target"],c["officers"].get<std::vector<int>>(),c["leader"]);
        check(!result.contains("error"),result.dump());const auto expected=read(base+c["after"].get<std::string>());
        for(int i=0;i<8192;++i)check(state.sram()[i]==expected[i],"Expedition "+c["id"].dump()+" SRAM "+std::to_string(i));
        check(result["reference_adviser"]==c["reference_adviser"],"Original adviser scratch semantics");
    }
    OriginalState state(rom,read(base+"expedition-0-before.bin"));const auto before=state.sram();
    std::ifstream defense_file(base+"defenders.json");
    for(const auto &c:Json::parse(defense_file)){
        OriginalState defense(rom,read(base+c["before"].get<std::string>()));
        check(defense.collect_defenders(c["target"])==c["defenders"],"Original defender roster");
        check(defense.sram()==read(base+c["after"].get<std::string>()),"Full-SRAM defender collection "+c["id"].dump());
        const auto prepared=defense.sram();defense.collect_defenders(c["target"]);
        check(defense.sram()==prepared,"Repeated collection preserves slots and deployment bytes");
    }
    auto full=before;for(int i=0;i<12;++i)full[0xdaa+i*2]=200+i;
    OriginalState overflow(rom,full);bool rejected=false;
    try{overflow.collect_defenders(12);}catch(const std::invalid_argument &){rejected=true;}
    check(rejected&&overflow.sram()==full,"Full malformed ledger rejected atomically");
    for(auto ids:std::vector<std::vector<int>>{{},{145,145},{4,143,144,145},{240}}){check(state.dispatch_expedition(13,12,ids,145).contains("error")&&state.sram()==before,"Invalid sortie is atomic");}
    check(state.dispatch_expedition(13,12,{145},4).contains("error"),"Leader must depart");
    check(state.dispatch_expedition(13,21,{145},145).contains("error"),"Friendly target rejected");
    check(state.dispatch_expedition(13,0,{145},145).contains("error"),"Disconnected target rejected");
    auto poor=before;poor[13*36+1]=poor[13*36+2]=poor[13*36+3]=0;OriginalState p(rom,poor);check(p.dispatch_expedition(13,12,{145},145).contains("error")&&p.sram()==poor,"Insufficient gold atomic");
    auto treaty=before;treaty[0xc8d+13*8]=0x6c;OriginalState t(rom,treaty);check(t.dispatch_expedition(13,12,{145},145).contains("error")&&t.sram()==treaty,"Original treaty masks prohibit invasion");
    OriginalSession session(rom);session.start(4,0);for(int i=0;i<24&&session.save()["phase"]=="ai_turn";++i)session.advance();
    const auto quote=session.expedition_quote(13,12,{145},145);check(!quote.contains("error"),quote.dump());
    const auto before_dispatch=session.snapshot();
    Json expected_defenders=Json::array();for(const auto &id:before_dispatch["cities"][12]["officer_slots"])if(!id.is_null())expected_defenders.push_back(id);
    check(!session.dispatch_expedition(13,12,{145},145).contains("error"),"Session expedition");const auto save=session.save();check(save["phase"]=="expedition","Dedicated human expedition phase");
    OriginalSession restored(rom);check(restored.restore(save).empty()&&restored.save()==save,"Exact expedition restoration");
    check(save["battle"]["defenders"]==expected_defenders,"Session collects defenders after opening AI movements");
    auto corrupt_defense=save;corrupt_defense["sram"][0xdaa]=240;
    check(!restored.restore(corrupt_defense).empty()&&restored.save()==save,"Forged defender rejected atomically");
    auto legacy=save;legacy["battle"].erase("defenders");for(int i=0;i<24;++i)legacy["sram"][0xdaa+i]=255;
    OriginalSession old(rom);check(old.restore(legacy).empty()&&old.save()==legacy,"Earlier expedition saves still restore without invented preparation");
    check(!restored.end_turn().empty()&&restored.execute_command(13,"buy",{{"quantity",1}}).contains("error"),"Cannot bypass battle through strategic commands");
    auto broken=save;broken["battle"]["leader"]=4;check(!restored.restore(broken).empty()&&restored.save()==save,"Conflicting commander restore rejected atomically");
    std::cout<<"PASS: five full-SRAM sorties, 32 defender captures, atomic rejection, old/new expedition saves and phase guards\n";
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
