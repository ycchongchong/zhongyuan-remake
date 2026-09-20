#include "zhongyuan/original_rom.hpp"
#include "zhongyuan/original_state.hpp"
#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <filesystem>

void check(bool value,const char *description) {if(!value) throw std::runtime_error(description);}
int main(int argc,char **argv) {
    try {
        check(zhongyuan::OriginalRom::crc32({'1','2','3','4','5','6','7','8','9'})==0xcbf43926,"CRC32 known vector");
        bool rejected=false;
        try { zhongyuan::OriginalRom invalid({0x4e,0x45,0x53,0x1a}); }catch(const std::runtime_error &) {rejected=true;}
        check(rejected,"Truncated ROM rejected");
        if(argc==2) {
            std::ifstream input(std::filesystem::u8path(argv[1]),std::ios::binary);
            if(!input) throw std::runtime_error("Reference ROM missing");
            std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
            const zhongyuan::OriginalRom rom(bytes);
            std::ifstream scenario_file(std::string(ZHONGYUAN_PROJECT_DIR)+"/data/scenario.json");
            auto scenario=zhongyuan::Json::parse(scenario_file);
            const int owners[]={3,4,2,5,1,6};
            for(std::size_t i=0;i<30;++i) {
                auto original=rom.city(i), playable=scenario["cities"][i];
                check(playable["name"]==original["name"] && playable["gold"]==original["gold"],"Playable city identity and initial gold match ROM");
                check(playable["owner"]==owners[original["faction_id_candidate"].get<int>()],"Playable faction mapping");
                int troops=0, ability=0;
                for(auto id:original["officer_slots"]) if(!id.is_null()) {
                    auto officer=rom.officer(id.get<int>());
                    troops+=officer["infantry"].get<int>()+officer["cavalry"].get<int>()+officer["archers"].get<int>();
                    ability=std::max(ability,officer["martial"].get<int>());
                }
                check(playable["troops"]==troops && playable["ability"]==ability,"Playable garrisons derived from ROM officers");
                for(auto key:{"land","commerce","population","control","officer_slots"})
                    check(playable["reference"][key]==original[key],"Original baseline fields preserved");
            }
            std::ifstream map_file(std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/world-roads.json");
            auto map_fixture=zhongyuan::Json::parse(map_file);
            const auto map=rom.world_map();
            check(map["positions"]==map_fixture["oam_positions"],"All city positions match original OAM capture");
            check(map["edges"]==map_fixture["edges"],"All 64 roads match screen components");
            for(std::size_t i=0;i<30;++i){
                std::vector<int> neighbors;
                for(auto edge:map_fixture["edges"]){if(edge[0]==i)neighbors.push_back(edge[1]);else if(edge[1]==i)neighbors.push_back(edge[0]);}
                std::sort(neighbors.begin(),neighbors.end());
                check(scenario["cities"][i]["neighbors"]==neighbors,"Playable original roads");
            }
            auto read_fixture=[](const char*name){std::ifstream in(std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/"+name,std::ios::binary);return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),{});};
            for(int difficulty=0;difficulty<3;++difficulty) {
                check(rom.initial_sram(difficulty)==read_fixture(("initial-difficulty-"+std::to_string(difficulty)+".bin").c_str()),"Original initialization matches all 8192 bytes for each difficulty");
                zhongyuan::OriginalState initial(rom,rom.initial_sram(difficulty));
                initial.set_players(4);initial.begin_ruler_turn(4);
                check(initial.snapshot()["rulers"][4]["books"]==(difficulty==2?2:3),"Liu Bei books reflect original difficulty");
            }
            std::ifstream calendar_file(std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/calendar.json");
            for(const auto &c:zhongyuan::Json::parse(calendar_file)) {
                const auto prefix="calendar-"+std::to_string(c["id"].get<int>());
                zhongyuan::OriginalState state(rom,read_fixture((prefix+"-before.bin").c_str()));
                std::uint8_t cursor=c["rng_before"];
                state.advance_calendar(cursor);
                check(state.sram()==read_fixture((prefix+"-after.bin").c_str()) && cursor==c["rng_after"],"Calendar, year rollover, one/two human rulers and RNG cursor match original instruction-boundary captures");
            }
            std::ifstream monthly_file(std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/monthly.json");
            for(const auto &c:zhongyuan::Json::parse(monthly_file)) {
                const auto prefix="monthly-"+std::to_string(c["id"].get<int>());
                zhongyuan::OriginalState state(rom,read_fixture((prefix+"-before.bin").c_str()));
                std::uint8_t cursor=c["rng_before"];
                state.settle_month(cursor);
                check(state.sram()==read_fixture((prefix+"-after.bin").c_str()),"Monthly arrivals, road expiry and April/October income match complete original SRAM");
                // The seasonal reference additionally runs its timed presentation,
                // which consumes RNG. This state-only method does not reproduce it.
            }
            std::ifstream ai_file(std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/ai.json");
            for(const auto &c:zhongyuan::Json::parse(ai_file)) {
                zhongyuan::OriginalState state(rom,read_fixture(c["before"].get<std::string>().c_str()));
                const auto result=state.ai_phase(c["runtime"]);
                for(auto it=c["expected"].begin();it!=c["expected"].end();++it)
                    if(result[it.key()]!=it.value())throw std::runtime_error("AI runtime "+c["id"].dump()+" "+it.key()+" actual "+result[it.key()].dump()+" expected "+it.value().dump());
                const auto expected=read_fixture(c["after"].get<std::string>().c_str());
                for(int i=0;i<8192;++i)if(state.sram()[i]!=expected[i])throw std::runtime_error("AI SRAM "+c["id"].dump()+" at "+std::to_string(i)+" actual "+std::to_string(state.sram()[i])+" expected "+std::to_string(expected[i]));
            }
            for(const auto &name:{"recruit","assign"}){
                std::ifstream file(std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/"+name+".json");
                for(const auto &c:zhongyuan::Json::parse(file)){
                    zhongyuan::OriginalState state(rom,read_fixture(c["before"].get<std::string>().c_str()));
                    auto error=std::string(name)=="recruit"?state.recruit_reserves(c["city"],c["quantity"]):state.assign_troops(c["city"],c["officer"],c["quantity"]);
                    check(error.empty()!=c.value("rejected",false),"Original recruitment/assignment acceptance");
                    check(state.sram()==read_fixture(c["after"].get<std::string>().c_str()),"Recruitment and troop composition match all original 8192 bytes");
                }
            }
            std::ifstream turn_file(std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/turn.json");
            for(const auto &c:zhongyuan::Json::parse(turn_file)){
                zhongyuan::OriginalState state(rom,read_fixture(c["before"].get<std::string>().c_str()));
                std::uint8_t cursor=c["rng_before"];
                state.next_ruler(cursor);
                check(state.sram()==read_fixture(c["after"].get<std::string>().c_str())&&cursor==c["rng_after"],"Ruler rotation, death skipping, monthly rollover and year limit match original complete SRAM and RNG");
            }
            std::ifstream search_file(std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/search.json");
            for(const auto &c:zhongyuan::Json::parse(search_file)) {
                zhongyuan::OriginalState state(rom,read_fixture(c["before"].get<std::string>().c_str()));
                std::uint8_t cursor=c["rng_before"];
                const std::string id=c["id"];
                if(id=="dispatch")check(state.dispatch_search(13,145).empty(),"Original search dispatch");
                else if(id=="collect")check(state.collect_search(13)==145,"Original return restores officer and retains record tail");
                else if(id.rfind("roll-",0)==0)check(state.search_kind(c["officer"],cursor)==c["rolled_kind"],"Original search outcome tiers");
                else if(id=="person-vacant") {
                    auto found=state.search_find(13,6,cursor);
                    check(found["reroll"]==true && found["exclude_people"]==false,"Empty first candidate rerolls without excluding officer outcomes");
                }else if(id=="person-cost" || id.rfind("find-",0)==0 || id=="gold-wrap" || id=="item-wrap") {
                    const auto found=state.search_find(13,c["kind"],cursor);
                    check(found["value"]==c["value"],"Original search quantity or recruitment quote");
                    if(id=="person-cost")check(found["candidate"]==c["candidate"],"Original search candidate");
                }else {
                    const bool joined=state.recruit_search(13,c["candidate"],c["kind"],c["value"],cursor);
                    check(joined==(c["end_file_offset"]==0x1b28a),"Original search recruitment outcome");
                }
                check(state.sram()==read_fixture(c["after"].get<std::string>().c_str()),"Search matches full independent 8192-byte SRAM");
                if(id!="dispatch" && cursor!=c["rng_after"])throw std::runtime_error("Search RNG mismatch "+id+": native "+std::to_string(cursor)+" reference "+c["rng_after"].dump());
            }
            const auto move_before=read_fixture("move-before.bin"), move_after=read_fixture("move-after.bin");
            zhongyuan::OriginalSession session(rom);
            const auto isolated_session=[&](int difficulty){
                zhongyuan::OriginalState state(rom,rom.initial_sram(difficulty));
                state.set_players(4);state.begin_ruler_turn(4);
                zhongyuan::Json old={{"format","native-original-v1"},{"rom_crc32",zhongyuan::OriginalRom::EXPECTED_CRC32},
                    {"sram",state.sram()},{"frame_counter",0},{"random_cursor",0},{"pending_development",nullptr},{"pending_search",nullptr}};
                check(session.restore(old).empty(),"Legacy isolated command fixture loads");
            };
            isolated_session(2);
            auto quote=session.prepare_development(13,4,0);
            check(!quote.contains("error") && quote["cost"]==16,"Native session generates original development quote");
            check(session.confirm_development().empty(),"Native session confirms development");
            auto live=session.snapshot();
            check(live["cities"][13]["gold"]==184 && live["cities"][13]["land"]==39 && live["rulers"][4]["books"]==1,"Actual mutable city records and original hard-difficulty books");
            check(session.move(13,21,{4}).empty(),"Native session moves real general after development");
            live=session.snapshot();
            check(live["cities"][13]["officer_slots"][0].is_null() && live["cities"][21]["officer_slots"][2]==4 && live["rulers"][4]["seat"]==21 && live["rulers"][4]["books"]==0,"Roster, capital and command count change together");
            auto saved=session.save();
            zhongyuan::OriginalSession restored(rom);
            check(restored.restore(saved).empty() && restored.save()==saved,"Native save roundtrip retains complete bytes and timers");
            saved["sram"][0]=-1;
            check(!restored.restore(saved).empty() && restored.save()==session.save(),"Invalid SRAM load preserves native campaign");
            isolated_session(0);
            const auto before_search=session.save();
            check(!session.search(13,4).empty() && session.save()==before_search,"Ruler cannot search; refusal is atomic");
            check(session.search(13,145).empty(),"Native session dispatches search officer");
            auto away=session.save();
            check(away["sram"][13*36+17]==255 && away["sram"][0xc30]==145 && away["sram"][0xc31]==141,"Search officer removed and delayed return recorded");
            check(session.visit_city(13).is_null(),"Search does not return before its monthly delay clears");
            away["sram"][0xc31]=13; // Controlled ready-return fixture, not an implemented turn advance.
            check(session.restore(away).empty(),"Ready search fixture loads");
            const auto report=session.visit_city(13);
            check(report["officer"]==145 && session.snapshot()["cities"][13]["officer_slots"][1]==145,"Search return integrated with mutable roster");
            const auto reporting=session.save();
            check(restored.restore(reporting).empty() && restored.save()==reporting,"Search report save/load prevents result replay");
            check(restored.visit_city(13)==report && restored.save()==reporting,"Revisiting cannot duplicate search rewards");
            check(!restored.move(13,21,{4}).empty(),"Pending search report blocks other commands");
            const auto processed=restored.finish_search(true);
            check(!processed.contains("error") && restored.visit_city(13).is_null(),"Search report finishes once");
            auto broken=reporting;broken["pending_search"]["kind"]=999;
            const auto stable=restored.save();
            check(!restored.restore(broken).empty() && restored.save()==stable,"Malformed search report cannot overwrite live session");
            zhongyuan::OriginalState moved(rom,move_before);
            check(moved.move_officers(13,21,{4}).empty(),"Original officer move accepted");
            check(moved.sram()==move_after,"Entire 8192-byte SRAM matches independent original move capture");
            zhongyuan::OriginalState invalid_move(rom,move_before);
            check(!invalid_move.move_officers(13,21,{4,145}).empty() && invalid_move.sram()==move_before,"Cannot empty source city");
            check(!invalid_move.move_officers(13,21,{4,4}).empty() && invalid_move.sram()==move_before,"Duplicate officer selection rejected");
            check(!invalid_move.move_officers(13,12,{4}).empty() && invalid_move.sram()==move_before,"Enemy movement rejected");
            std::uint8_t sequence_cursor=0;
            const auto offer=invalid_move.next_development_offer(13,4,0,sequence_cursor);
            check(sequence_cursor==4 && offer["index"]==8 && offer["cost"]==16,"Original sequence rejects low-three-bit values 6,6,7 then accepts 3 in six-option tier");
            std::ifstream development_file(std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/development.json");
            const auto development=zhongyuan::Json::parse(development_file);
            for(const auto &c:development["cases"]) {
                const auto before=read_fixture(c["before"].get<std::string>().c_str());
                const auto after=read_fixture(c["after"].get<std::string>().c_str());
                zhongyuan::OriginalState state(rom,before);
                check(state.develop(c["city"],c["officer"],c["kind"],c["proposal"],c["random_byte"]).empty(),"Original development accepted");
                check(state.sram()==after,"All 8192 bytes match independent land/commerce/population development captures");
                zhongyuan::OriginalState refused(rom,before);
                check(!refused.develop(c["city"],c["officer"],c["kind"],0,c["random_byte"]).empty() && refused.sram()==before,"Wrong intelligence-tier proposal rejected atomically");
                auto poor=before;poor[13*36+1]=poor[13*36+2]=poor[13*36+3]=0;
                zhongyuan::OriginalState no_gold(rom,poor);
                check(!no_gold.develop(c["city"],c["officer"],c["kind"],c["proposal"],c["random_byte"]).empty() && no_gold.sram()==poor,"Insufficient development gold rejected atomically");
            }
            auto city=rom.city(13);
            check(city["gold"]==200 && city["land"]==25 && city["commerce"]==20 && city["population"]==19000 && city["control"]==55,"Xinye screen data");
            check(city["officer_slots"][0]==4 && city["officer_slots"][1]==145,"Xinye officer identities");
            const auto liubei=rom.officer(4),zhaoyun=rom.officer(145);
            check(liubei["stamina"]==78 && liubei["intelligence"]==63 && liubei["martial"]==53 && liubei["virtue"]==99 && liubei["loyalty"].is_null(),"Liu Bei attributes");
            // 9C1A/9C30/9C43 initialize kinds 0/1/2; A520 and A568 select kind 2 for projectiles.
            check(liubei["infantry"]==200 && liubei["cavalry"]==400 && liubei["archers"]==400,"Liu Bei troop composition");
            check(zhaoyun["stamina"]==98 && zhaoyun["intelligence"]==85 && zhaoyun["martial"]==96 && zhaoyun["virtue"]==87 && zhaoyun["loyalty"]==95,"Zhao Yun attributes");
            check(zhaoyun["infantry"]==100 && zhaoyun["cavalry"]==400 && zhaoyun["archers"]==0,"Zhao Yun troop composition");
            for(const auto &spec:std::vector<std::pair<bool,int>>{{false,13},{true,4},{true,145}}) {
                const auto file=std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/"+(spec.first?"officer-":"city-")+std::to_string(spec.second)+"-name-mask.bin";
                std::ifstream expected_file(file,std::ios::binary);
                const std::vector<std::uint8_t> expected((std::istreambuf_iterator<char>(expected_file)),{});
                const auto actual=rom.name_pixels(spec.first,spec.second);
                check(expected.size()==actual.size() && std::equal(expected.begin(),expected.end(),actual.begin()),"Original screenshot glyph pixels");
            }
            for(std::size_t i=0;i<rom.OFFICER_COUNT;++i) rom.name_pixels(true,i);
            for(std::size_t i=0;i<rom.CITY_COUNT;++i) rom.name_pixels(false,i);
            bool bounds=false;try{rom.tile(16384);}catch(const std::out_of_range&){bounds=true;}
            check(bounds,"CHR bounds checked");
            bytes[0x3000]^=1; rejected=false;
            try{zhongyuan::OriginalRom wrong(bytes);}catch(const std::runtime_error&){rejected=true;}
            check(rejected,"Modified ROM rejected");
            std::cout<<"PASS: selected ROM, original data, 271 name decodes, 3 screenshot glyph matches and malformed data\n";
        }
        return 0;
    }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
