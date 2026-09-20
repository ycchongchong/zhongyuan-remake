#include "zhongyuan/original_session.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool value,const std::string &message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string &path){std::ifstream f(std::filesystem::u8path(path),std::ios::binary);check(bool(f),path);return {(std::istreambuf_iterator<char>(f)),{}};}
int main(int argc,char **argv){try{
    check(argc==2,"Expected reference ROM");OriginalRom rom(read(argv[1]));
    const auto base=std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/";
    {
        std::ifstream initial_file(std::string(ZHONGYUAN_PROJECT_DIR)+"/tests/clash-session-start.json");const auto initial=Json::parse(initial_file);
        for(int variant=0;variant<4;++variant){
            auto setup=initial;setup["sram"]=initial["battle"]["tactics"]["deployment_sram"];setup["battle"].erase("tactics");
            auto &s=setup["sram"];s[0xd8a]=130;setup["battle"]["deployment"].update({{"two_players",true},{"handover",false},{"side","attacker"}});
            const int loser=variant==0?12:variant==3?4:145;
            if(variant==3){s[0xdc2]=4;s[13*36+16]=145;setup["battle"]["officers"]=Json::array({143,144,4});s[0x438+4*8+7]=3;s[0x438+4*8+6]=(s[0x438+4*8+6].get<int>()&240)|5;}
            if(variant>=2){
                setup["battle"]["leader"]=loser;
                for(int i=0;i<11;++i)if(s[0xdc2+i*3]!=255){const bool leader=s[0xdc2+i*3]==loser;
                    s[0xdc4+i*3]=(s[0xdc4+i*3].get<int>()&127)|(leader?128:0);s[0xe1a+s[0xdc3+i*3].get<int>()]=0x30|i|(leader?64:0);}
            }
            int cost=0;for(const auto &id:setup["battle"]["officers"])cost+=(s[0x438+id.get<int>()*8+6].get<int>()&15)*20;setup["battle"]["cost"]=cost;
            s[0x438+loser*8+1]=1;setup["random_cursor"]=0;
            OriginalSession defeated(rom),loaded(rom);auto error=defeated.restore(setup);check(error.empty(),"Controlled direct defeat deployment: "+error);
            check(!defeated.advance_clash_defeat().empty()&&defeated.save()==setup,"Defeat before tactics is rejected atomically");
            check(defeated.begin_tactics().empty(),"Begin defeat approach");
            for(const auto &a:initial["battle"]["tactics"]["moves"]){
                const auto kind=a.value("kind",std::string("move"));
                if(kind=="move")error=defeated.move_tactical(a["slot"],a["direction"]);
                else if(kind=="attack")error=defeated.attack_tactical(a["slot"],a["direction"]);
                else if(kind=="confirm_attack"){const auto before=defeated.save();check(!defeated.advance_clash_defeat().empty()&&defeated.save()==before,"Premature defeat cannot create clash state");defeated.tick(7);error=defeated.confirm_tactical_attack();}
                else if(kind=="begin_clash")error=defeated.begin_clash();
                else check(false,"Unexpected defeat approach action");
                check(error.empty(),"Valid direct defeat approach: "+error);
            }
            for(int j=0;j<2;++j)check(defeated.cycle_clash_order(variant==0?0:1,3).empty(),"Winning general waits while ordinary troops fight");
            for(int step=0;step<900;++step){
                check(defeated.advance_clash().empty(),"Execute unit actions before general defeat");
                if(defeated.save()["battle"]["tactics"]["attack"]["stage"]=="clash_boundary")break;
            }
            const auto entry=defeated.save();check(entry["battle"]["tactics"]["attack"]["boundary"]=="general_defeat","General defeat reached by actual hit and end-action dispatch");
            std::ifstream recorded(std::string(ZHONGYUAN_PROJECT_DIR)+"/tests/clash-defeat-"+std::to_string(variant)+".json");
            check(Json::parse(recorded)==entry,"Defeat UI fixture equals controlled native setup and legal action replay");
            check(loaded.restore(entry).empty()&&loaded.save()==entry,"Legacy general-defeat boundary restores exactly");
            for(int step=0;step<2;++step){
                const auto before=defeated.save();const auto &attack=before["battle"]["tactics"]["attack"];const auto &clash=attack["clash"];
                auto context=clash;context.update({{"active",clash["runtime"]["active"]},{"target",before["battle"]["target"]}});
                OriginalState expected(rom,before["sram"].get<std::vector<std::uint8_t>>());
                auto result=step?expected.finish_clash_defeat(clash["runtime"]["units"],context):expected.clash_general_defeat(clash["first"],clash["second"],context["active"],clash["orientation"],clash["tactical_side"]);
                check(!result.contains("error")&&result["officer"]==loser,"Verified original defeat kernels identify losing general");
                if(step&&result["phase"]==11)check(expected.restore_tactical_board(before["battle"]["target"]).empty(),"Original defeat tactical return");
                check(defeated.advance_clash_defeat().empty(),"Advance defeat announcement and result");const auto after=defeated.save();
                check(after["sram"]==expected.sram()&&after["random_cursor"]==before["random_cursor"]&&after["battle"]["tactics"]["points"]==entry["battle"]["tactics"]["points"],"Defeat session matches full SRAM without RNG or mobility charge");
                check(after["battle"]["tactics"]["attack"]["clash"]["runtime"]["units"]==clash["runtime"]["units"],"Defeat acknowledgement does not alter unit injury twice");
                check(loaded.restore(after).empty()&&loaded.save()==after,"Every defeat stage replays exactly");
                auto forged=after;forged["battle"]["tactics"]["attack"]["defeat"]["officer"]=(loser+1)%241;
                check(!loaded.restore(forged).empty()&&loaded.save()==after,"Forged defeated general rejected atomically");
                if(!step){
                    check(!defeated.advance_clash().empty()&&!defeated.advance_clash_retreat().empty()&&!defeated.cycle_clash_order(0,3).empty()&&!defeated.finish_clash_result().empty()&&defeated.save()==after,"Defeat announcement rejects conflicting actions");
                    check(after["battle"]["tactics"]["attack"]["stage"]=="clash_defeat"&&after["battle"]["tactics"]["attack"]["clash"]["runtime"]["counter"]==2,"Announcement holds at original counter two");
                }
            }
            const auto final=defeated.save();const auto &result=final["battle"]["tactics"]["attack"]["result"];
            check(result["kind"]=="defeat"&&result["officer"]==loser&&!defeated.advance_clash_defeat().empty()&&defeated.save()==final,"Defeat settlement occurs once");
            check(result["phase"]==(variant==3?15:11)&&result["map_restored"]==(variant!=3),"Original ruler and ordinary global result phases");
            if(variant==3)check(defeated.clash_rgb().size()==256*160*3,"Ruler defeat retains clash image");
            auto forged=final;forged["battle"]["tactics"]["attack"]["result"]["can_continue"]=!result["can_continue"].get<bool>();
            check(!loaded.restore(forged).empty()&&loaded.save()==final,"Cannot forge defeat continuation permission");
            if(variant>=2)check(result["can_continue"]==false&&!defeated.finish_clash_result().empty()&&defeated.save()==final,"Commander/ruler defeat holds final battle result");
            else check(result["can_continue"]==true&&defeated.finish_clash_result().empty()&&loaded.restore(defeated.save()).empty(),"Ordinary defeat resumes remaining tactical actions");
            std::cout<<"Direct defeat variant "<<variant<<": "<<result.dump()<<"\n";
        }
    }
    {
        std::ifstream file(base+"direct-defeat-flow.json");
        for(const auto &c:Json::parse(file)){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));const auto &context=c["context"];
            auto result=c["step"]==0?state.clash_general_defeat(context["first"],context["second"],context["active"],context["orientation"],context["tactical_side"]):state.finish_clash_defeat(c["units"],context);
            check(!result.contains("error")&&result["officer"]==c["officer"]&&state.sram()==read(base+c["after"].get<std::string>()),"Controlled original defeat flow matches full SRAM");
            check(c["units"]==c["units_after"]&&c["cursor_before"]==c["cursor_after"],"Original defeat acknowledgements preserve units and RNG");
            if(c["step"]==0)check(c["phase"]==10&&c["counter"]==2,"Original announcement enters counter two");
            else check(result["phase"]==c["phase"]&&result["counter"]==c["counter"],"Original global defeat result");
            if(c.contains("winner"))check(result["winner"]==c["winner"],"Original ruler winner faction");
            if(c.contains("returned"))check(state.restore_tactical_board(context["target"]).empty()&&state.sram()==read(base+c["returned"].get<std::string>()),"Original defeat map return matches full SRAM");
        }
    }
    {
        std::ifstream initial_file(std::string(ZHONGYUAN_PROJECT_DIR)+"/tests/clash-session-start.json");const auto initial=Json::parse(initial_file);
        for(int variant=0;variant<7;++variant){
            auto setup=initial;setup["sram"]=initial["battle"]["tactics"]["deployment_sram"];setup["battle"].erase("tactics");
            auto &raw=setup["sram"];const int officer=variant==2?144:variant==5?4:145;
            if(variant==2){
                raw[0xdc2]=144;raw[0xdc5]=145;
                raw[0x438+144*8+4]=48;raw[0x438+144*8+7]=17;raw[0x438+144*8+6]=(raw[0x438+144*8+6].get<int>()&240)|5;
            }
            if(variant==5){
                raw[0xdc2]=4;raw[13*36+16]=145;setup["battle"]["officers"]=Json::array({143,144,4});
                raw[0x438+4*8+7]=3;raw[0x438+4*8+6]=(raw[0x438+4*8+6].get<int>()&240)|5; // Five troops, original infantry/cavalry remainder.
            }
            if(variant==4||variant==5){
                setup["battle"]["leader"]=officer;
                for(int i=0;i<11;++i)if(raw[0xdc2+i*3]!=255){
                    const bool leader=raw[0xdc2+i*3]==officer;raw[0xdc4+i*3]=(raw[0xdc4+i*3].get<int>()&127)|(leader?128:0);
                    raw[0xe1a+raw[0xdc3+i*3].get<int>()]=0x30|i|(leader?64:0);
                }
            }
            raw[0x438+officer*8+1]=(variant>=3&&variant<=5)?1:98;
            setup["random_cursor"]=variant==0?7:variant==2?2:0;
            if(variant==6){raw[0xd8a]=130;setup["battle"]["deployment"].update({{"two_players",true},{"handover",false},{"side","attacker"}});}
            int cost=0;for(const auto &id:setup["battle"]["officers"])cost+=(raw[0x438+id.get<int>()*8+6].get<int>()&15)*20;setup["battle"]["cost"]=cost;
            OriginalSession retiring(rom),loaded(rom);auto error=retiring.restore(setup);
            check(error.empty(),"Controlled retreat deployment "+std::to_string(variant)+": "+error);
            check(retiring.begin_tactics().empty(),"Begin retreat integration tactics");
            for(const auto &action:initial["battle"]["tactics"]["moves"]){
                const auto kind=action.value("kind",std::string("move"));
                if(kind=="move")error=retiring.move_tactical(action["slot"],action["direction"]);
                else if(kind=="attack")error=retiring.attack_tactical(action["slot"],action["direction"]);
                else if(kind=="confirm_attack"){retiring.tick(7);error=retiring.confirm_tactical_attack();}
                else if(kind=="begin_clash")error=retiring.begin_clash();
                else check(false,"Unexpected retreat approach command");
                check(error.empty(),"Valid retreat approach: "+error);
            }
            const int side=variant==6?1:0;
            if(side)for(int kind=0;kind<4;++kind)for(int i=0;i<2;++i)check(retiring.cycle_clash_order(0,kind).empty(),"First player waits for second player's retreat");
            check(retiring.cycle_clash_order(side,3).empty(),"Select retreat order through original menu");
            const auto orders=retiring.save();check(!retiring.advance_clash_retreat().empty()&&retiring.save()==orders,"Cannot retreat before reaching the original field edge");
            for(int i=0;i<200;++i){
                const auto stage=retiring.save()["battle"]["tactics"]["attack"]["stage"];
                if(stage!="clash_orders"&&stage!="clash_running")break;
                check(retiring.advance_clash().empty(),"Reach retreat via unit execution");
            }
            const auto entry=retiring.save();
            check(entry["battle"]["tactics"]["attack"].value("boundary",std::string{})=="retreat","Original retreat entry reached "+std::to_string(variant));
            check(loaded.restore(entry).empty()&&loaded.save()==entry,"Legacy retreat boundary remains exactly replayable");
            if(variant==0||variant==2||variant==5||variant==6){
                std::ifstream recorded(std::string(ZHONGYUAN_PROJECT_DIR)+"/tests/clash-retreat-"+std::to_string(variant)+".json");
                check(Json::parse(recorded)==entry,"UI retreat fixture equals controlled native deployment and action replay");
            }
            bool injured=false,scattered=false;int advances=0;
            for(;advances<5;++advances){
                const auto before=retiring.save();const auto &attack=before["battle"]["tactics"]["attack"];
                if(attack["stage"]=="clash_result")break;
                const auto &runtime=attack["clash"]["runtime"];auto units=runtime["units"],context=attack["clash"];
                context.update({{"active",runtime["active"]},{"target",before["battle"]["target"]},{"counter",runtime["counter"]}});
                OriginalState expected(rom,before["sram"].get<std::vector<std::uint8_t>>());std::uint8_t cursor=before["random_cursor"];
                const bool starting=attack["stage"]=="clash_boundary";
                auto result=starting?expected.clash_retreat_injury(units,runtime["active"],cursor):expected.clash_retreat_confirm(units,context,cursor);
                check(!result.contains("error"),"Reference-verified retreat kernels accept session input");
                if(!starting&&result["phase"]==11)check(expected.restore_tactical_board(before["battle"]["target"]).empty(),"Expected original return map");
                check(retiring.advance_clash_retreat().empty(),"Advance pursuit or retreat acknowledgement");const auto after=retiring.save();
                const auto &actual=after["battle"]["tactics"]["attack"];
                check(after["sram"]==expected.sram()&&after["random_cursor"]==cursor&&actual["clash"]["runtime"]["units"]==units&&actual["clash"]["runtime"]["counter"]==result["counter"],"Retreat integration matches all kernel SRAM, units, counter and RNG");
                check(after["battle"]["tactics"]["points"]==entry["battle"]["tactics"]["points"],"Clash retreat cannot spend tactical mobility");
                check(loaded.restore(after).empty()&&loaded.save()==after,"Every retreat stage replays exactly");
                injured|=actual["retreat"]["injury"]["counter"]==2;scattered|=actual["retreat"]["losses"].get<int>()>0;
                if(advances==0){auto forged=after;forged["battle"]["tactics"]["attack"]["retreat"]["injury"]["damage"]=255;
                    check(!loaded.restore(forged).empty()&&loaded.save()==after,"Forged pursuit result rejected atomically");}
                if(actual["stage"]!="clash_result")check(!retiring.advance_clash().empty()&&!retiring.cycle_clash_order(side,3).empty()&&!retiring.finish_clash_result().empty()&&retiring.save()==after,"Retreat dialogue locks conflicting actions and early result closure");
            }
            const auto final=retiring.save();const auto &result=final["battle"]["tactics"]["attack"]["result"];
            check(result["kind"]=="retreat"&&!retiring.advance_clash_retreat().empty()&&retiring.save()==final,"Retreat settles once only");
            if(variant==0)check(!injured,"Original avoid-pursuit branch");
            if(variant==1)check(injured&&!result["defeated"].get<bool>(),"Survive original pursuit injury");
            if(variant==2)check(scattered,"Original retreat desertion reaches loss announcement");
            if(variant>=3&&variant<=5)check(result["defeated"]==true,"Pursuit disables low-stamina general");
            if(variant==5)check(result["phase"]==15&&result["map_restored"]==false&&retiring.clash_rgb().size()==256*160*3,"Human ruler defeat retains original global phase and clash view");
            else check(result["map_restored"]==true,"Ordinary retreat restores tactical terrain and occupants");
            auto forged=final;forged["battle"]["tactics"]["attack"]["result"]["can_continue"]=!result["can_continue"].get<bool>();
            check(!loaded.restore(forged).empty()&&loaded.save()==final,"Cannot forge retreat continuation permission");
            if(variant==4||variant==5)check(result["can_continue"]==false&&!retiring.finish_clash_result().empty()&&retiring.save()==final,"Commander/ruler departure preserves unresolved final battle");
            else check(result["can_continue"]==true&&retiring.finish_clash_result().empty()&&loaded.restore(retiring.save()).empty(),"Surviving army returns to tactics after clash retreat");
            std::cout<<"Clash retreat variant "<<variant<<": "<<advances<<" stages, injury="<<injured<<", desertion="<<scattered<<", result="<<result.dump()<<"\n";
        }
    }
    {
        std::ifstream file(base+"clash-retreat-flow.json");
        for(const auto &c:Json::parse(file)){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));auto units=c["units"];std::uint8_t cursor=c["cursor_before"];
            const bool injury=c["kind"]=="injury";
            const auto result=injury?state.clash_retreat_injury(units,c["context"]["active"],cursor):state.clash_retreat_confirm(units,c["context"],cursor);
            check(!result.contains("error")&&result["counter"]==c["counter"]&&units==c["units_after"]&&cursor==c["cursor_after"]&&state.sram()==read(base+c["after"].get<std::string>()),"Natural retreat boundary matches native rules");
            if(injury)check(result["damage"]==c["damage"],"Natural pursuit damage matches");else check(result["phase"]==c["phase"],"Natural retreat global phase matches");
            if(c.contains("returned"))check(state.restore_tactical_board(c["context"]["target"]).empty()&&state.sram()==read(base+c["returned"].get<std::string>())&&c["points_before"]==c["points_after"],"Natural retreat settlement and tactical return match full SRAM and mobility");
        }
    }
    {
        std::ifstream file(base+"clash-setup.json");const auto cases=Json::parse(file);
        for(const auto &c:cases){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));
            const auto result=state.initialize_clash(c["context"]);
            for(const auto key:{"units","orders","phase","counter","active","countdown","cycle"})check(result.at(key)==c.at(key),"Clash startup "+std::string(key)+" "+c["id"].dump());
            check(state.sram()==read(base+c["after"].get<std::string>()),"Full SRAM startup "+c["id"].dump());
            if(c.contains("rgb"))check(rom.clash_rgb(c["context"]["map"],4,2,result["units"])==read(base+c["rgb"].get<std::string>()),"Original initial clash pixels "+c["id"].dump());
            auto invalid=c["context"];invalid["map"]=std::uint64_t(1)<<32;const auto before=state.sram();
            check(state.initialize_clash(invalid).contains("error")&&state.sram()==before,"Invalid startup is atomic");
        }
        std::ifstream facing_file(base+"clash-facing.json");
        for(const auto &c:Json::parse(facing_file))check(rom.clash_rgb(c["scene"],c["first_faction"],c["second_faction"],c["units"])==read(base+c["rgb"].get<std::string>()),"Original repainted facing pixels "+c["id"].dump());
        std::ifstream menu_file(base+"clash-menu.json");const auto menus=Json::parse(menu_file);OriginalState state(rom,rom.initial_sram(0));
        const auto before=state.sram();
        for(const auto &c:menus){
            auto result=state.clash_menu(c["orders"],c,c["input"]);
            check(result.at("orders")==c["orders_after"]&&result.at("row")==c["row_after"]&&result.at("phase")==c["phase"]&&result.at("counter")==c["counter"],"Original order menu "+c["id"].dump());
        }
        check(state.sram()==before,"Order menu preserves SRAM");
    }
    {
        for(const auto name:{"clash-step.json","clash-step-natural.json"}){
        std::ifstream file(base+name);const auto cases=Json::parse(file);
        for(const auto &c:cases){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));std::uint8_t cursor=c["cursor_before"];
            auto result=state.clash_step(c["input"],c["context"],cursor);
            for(const auto &[key,value]:c["output"].items())check(result.contains(key)&&result[key]==value,"Clash execution "+c["id"].dump()+" "+key+" expected "+value.dump()+" got "+result.value(key,Json{}).dump()+" result "+result.dump());
            check(cursor==c["cursor_after"]&&state.sram()==read(base+c["after"].get<std::string>()),"Full SRAM/RNG clash execution "+c["id"].dump());
        }
        }
    }
    {
        std::ifstream file(base+"clash-step.json");const auto seed=Json::parse(file)[0];
        const auto raw=read(base+seed["before"].get<std::string>());OriginalState state(rom,raw);
        for(const auto key:{"units","active","target","phase","direction"}){
            auto invalid=seed["input"];invalid[key]=std::uint64_t(1)<<32;std::uint8_t cursor=255;
            check(state.clash_step(invalid,seed["context"],cursor).contains("error")&&cursor==255&&state.sram()==raw,"Malformed execution is atomic");
        }
        std::ifstream ai_file(base+"clash-strategy.json");const auto ai_cases=Json::parse(ai_file);
        for(std::size_t i=0;i<ai_cases.size();i+=53){
            const auto &c=ai_cases[i];OriginalState ai(rom,read(base+c["before"].get<std::string>()));
            auto runtime=seed["input"];runtime.update(c["context"]);runtime.update({{"phase",3},{"units",c["units"]},{"orders",c["orders"]}});
            auto context=seed["context"];context.update(c["context"]);context["map"]=c["context"]["scene"];
            std::uint8_t cursor=c["cursor_before"];auto result=ai.clash_step(runtime,context,cursor);
            for(const auto &[key,value]:c["output"].items())check(result.at(key)==value,"Execution dispatches reference AI result");
            check(cursor==c["cursor_after"]&&result["phase"]==4&&result["counter"]==0,"AI completion returns to unit execution");
        }
        const auto c=ai_cases[0];OriginalState ai(rom,read(base+c["before"].get<std::string>()));
        auto runtime=seed["input"];runtime.update(c["context"]);runtime.update({{"phase",3},{"units",c["units"]},{"orders",c["orders"]}});runtime.erase("tail");
        auto context=seed["context"];context.update(c["context"]);context["map"]=c["context"]["scene"];context.erase("tail");
        std::uint8_t cursor=c["cursor_before"];const auto before=ai.sram();auto result=ai.clash_step(runtime,context,cursor);
        check(result["boundary"]=="ai_scratch"&&result["units"]==runtime["units"]&&result["orders"]==runtime["orders"]&&cursor==c["cursor_before"]&&ai.sram()==before,"Missing original scratch holds AI atomically, including previously sampled RNG");
    }
    for(const auto &name:{"autodeploy","prepare-battle","deploy-move","deploy-confirm","attack-auto","defense-move","defense","pvp","tactical-move","tactical-attack","clash-entry","tactical-formation","tactical-scout","retreat-army","retreat-unit","battle-close"}){
        std::ifstream file(base+name+".json");const auto cases=Json::parse(file);
        for(const auto &c:cases){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));
            if(std::string(name)=="retreat-army"){
                std::uint8_t cursor=c["cursor_before"];auto result=state.retreat_army_step(cursor);
                check(!result.contains("error")&&result["chosen"]==c["chosen"]&&cursor==c["cursor_after"],"Army retreat priority and randomness");
            }
            else if(std::string(name)=="retreat-unit"){
                auto result=state.retreat_attacker(c["slot"]);check(!result.contains("error")&&result["returned"]==c["returned"],"Individual retreat capacity");
            }
            else if(std::string(name)=="battle-close")state.close_battle(c["target"]);
            else if(std::string(name)=="tactical-formation"){
                check(state.tactical_formation(0,c["direction"]).empty()&&c["points_before"]==c["points_after"],"Free original formation arrow");
            }
            else if(std::string(name)=="tactical-scout"){
                int points=c["points_before"];auto result=state.tactical_scout(0,c["cell"],points);
                check(!result.contains("error")==c["accepted"].get<bool>()&&points==c["points_after"],"Scouting target and mobility");
                if(c["accepted"])check(result["officer"]==c["officer"],"Scouted original officer");
            }
            else if(std::string(name)=="tactical-attack"){
                int points=c["points"];auto result=state.tactical_attack(0,c["direction"],points);
                check(!result.contains("error")==c["accepted"].get<bool>()&&points==c["points_after"],"Attack acceptance/cost "+c["id"].dump());
                if(c["accepted"]){const auto &r=c["runtime"];
                    check(result["attacker"]==r["0x688"]&&result["defender"]==r["0x689"]&&result["terrain_pair"]==r["0x68e"]&&result["defender_token"]==r["0x692"],"Attack runtime matches reference");}
            }
            else if(std::string(name)=="clash-entry"){
                int points=10;auto attack=state.tactical_attack(0,0,points);check(!attack.contains("error"),"Clash attack fixture");
                auto clash=state.prepare_clash(attack,c["human"],c["frame"].get<std::uint8_t>());const auto &r=c["runtime"];
                check(clash["map"]==r["0x687"]&&clash["orientation"]==r["0x68f"]&&clash["first"]==r["0x688"]&&clash["second"]==r["0x689"]&&clash["first_token"]==r["0x691"]&&clash["second_token"]==r["0x692"]&&clash["phase"]==r["0x686"]&&clash["counter"]==r["0x68b"],"Clash runtime matches reference "+c["id"].dump());
            }
            else if(std::string(name)=="tactical-move"){
                int points=c["points_before"];const auto error=state.tactical_step(0,c["direction"],points);
                check(points==c["points_after"]&&error.empty()==c["moved"].get<bool>(),"Original tactical cost and acceptance "+c["id"].dump());
            }
            else if(std::string(name)=="pvp"){
                if(c["kind"]=="prepare")state.prepare_deployment(c["source"],c["target"],true,true);
                else if(c["kind"]=="defender_next")state.start_deployment_unit(c["target"],c["slot"],true);
                else if(c["kind"]=="attacker_begin")state.start_deployment_unit(c["target"],0);
                else if(c["kind"]=="attacker_next")state.start_deployment_unit(c["target"],c["slot"]);
            }
            else if(std::string(name)=="defense"){
                if(c["kind"]=="prepare")state.prepare_deployment(c["source"],c["target"],true);
                else if(c["kind"]=="confirm")state.start_deployment_unit(c["target"],c["slot"],true);
                else if(c["kind"]=="auto")state.auto_deploy_attackers(c["target"]);
                else state.finish_defense_deployment(c["target"]);
            }
            else if(std::string(name)=="attack-auto")state.auto_deploy_attackers(c["target"]);
            else if(std::string(name)=="autodeploy")state.auto_deploy_defenders(c["target"]);
            else if(std::string(name)=="prepare-battle")state.prepare_deployment(c["source"],c["target"]);
            else if(std::string(name)=="deploy-confirm")state.start_deployment_unit(c["target"],c["slot"]);
            else{const auto error=state.move_deployment_unit(c["target"],c["slot"],c["direction"],std::string(name)=="defense-move");check(error.empty()==c["moved"].get<bool>(),"Original movement acceptance");}
            const auto expected=read(base+c["after"].get<std::string>());
            for(int i=0;i<8192;++i)check(state.sram()[i]==expected[i],std::string(name)+" "+c["id"].dump()+" SRAM "+std::to_string(i)+" actual "+std::to_string(state.sram()[i])+" expected "+std::to_string(expected[i]));
        }
    }
    {
        std::ifstream file(base+"clash-ai.json");
        for(const auto &c:Json::parse(file)){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));std::uint8_t cursor=c["cursor_before"];
            const auto decision=state.clash_order(c["units"],c["orders"],c["active"],c["second_token"],c["command_before"],cursor);
            check(!decision.contains("error"),"Clash AI accepts reference "+c["id"].dump());
            check(decision["command"]==c["command_after"]&&decision["active"]==c["active_after"]&&decision["phase"]==c["phase"]&&decision["counter"]==c["counter"]&&cursor==c["cursor_after"],"Clash AI "+c["id"].dump()+" actual "+decision.dump()+" cursor "+std::to_string(cursor)+" expected command "+c["command_after"].dump()+" cursor "+c["cursor_after"].dump());
        }
    }
    for(const auto &name:{"clash-move","clash-attack"}){
        std::ifstream file(base+name+".json");
        for(const auto &c:Json::parse(file)){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));auto units=c["units_before"];
            if(std::string(name)=="clash-move")check(state.clash_move(units,c["active"],c["direction"]).empty(),"Execute original movement decision");
            else{
                const auto result=state.clash_attack(units,c["active"],c["command"],c["target_before"]);
                check(!result.contains("error")&&result["target"]==c["target_after"]&&result["phase"]==c["phase"]&&result["counter"]==c["counter"],"Original attack entry "+c["id"].dump()+" "+result.dump());
            }
            check(units==c["units_after"]&&state.sram()==read(base+c["after"].get<std::string>()),"Original action SRAM and unit records "+std::string(name)+" "+c["id"].dump());
        }
    }
    {
        std::ifstream file(base+"clash-next.json");
        for(const auto &c:Json::parse(file)){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));
            const auto r=state.clash_end_action(c["units"],c["first"],c["second"],c["scene"],c["active"],c["extra"],c["moved"]);
            check(!r.contains("error")&&r["active"]==c["active_after"]&&r["extra"]==c["extra_after"]&&r["phase"]==c["phase"]&&r["counter"]==c["counter"],"Original extra-action and unit advance "+c["id"].dump()+" "+r.dump());
            check(state.sram()==read(base+c["after"].get<std::string>()),"Original action repaint SRAM "+c["id"].dump());
        }
    }
    {
        std::ifstream file(base+"projectile.json");
        for(const auto &c:Json::parse(file)){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));
            const auto r=state.clash_projectile(c["units"],c["active"],c["direction"],c["input"],c["launch"]);
            check(r==c["output"],"Original projectile frame "+c["id"].dump()+" "+r.dump());
            check(state.sram()==read(base+c["after"].get<std::string>()),"Original projectile SRAM "+c["id"].dump());
        }
    }
    {
        std::ifstream file(base+"clash-death.json");
        for(const auto &c:Json::parse(file)){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));auto units=c["units_before"];
            check(state.clash_resolve_hit(units,c["active"],c["target"],c["orientation"],c["tactical_side"]).empty(),"Resolve damage animation");
            check(units==c["units_after"]&&state.sram()==read(base+c["after"].get<std::string>()),"Original death and kill counters "+c["id"].dump());
        }
        std::ifstream handover(base+"clash-handover.json");OriginalState state(rom,rom.initial_sram(0));
        for(const auto &c:Json::parse(handover))check(state.clash_handover(c["units"],c["input"])==c["output"],"Original army handover and AI dispatch "+c["id"].dump());
    }
    {
        std::ifstream file(base+"clash-terminal.json");
        for(const auto &c:Json::parse(file)){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));
            check(state.clash_end_action(c["units"],c["first"],c["second"],c["scene"],c["active"],c["extra"],false)==c["output"],"Commander death enters result phase "+c["id"].dump());
            check(state.sram()==read(base+c["after"].get<std::string>()),"Terminal clash repaint");
        }
        std::ifstream flights(base+"projectile.json");const auto c=Json::parse(flights)[0];
        const auto raw=read(base+c["before"].get<std::string>());OriginalState state(rom,raw);
        auto flight=c["input"];flight["x"]=std::uint64_t(4294967296ULL);
        check(state.clash_projectile(c["units"],c["active"],c["direction"],flight,true).contains("error")&&state.sram()==raw,"Invalid projectile byte rejected atomically");
        auto units=c["units"];const auto unchanged=units;
        check(!state.clash_resolve_hit(units,0,255,0,128).empty()&&units==unchanged&&state.sram()==raw,"Missing casualty target cannot change counters");
        std::ifstream handovers(base+"clash-handover.json");const auto h=Json::parse(handovers)[0];auto runtime=h["input"];runtime["active"]=15;
        check(state.clash_handover(h["units"],runtime).contains("error")&&state.sram()==raw,"Invalid handover sentinel rejected");
    }
    {
        std::ifstream file(base+"clash-strategy.json");const auto cases=Json::parse(file);
        for(const auto &c:cases){
            const auto raw=read(base+c["before"].get<std::string>());OriginalState state(rom,raw);std::uint8_t cursor=c["cursor_before"];
            const auto decision=state.clash_strategy(c["units"],c["orders"],c["context"],cursor);
            check(decision==c["output"]&&cursor==c["cursor_after"],"Original army strategy "+c["id"].dump()+" actual "+decision.dump()+" rng "+std::to_string(cursor)+" expected "+c["output"].dump()+" rng "+c["cursor_after"].dump());
            check(state.sram()==raw,"Army strategy must not mutate SRAM");
        }
        std::ifstream actions_file(base+"strategy-orders.json");
        for(const auto &action:Json::parse(actions_file)){
            const auto c=cases[action["strategy_id"].get<int>()];const auto raw=read(base+c["before"].get<std::string>());OriginalState state(rom,raw);std::uint8_t cursor=c["cursor_before"];
            const auto strategy=state.clash_strategy(c["units"],c["orders"],c["context"],cursor);
            check(!strategy.contains("error"),"Army strategy output feeds unit orders");
            const auto decision=state.clash_order(c["units"],strategy["orders"],action["active"],action["second_token"],action["command"],cursor);
            check(decision==action["output"]&&cursor==action["cursor_after"]&&state.sram()==raw,"Composed original army strategy and unit decision "+action["id"].dump()+" "+decision.dump());
        }
        const auto c=cases[0];const auto raw=read(base+c["before"].get<std::string>());OriginalState state(rom,raw);std::uint8_t cursor=255;
        auto context=c["context"];context["tail"][0]=std::uint64_t(4294967296ULL);
        check(state.clash_strategy(c["units"],c["orders"],context,cursor).contains("error")&&cursor==255&&state.sram()==raw,"Invalid strategy scratch byte cannot consume randomness");
        context=c["context"];context["first"]=241;
        check(state.clash_strategy(c["units"],c["orders"],context,cursor).contains("error")&&cursor==255,"Invalid strategy officer rejected before random draws");
    }
    {
        std::ifstream file(base+"clash-injury.json");OriginalState state(rom,rom.initial_sram(0));const auto raw=state.sram();
        for(const auto &c:Json::parse(file)){
            auto units=c["units"];std::uint8_t cursor=c["cursor_before"];
            const auto result=state.clash_retreat_injury(units,c["active"],cursor);
            check(result==c["output"]&&units==c["units_after"]&&cursor==c["cursor_after"]&&state.sram()==raw,"Original pursuit injury "+c["id"].dump()+" "+result.dump());
        }
        std::ifstream losses(base+"clash-desertion.json");const auto cases=Json::parse(losses);
        for(const auto &c:cases){
            OriginalState retreat(rom,read(base+c["before"].get<std::string>()));auto units=c["units"];std::uint8_t cursor=c["cursor_before"];
            const auto result=retreat.clash_retreat_desertion(units,c["context"],cursor);
            check(result==c["output"]&&units==c["units_after"]&&cursor==c["cursor_after"],"Original retreat desertion "+c["id"].dump()+" "+result.dump()+" rng "+std::to_string(cursor));
            check(retreat.sram()==read(base+c["after"].get<std::string>()),"Original persistent desertion losses and immediate/no-message settlement "+c["id"].dump());
        }
        const auto c=cases[0];auto units=c["units"];std::uint8_t cursor=255;const auto before=units;
        check(state.clash_retreat_injury(units,1,cursor).contains("error")&&units==before&&cursor==255&&state.sram()==raw,"Ordinary unit cannot invoke commander retreat injury");
        auto context=c["context"];context["first_token"]=255;
        check(state.clash_retreat_desertion(units,context,cursor).contains("error")&&units==before&&cursor==255&&state.sram()==raw,"Invalid retreat ledger cannot consume RNG or remove troops");
    }
    {
        std::ifstream file(base+"retreat-confirm.json");const auto cases=Json::parse(file);
        for(const auto &c:cases){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));auto units=c["units"];std::uint8_t cursor=c["cursor"];
            const auto result=state.clash_retreat_confirm(units,c["context"],cursor);
            check(result==c["output"]&&units==c["units"]&&cursor==c["cursor"],"Original retreat confirmation branch "+c["id"].dump()+" "+result.dump());
            check(state.sram()==read(base+c["after"].get<std::string>()),"Original retreat confirmation SRAM "+c["id"].dump());
        }
        std::ifstream losses(base+"clash-desertion.json");
        for(const auto &c:Json::parse(losses)){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));auto units=c["units"],context=c["context"];std::uint8_t cursor=c["cursor_before"];
            context.update({{"orientation",0},{"tactical_side",128},{"counter",4}});
            const auto result=state.clash_retreat_confirm(units,context,cursor);
            check(result==c["output"]&&units==c["units_after"]&&cursor==c["cursor_after"]&&state.sram()==read(base+c["after"].get<std::string>()),"Confirmation dispatches the original desertion branch "+c["id"].dump());
        }
        const auto c=cases[0];const auto raw=read(base+c["before"].get<std::string>());OriginalState state(rom,raw);auto units=c["units"],context=c["context"];std::uint8_t cursor=255;
        context["counter"]=std::uint64_t(4294967296ULL);
        check(state.clash_retreat_confirm(units,context,cursor).contains("error")&&state.sram()==raw&&units==c["units"]&&cursor==255,"Invalid confirmation phase is atomic");
        context["counter"]=3;
        check(state.clash_retreat_confirm(units,context,cursor).contains("error")&&state.sram()==raw,"Living retreating general cannot increment defeat statistics");
    }
    for(const auto &name:{"tactical-restore","tactical-repaint"}){
        std::ifstream file(base+name+".json");
        for(const auto &c:Json::parse(file)){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));
            const auto error=std::string(name)=="tactical-restore"?state.restore_tactical_terrain(c["target"]):state.repaint_tactical_unit(c["token"]);
            check(error.empty()&&state.sram()==read(base+c["after"].get<std::string>()),"Original tactical reconstruction "+std::string(name)+" "+c["id"].dump()+" "+error);
        }
    }
    {
        std::ifstream file(base+"surrender-return.json");
        for(const auto &c:Json::parse(file)){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));auto units=c["units"],captives=c["captives"];std::uint8_t cursor=c["cursor_before"];
            const auto result=state.clash_surrender(units,captives,c["context"],cursor);
            check(!result.contains("error")&&result["phase"]==11&&units==c["settled_units"]&&captives==c["captives_after"]&&cursor==c["cursor_settled"]&&state.sram()==read(base+c["settled"].get<std::string>()),"Natural surrender settlement retains the original captive ledger");
            check(state.restore_tactical_board(c["context"]["target"]).empty()&&state.sram()==read(base+c["after"].get<std::string>()),"Natural surrender-to-tactical path matches all SRAM bytes");
            const auto raw=state.sram();
            check(!state.restore_tactical_terrain(30).empty()&&!state.repaint_tactical_unit(255).empty()&&state.sram()==raw,"Invalid tactical restore request is atomic");
            auto broken=raw;broken[0xdab]=255;OriginalState incomplete(rom,broken);
            check(!incomplete.restore_tactical_board(c["context"]["target"]).empty()&&incomplete.sram()==broken,"Incomplete deployment cannot partially replace the battle board");
        }
    }
    for(const auto &name:{"clash-surrender","general-defeat","defeat-result"}){
        std::ifstream file(base+name+".json");
        for(const auto &c:Json::parse(file)){
            OriginalState state(rom,read(base+c["before"].get<std::string>()));Json result;
            if(std::string(name)=="clash-surrender"){
                auto units=c["units_before"],captives=c["captives_before"];std::uint8_t cursor=c["cursor_before"];
                result=state.clash_surrender(units,captives,c["context"],cursor);
                check(units==c["units_after"]&&captives==c["captives_after"]&&cursor==c["cursor_after"],"Surrender army, captive ledger and randomness "+c["id"].dump()+" "+result.dump());
            }else if(std::string(name)=="general-defeat")result=state.clash_general_defeat(c["first"],c["second"],c["active"],c["orientation"],c["tactical_side"]);
            else result=state.finish_clash_defeat(c["units"],c["context"]);
            check(result==c["output"],"Original result transition "+std::string(name)+" "+c["id"].dump()+" "+result.dump());
            check(state.sram()==read(base+c["after"].get<std::string>()),"Original result SRAM "+std::string(name)+" "+c["id"].dump());
        }
    }
    {
        std::ifstream file(base+"clash-surrender.json");const auto c=Json::parse(file)[0];
        const auto raw=read(base+c["before"].get<std::string>());OriginalState state(rom,raw);
        auto units=c["units_before"],captives=c["captives_before"],context=c["context"];std::uint8_t cursor=255;
        context["active"]=std::uint64_t(4294967296ULL);
        check(state.clash_surrender(units,captives,context,cursor).contains("error")&&units==c["units_before"]&&captives==c["captives_before"]&&cursor==255&&state.sram()==raw,"Oversized surrender byte rejected atomically");
        context=c["context"];
        // Force a failure in the second settlement, after troop counts and loyalty
        // would have changed. A missing defender must roll everything back.
        context["active"]=(context["first_token"].get<int>()&32)?128:0;
        auto broken=raw;const int city=context["target"];for(int i=0;i<12;++i)broken[city*36+16+i]=255;
        OriginalState missing(rom,broken);
        check(missing.clash_surrender(units,captives,context,cursor).contains("error")&&units==c["units_before"]&&captives==c["captives_before"]&&cursor==255&&missing.sram()==broken,"Second settlement rejection rolls back troop counts, loyalty, captives and RNG");
        context=c["context"];
        check(state.finish_clash_defeat(units,context).contains("error")&&state.sram()==raw,"Living general cannot trigger defeat settlement");
        check(state.clash_general_defeat(145,12,255,0,128).contains("error")&&state.sram()==raw,"Invalid winner cannot alter general defeat statistics");
    }
    std::ifstream mobility_file(base+"tactical-mobility.json");
    for(const auto &c:Json::parse(mobility_file))check(rom.tactical_mobility(c["units"])==c["points"],"Original unit-count mobility");
    for(const auto &name:{"clash-roster","clash-damage","clash-map","clash-result"}){
        std::ifstream file(base+name+".json");
        for(const auto &c:Json::parse(file)){
            if(std::string(name)=="clash-map"){check(rom.clash_map(c["id"])==c["terrain"],"Original clash terrain "+c["id"].dump());continue;}
            const auto raw=read(base+c["before"].get<std::string>());OriginalState state(rom,raw);
            if(std::string(name)=="clash-result"){
                check(state.settle_clash(c["units"],c["first"],c["second"],c["first_token"],c["second_token"],c["target"]).empty(),"Settle clash results");
                check(state.sram()==read(base+c["after"].get<std::string>()),"Original clash results "+c["id"].dump());continue;
            }
            if(std::string(name)=="clash-roster"){
                check(state.clash_units(c["first"],c["second"])==c["units"],"Original clash roster "+c["id"].dump());
                for(int side=0;side<2;++side){
                    int counts[3]={};for(int slot=1;slot<11;++slot){const int type=c["units"][side*33+slot*3];if(type!=255)++counts[type&3];}
                    const auto officer=state.officer(c[side?"second":"first"]);
                    check(officer["infantry"]==counts[0]*100&&officer["cavalry"]==counts[1]*100&&officer["archers"]==counts[2]*100,"Displayed troop names match original melee, cavalry and projectile classes");
                }
            }
            else {
                auto units=c["units_before"];std::uint8_t cursor=c["cursor_before"];
                check(state.clash_damage(units,c["first"],c["second"],c["attacker"],c["target"],cursor).empty(),"Apply clash damage");
                check(units==c["units_after"]&&cursor==c["cursor_after"],"Original clash damage and randomness "+c["id"].dump());
            }
            check(state.sram()==raw,"Clash roster/damage keeps strategic records until result settlement");
        }
    }
    {
        std::ifstream file(base+"clash-damage.json");const auto c=Json::parse(file)[0];
        const auto raw=read(base+c["before"].get<std::string>());OriginalState state(rom,raw);
        auto units=c["units_before"];std::uint8_t cursor=255;const auto before=units;
        check(!state.clash_damage(units,c["first"],c["second"],c["attacker"],2,cursor).empty()&&units==before&&cursor==255,"Friendly damage rejected without consuming RNG");
        units[0]=std::uint64_t(4294967296ULL);const auto invalid=units;
        check(!state.clash_damage(units,c["first"],c["second"],c["attacker"],c["target"],cursor).empty()&&units==invalid&&cursor==255,"Oversized unit byte cannot wrap through damage validation");
        check(!state.settle_clash(units,c["first"],c["second"],0x70,0x14,12).empty()&&state.sram()==raw,"Invalid result is atomic");
        auto overflow=raw;overflow[0x438+145*8+4]=240;OriginalState oversized(rom,overflow);
        check(oversized.clash_units(145,12).contains("error")&&oversized.sram()==overflow,"More than ten troop units rejected without corrupting adjacent army");
    }
    OriginalSession session(rom);session.start(4,0);for(int i=0;i<24&&session.save()["phase"]=="ai_turn";++i)session.advance();
    for(int city=0;city<30;++city){
        auto raw=read(base+"autodeploy-"+std::to_string(city)+"-before.bin");
        // These AI-routine fixtures retain city roster holes deliberately;
        // the player expedition starts with an empty defender ledger.
        for(int i=0;i<24;++i)raw[0xdaa+i]=255;
        OriginalState placement(rom,raw);
        const int current=placement.prepare_deployment(13,city);
        check(placement.valid_deployment(13,city,current),"All thirty cities have consistent deployment tables: "+std::to_string(city));
    }
    check(!session.dispatch_expedition(13,12,{145},145).contains("error"),"Dispatch expedition");
    const auto before=session.save();check(session.prepare_deployment().empty(),"Begin manual deployment");
    auto save=session.save();check(save["battle"]["deployment"]["current"]==0,"First attacker active");
    check(save["sram"][0xd9f]==before["sram"][0xd9f],"Deployment spends no additional books");
    OriginalSession restored(rom);check(restored.restore(save).empty()&&restored.save()==save,"Resume active deployment");
    check(!restored.prepare_deployment().empty()&&restored.save()==save,"Cannot restart deployment");
    check(restored.move_deployment(1).empty(),"Move within original deployment area");
    save=restored.save();check(session.restore(save).empty(),"Restore moved unit");
    auto invalid=save;invalid["sram"][0xdc3]=0;
    check(!session.restore(invalid).empty()&&session.save()==save,"Invalid position restore atomic");
    invalid=save;invalid["sram"][0xe1a]=16;
    check(!session.restore(invalid).empty()&&session.save()==save,"Forged occupancy restore atomic");
    check(session.confirm_deployment().empty(),"Confirm final attacker");save=session.save();
    check(save["battle"]["deployment"]["current"]==-1&&restored.restore(save).empty(),"Completed deployment survives save");
    check(!session.confirm_deployment().empty()&&!session.end_turn().empty()&&session.save()==save,"No repeated confirm or skipping unfinished combat");
    auto controlled=before;controlled["format"]="native-original-v1";controlled["sram"]=read(base+"expedition-2-before.bin");
    check(session.restore(controlled).empty(),"Three-unit controlled campaign");
    check(!session.dispatch_expedition(13,12,{143,144,145},143).contains("error")&&session.prepare_deployment().empty(),"Three attackers enter deployment");
    for(int slot=0;slot<3;++slot){
        check(session.save()["battle"]["deployment"]["current"]==slot,"Original sequential unit order");
        save=session.save();check(restored.restore(save).empty()&&restored.save()==save,"Each active unit can resume");
        check(session.confirm_deployment().empty(),"Advance to next original candidate square");
    }
    save=session.save();check(restored.restore(save).empty()&&save["battle"]["deployment"]["current"]==-1,"All three placements retained");
    invalid=save;invalid["battle"]["deployment"]["current"]=0;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Cannot reopen confirmed units by forging progress");
    // Start at the independently captured AI invasion boundary (Sun Quan -> Liu Bei).
    std::ifstream ai_file(base+"ai.json");const auto ai_case=Json::parse(ai_file)[15];
    auto invasion=session.save();invasion["phase"]="battle";invasion["sram"]=read(base+"ai-15-after.bin");
    invasion["ai"]=ai_case["expected"];invasion["battle"]=invasion["ai"];invasion["random_cursor"]=49;
    check(session.restore(invasion).empty(),"Legacy AI invasion can resume");
    check(session.prepare_deployment().empty(),"Human defense enters manual deployment");
    save=session.save();
    check(save["sram"]==read(base+"defense-prepare-after.bin"),"Session defense preparation matches original SRAM");
    check(save["battle"]["deployment"]["side"]=="defender"&&save["sram"][0xdc3]==255,"AI attackers wait for human defense");
    check(restored.restore(save).empty()&&restored.save()==save,"Human defender progress restores");
    check(session.move_deployment(1).empty(),"Move human defender");
    save=session.save();check(restored.restore(save).empty(),"Moved defender progress restores");
    invalid=save;invalid["battle"]["deployment"]["current"]=12;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Out-of-range defender cursor rejected atomically");
    invalid=save;invalid["sram"][0xdc3]=0x7c;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Cannot deploy AI before defender confirmation");
    invalid=save;invalid["sram"][0xdab]=0;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Invalid human defense position rejected");
    invalid=save;invalid["sram"][0xe1a]=16;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Forged defender occupancy rejected");
    check(session.move_deployment(0).empty(),"Return defender to original square");
    check(session.confirm_deployment().empty()&&session.save()["sram"]==read(base+"defense-confirm-after.bin"),"Next defender matches original confirmation");
    save=session.save();check(restored.restore(save).empty(),"Second defender restores");
    check(session.confirm_deployment().empty()&&session.save()["sram"]==read(base+"defense-finish-after.bin"),"Final defense confirms and auto-deploys original attackers");
    save=session.save();check(restored.restore(save).empty()&&restored.save()==save,"Completed defense restores with original AI commander");
    invalid=save;invalid["sram"][0xe04]=1;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Forged AI commander role rejected");
    invalid=save;invalid["battle"]["deployment"]["side"]="attacker";
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Wrong deployment side rejected");
    check(!session.confirm_deployment().empty()&&!session.move_deployment(0).empty()&&!session.end_turn().empty()&&session.save()==save,"Completed defense cannot skip combat or rotate AI");
    for(int city=0;city<30;++city){
        auto raw=read(base+"defense-prepare-before.bin");
        std::fill(raw.begin()+0xdaa,raw.begin()+0xdc2,255);
        OriginalState defense(rom,raw);const int source=city==24?18:24;
        const std::vector<std::uint8_t> attackers(raw.begin()+0xdc2,raw.begin()+0xde3);
        int current=defense.prepare_deployment(source,city,true);
        check(defense.valid_defense_deployment(source,city,current,attackers),"Thirty-city defense starts consistently: "+std::to_string(city));
        for(int i=current+1;i<12;++i)if(defense.sram()[0xdaa+i*2]!=255){
            defense.start_deployment_unit(city,i,true);
            check(defense.valid_defense_deployment(source,city,i,attackers),"Thirty-city sequential defense: "+std::to_string(city));
        }
        defense.finish_defense_deployment(city);
        check(defense.valid_defense_deployment(source,city,-1,attackers),"Thirty-city completed defense: "+std::to_string(city));
    }
    std::ifstream pvp_file(base+"pvp.json"),expedition_file(base+"expedition.json");
    const auto pvp_cases=Json::parse(pvp_file),expedition_cases=Json::parse(expedition_file);
    for(const auto &c:pvp_cases){
        const auto kind=c["kind"].get<std::string>();
        if(kind=="prepare"){
            auto raw=read(base+expedition_cases[c["scenario"].get<int>()]["before"].get<std::string>());
            raw[0xd89]=c["swapped"].get<bool>()?130:132;raw[0xd8a]=c["swapped"].get<bool>()?132:130;
            auto initial=session.save();initial["format"]="native-original-v1";initial["sram"]=raw;
            check(session.restore(initial).empty(),"Two-player command fixture");
            const auto &exp=expedition_cases[c["scenario"].get<int>()];
            check(!session.dispatch_expedition(13,12,exp["officers"].get<std::vector<int>>(),exp["leader"]).contains("error"),"Dispatch against other human");
            save=session.save();check(restored.restore(save).empty(),"Unprepared PvP save");
            check(session.prepare_deployment().empty(),"Human defender begins PvP");
        }else if(kind=="attacker_begin")check(session.prepare_deployment().empty(),"Other player accepts handover");
        else if(kind!="wrong_port")check(session.confirm_deployment().empty(),"PvP sequential confirmation");
        save=session.save();
        check(save["sram"]==read(base+c["after"].get<std::string>()),"Session PvP full SRAM: "+c["id"].dump());
        check(restored.restore(save).empty()&&restored.save()==save,"Every PvP stage saves and resumes: "+c["id"].dump());
        if(kind=="handover"){
            check(save["battle"]["deployment"]["handover"]==true&&save["battle"]["deployment"]["current"]==-1,"Explicit inter-player handover");
            check(!session.move_deployment(0).empty()&&!session.confirm_deployment().empty()&&session.save()==save,"No unit commands while waiting for handover");
            invalid=save;invalid["battle"]["deployment"]["handover"]=false;
            check(!restored.restore(invalid).empty()&&restored.save()==save,"Cannot claim complete with attackers unplaced");
        }
        invalid=save;invalid["battle"]["deployment"]["side"]=save["battle"]["deployment"]["side"]=="defender"?"attacker":"defender";
        check(!restored.restore(invalid).empty()&&restored.save()==save,"Reject forged PvP side");
        if(kind=="finish")check(!session.prepare_deployment().empty()&&!session.end_turn().empty()&&session.save()==save,"Completed PvP does not restart or skip combat");
    }
    // Capacity and area integration: all 30 maps, 12 manual defenders and 11 attackers.
    for(int city=0;city<30;++city){
        auto raw=read(base+"pvp-0-before.bin");
        for(int i=0;i<24;++i)raw[0xdaa+i]=255;
        for(int i=0;i<12;++i)raw[city*36+16+i]=100+i;
        for(int i=0;i<11;++i){raw[0xdc2+i*3]=160+i;raw[0xdc3+i*3]=255;raw[0xdc4+i*3]=13|(i==0?128:0);}
        OriginalState battle(rom,raw);const int source=city==13?12:13;
        check(battle.prepare_deployment(source,city,true,true)==0,"Full-capacity PvP first defender");
        for(int i=1;i<12;++i)battle.start_deployment_unit(city,i,true);
        check(battle.valid_pvp_deployment(source,city,-1,false,true),"Full-capacity PvP handover on city "+std::to_string(city));
        for(int i=0;i<11;++i)battle.start_deployment_unit(city,i,false);
        check(battle.valid_pvp_deployment(source,city,-1,false,false),"All 23 units fit original areas on city "+std::to_string(city));
    }
    auto tactical_start=session.save();tactical_start["format"]="native-original-v1";tactical_start["sram"]=read(base+"expedition-2-before.bin");
    check(session.restore(tactical_start).empty(),"Tactical command fixture");
    check(!session.dispatch_expedition(13,12,{143,144,145},143).contains("error"),"Tactical expedition");
    check(!session.begin_tactics().empty(),"Cannot skip deployment into tactics");
    check(session.prepare_deployment().empty(),"Prepare tactical expedition");
    for(int i=0;i<3;++i)check(session.confirm_deployment().empty(),"Complete tactical deployment");
    save=session.save();check(session.begin_tactics().empty(),"Begin attacker tactical turn");
    check(session.save()["sram"]==save["sram"]&&session.save()["battle"]["tactics"]["points"]==20,"Three attackers have original 20 shared mobility and unchanged SRAM");
    check(session.move_tactical(0,1).empty(),"Tactical left move");save=session.save();
    check(restored.restore(save).empty()&&restored.save()==save,"Tactical movement replays from saved deployment");
    invalid=save;invalid["battle"]["tactics"]["points"]=40;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Forged mobility rejected atomically");
    invalid=save;invalid["sram"][0xdc3]=0;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Forged tactical position rejected");
    invalid=save;invalid["battle"]["tactics"]["moves"][0]["direction"]=4;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Invalid replay direction rejected");
    check(session.select_tactical_unit(1).empty(),"Select another attacker sharing mobility");save=session.save();
    check(restored.restore(save).empty(),"Selected tactical unit persists");
    check(!session.move_tactical(10,0).empty()&&!session.begin_tactics().empty()&&!session.end_turn().empty()&&session.save()==save,"Cannot select empty slot, restart points or skip unresolved combat");
    // Find a reachable enemy from a valid deployment, then exercise all session boundaries.
    check(session.restore(tactical_start).empty(),"Fresh attack integration state");
    check(!session.dispatch_expedition(13,12,{143,144,145},143).contains("error"),"Attack integration expedition");
    check(session.prepare_deployment().empty(),"Attack integration deployment");
    for(int i=0;i<2;++i)check(session.move_deployment(2).empty(),"Approach during deployment");
    while(session.move_deployment(1).empty()){}
    for(int i=0;i<3;++i)check(session.confirm_deployment().empty(),"Attack integration deployment finish");
    check(session.begin_tactics().empty(),"Attack integration tactics");
    auto support_start=session.save();const int support_points=support_start["battle"]["tactics"]["points"];
    check(session.adjust_tactical_formation(0,0).empty(),"Change formation in tactical session");save=session.save();
    check(save["battle"]["tactics"]["points"]==support_points&&restored.restore(save).empty(),"Free formation replays on load");
    invalid=save;invalid["sram"][0x438+save["sram"][0xdc2].get<int>()*8+4]=255;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Forged formation rejected atomically");
    check(session.scout_tactical(0,0).empty(),"Scout distant defender");save=session.save();
    check(save["battle"]["tactics"]["points"]==support_points-1&&restored.restore(save).empty()&&restored.save()==save,"Scouting costs one point and report replays");
    check(!session.move_tactical(0,0).empty()&&!session.adjust_tactical_formation(0,0).empty()&&!session.scout_tactical(0,0).empty()&&!session.attack_tactical(0,0).empty()&&session.save()==save,"Report prevents conflicting commands and double payment");
    check(session.close_tactical_scout().empty()&&restored.restore(session.save()).empty(),"Close report without refund and save");
    check(session.restore(support_start).empty(),"Restore clean attack approach");
    std::vector<Json> queue{session.save()};std::array<int,160> best{};best.fill(-1);bool found=false;
    for(std::size_t i=0;i<queue.size()&&!found;++i){
        check(session.restore(queue[i]).empty(),"Replay attack approach");
        for(int direction=0;direction<4&&!found;++direction){
            if(session.attack_tactical(0,direction).empty()){found=true;break;}
            check(session.restore(queue[i]).empty(),"Reset rejected attack approach");
            if(session.move_tactical(0,direction).empty()){
                auto moved=session.save();int cell=moved["sram"][0xdc3],points=moved["battle"]["tactics"]["points"];
                if(points>best[cell]){best[cell]=points;queue.push_back(moved);}
            }
            check(session.restore(queue[i]).empty(),"Reset movement branch");
        }
    }
    check(found,"Valid battlefield approach reaches adjacent enemy");save=session.save();
    check(restored.restore(save).empty()&&restored.save()==save,"Pending attack survives exact replay");
    invalid=save;invalid["battle"]["tactics"]["attack"]["defender"]=0;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Forged attack rejected atomically");
    check(!session.move_tactical(0,0).empty()&&!session.select_tactical_unit(1).empty()&&!session.attack_tactical(0,0).empty()&&!session.cycle_clash_order(0,0).empty()&&!session.advance_clash().empty()&&session.save()==save,"Pending attack rejects movement, switching, repeat payment and premature clash commands atomically");
    const int spent_points=save["battle"]["tactics"]["points"];
    check(session.cancel_tactical_attack().empty()&&session.save()["battle"]["tactics"]["points"]==spent_points,"Cancel does not refund original attack cost");
    check(restored.restore(session.save()).empty(),"Cancelled attack replays");
    check(session.restore(save).empty(),"Resume confirmation");session.tick(7);
    check(session.confirm_tactical_attack().empty(),"Prepare original clash");save=session.save();
    check(save["battle"]["tactics"]["attack"]["stage"]=="clash_ready"&&restored.restore(save).empty()&&restored.save()==save,"Frame-dependent clash preparation survives save");
    check(!session.confirm_tactical_attack().empty()&&!session.cancel_tactical_attack().empty()&&!session.end_turn().empty()&&session.save()==save,"Cannot reinitialize, cancel or skip unresolved realtime combat");
    invalid=save;invalid["battle"]["tactics"]["attack"]["clash"]["first"]=0;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Forged clash orientation rejected");
    const auto legacy_clash=save;
    check(session.clash_rgb().empty()&&session.begin_clash().empty(),"Enter native clash armies from legacy preparation");save=session.save();
    check(restored.restore(save).empty()&&restored.save()==save&&session.clash_rgb().size()==256*160*3,"Clash map and armies survive session replay");
    const auto &players=save["battle"]["tactics"]["attack"]["clash"]["players"];
    const int human=players[0]==0?1:0,computer=human^1;
    check(!session.begin_clash().empty()&&!session.cycle_clash_order(computer,0).empty()&&!session.cycle_clash_order(0,4).empty()&&session.save()==save,"Reject repeated startup and computer orders atomically");
    const auto initial_clash=save;
    const auto pixels=session.clash_rgb();
    for(int kind=0;kind<4;++kind)check(session.cycle_clash_order(human,kind).empty(),"Cycle human troop orders");
    const auto orders_save=session.save();
    check(session.clash_rgb()==pixels&&orders_save["sram"]==save["sram"]&&orders_save["random_cursor"]==save["random_cursor"]&&restored.restore(orders_save).empty()&&restored.save()==orders_save,"Orders persist without advancing troops or randomness");
    for(const auto key:{"units","orders","phase","countdown"}){
        invalid=orders_save;auto &v=invalid["battle"]["tactics"]["attack"]["clash"]["runtime"][key];if(v.is_array())v[0]=123;else v=123;
        check(!restored.restore(invalid).empty()&&restored.save()==orders_save,"Reject forged clash runtime atomically");
    }
    invalid=orders_save;invalid["sram"][0xe1a+80]=255;
    check(!restored.restore(invalid).empty()&&restored.save()==orders_save,"Reject forged clash occupancy");
    invalid=orders_save;invalid["battle"]["tactics"]["moves"].back()["unit_kind"]=std::uint64_t(1)<<32;
    check(!restored.restore(invalid).empty()&&restored.save()==orders_save,"Reject oversized order history field");
    check(restored.restore(legacy_clash).empty()&&restored.save()==legacy_clash,"Legacy clash-ready save remains compatible");
    {
        OriginalSession fighter(rom);check(fighter.restore(initial_clash).empty(),"Load initial armies for automatic execution");
        check(fighter.advance_clash().empty()&&fighter.advance_clash().empty(),"Dispatch mirrored first-army AI");
        check(fighter.save()["battle"]["tactics"]["attack"]["boundary"]=="ai_scratch","Unknown first-army scratch is held without assumptions");
        const auto scratch_boundary=fighter.save();check(restored.restore(scratch_boundary).empty()&&restored.save()==scratch_boundary,"Unknown scratch boundary saves and replays");
        check(!fighter.advance_clash().empty()&&fighter.save()==scratch_boundary,"Cannot skip missing original scratch");
        std::vector<Json> approaches{support_start};std::array<int,160> mobility{};mobility.fill(-1);bool ready=false;
        for(std::size_t i=0;i<approaches.size()&&!ready;++i){
            for(int direction=0;direction<4&&!ready;++direction){
                check(fighter.restore(approaches[i]).empty(),"Restore alternate terrain approach");
                if(fighter.attack_tactical(0,direction).empty()){
                    const auto pending=fighter.save()["battle"]["tactics"]["attack"];
                    if(!(rom.clash_terrain(pending["terrain_pair"])["orientation"].get<int>()&128)){
                        fighter.tick(7);check(fighter.confirm_tactical_attack().empty()&&fighter.begin_clash().empty(),"Original terrain places player army first");ready=true;break;
                    }
                }
                check(fighter.restore(approaches[i]).empty(),"Restore alternate movement branch");
                if(fighter.move_tactical(0,direction).empty()){
                    auto branch=fighter.save();const int cell=branch["sram"][0xdc3],points=branch["battle"]["tactics"]["points"];
                    if(points>mobility[cell]){mobility[cell]=points;approaches.push_back(std::move(branch));}
                }
            }
        }
        check(ready,"Valid original battlefield approach reaches player-first clash");
        const auto execution_start=fighter.save();
        std::ifstream saved_start(std::string(ZHONGYUAN_PROJECT_DIR)+"/tests/clash-session-start.json");
        check(Json::parse(saved_start)==execution_start,"Godot execution fixture is generated by legal native actions");
        const auto original_units=execution_start["battle"]["tactics"]["attack"]["clash"]["runtime"]["units"];
        bool moved=false,damaged=false,computer_turn=false;int steps=0;Json counters=Json::array();
        for(;steps<400;++steps){
            const auto current=fighter.save();const auto stage=current["battle"]["tactics"]["attack"]["stage"];
            if(stage!="clash_orders"&&stage!="clash_running")break;
            check(fighter.advance_clash().empty(),"Advance original execution from valid session");
            const auto running=fighter.save();const auto &rt=running["battle"]["tactics"]["attack"]["clash"]["runtime"];
            computer_turn|=rt["phase"]==3;counters.push_back(Json::array({rt["phase"],rt["counter"]}));
            for(int i=0;i<66;i+=3){moved|=original_units[i+2]!=rt["units"][i+2];damaged|=original_units[i+1]!=rt["units"][i+1];}
            check(running["battle"]["tactics"]["points"]==execution_start["battle"]["tactics"]["points"],"Clash steps cannot spend tactical mobility");
            if(steps==12||steps==40)check(restored.restore(running).empty()&&restored.save()==running,"Mid-action save replays exactly");
        }
        const auto stopped=fighter.save();const auto &attack=stopped["battle"]["tactics"]["attack"];
        std::cout<<"Clash integration: "<<steps<<" steps, moved="<<moved<<", damaged="<<damaged<<", AI="<<computer_turn<<", stage="<<attack["stage"]<<" boundary="<<attack.value("boundary",Json{})<<"\n";
        check(moved&&damaged&&computer_turn,"Automatic execution moves units, applies damage and dispatches computer strategy");
        check(restored.restore(stopped).empty()&&restored.save()==stopped,"Final execution boundary replays exactly");
        for(const auto key:{"units","target","extra","global_phase","ai_side"}){
            invalid=stopped;auto &v=invalid["battle"]["tactics"]["attack"]["clash"]["runtime"][key];if(v.is_array())v[2]=22;else v=254;
            check(!restored.restore(invalid).empty()&&restored.save()==stopped,"Reject forged execution runtime");
        }
        if(attack["stage"]!="clash_running")check(!fighter.advance_clash().empty()&&fighter.save()==stopped,"Execution cannot skip pending terminal phase");
        check(attack["stage"]=="surrender_notice"&&attack["clash"]["players"][attack["surrender"]["side"].get<int>()]==0,"Computer surrender enters announcement without a human consent prompt");
        check(fighter.advance_clash_surrender().empty()&&fighter.advance_clash_surrender().empty(),"Automatic clash reaches existing surrender settlement");
        const auto result=fighter.save();check(result["battle"]["tactics"]["attack"]["stage"]=="clash_result"&&restored.restore(result).empty()&&restored.save()==result,"Fighting casualties and NPC surrender settle and replay together");
        check(result["battle"]["tactics"]["captives"]!=Json(std::vector<int>(24,255)),"NPC surrender preserves the captured general");
        // Human mid-clash surrender cancellation must resume the same decision.
        check(fighter.restore(execution_start).empty(),"Restart independent human surrender branch");
        check(fighter.advance_clash().empty()&&fighter.advance_clash().empty(),"Open first player decision boundary");
        for(int i=0;i<3;++i)check(fighter.cycle_clash_order(0,3).empty(),"Select surrender during an action gap");
        const auto resume=fighter.save()["battle"]["tactics"]["attack"]["clash"]["runtime"];
        check(fighter.advance_clash().empty()&&fighter.save()["battle"]["tactics"]["attack"]["stage"]=="surrender_confirm","Human surrender requested by unit dispatcher");
        check(fighter.answer_clash_surrender(false).empty()&&fighter.save()["battle"]["tactics"]["attack"]["stage"]=="clash_running"&&fighter.save()["battle"]["tactics"]["attack"]["clash"]["runtime"]==resume,"Cancel mid-clash surrender restores exact decision state");
        check(fighter.cycle_clash_order(0,3).empty()&&fighter.advance_clash().empty()&&restored.restore(fighter.save()).empty(),"Correct order after cancellation and resume saved execution");
        check(session.save()==orders_save,"Independent execution preserves configured surrender test session");
    }
    check(!session.request_clash_surrender().empty()&&session.save()==orders_save,"Cannot surrender without a surrender order");
    check(session.cycle_clash_order(human,3).empty()&&session.cycle_clash_order(human,3).empty(),"Select ordinary general surrender order");
    const auto configured=session.save();const auto original_clash=configured["battle"]["tactics"]["attack"]["clash"];
    check(session.request_clash_surrender().empty(),"Open human surrender confirmation");save=session.save();
    check(save["sram"]==configured["sram"]&&save["battle"]["tactics"]["attack"]["clash"]["runtime"]["counter"]==2&&restored.restore(save).empty()&&restored.save()==save,"Confirmation preserves troops and resumes exactly");
    check(!session.cycle_clash_order(human,3).empty()&&!session.advance_clash_surrender().empty()&&session.save()==save,"Confirmation locks orders and acknowledgement skipping");
    check(session.answer_clash_surrender(false).empty()&&session.save()["battle"]["tactics"]["attack"]["clash"]==original_clash&&restored.restore(session.save()).empty(),"Cancel returns to configured armies without mutations");
    check(session.request_clash_surrender().empty()&&session.answer_clash_surrender(true).empty(),"Confirm opens surrender announcement");save=session.save();
    check(save["sram"]==configured["sram"]&&save["battle"]["tactics"]["attack"]["clash"]["runtime"]["counter"]==5&&restored.restore(save).empty(),"Announcement waits without premature capture");
    check(!session.answer_clash_surrender(false).empty()&&session.save()==save,"Confirmed surrender cannot be cancelled during announcement");
    check(session.advance_clash_surrender().empty(),"Acknowledge announcement and show acceptance");save=session.save();
    check(save["sram"]==configured["sram"]&&save["battle"]["tactics"]["attack"]["clash"]["runtime"]["counter"]==6&&restored.restore(save).empty(),"Acceptance still precedes persistent settlement");
    OriginalState expected(rom,save["sram"].get<std::vector<std::uint8_t>>());
    auto units=original_clash["runtime"]["units"],captives=Json(std::vector<int>(24,255)),context=original_clash;
    context["active"]=human*128;context["target"]=save["battle"]["target"];
    std::uint8_t expected_cursor=save["random_cursor"];
    auto outcome=expected.clash_surrender(units,captives,context,expected_cursor);
    check(!outcome.contains("error")&&expected.restore_tactical_board(save["battle"]["target"]).empty(),"Reference-verified kernels prepare expected session settlement");
    check(session.advance_clash_surrender().empty(),"Commit surrender and restore tactical map");save=session.save();
    check(save["sram"]==expected.sram()&&save["random_cursor"]==expected_cursor&&save["battle"]["tactics"]["captives"]==captives&&save["battle"]["tactics"]["points"]==configured["battle"]["tactics"]["points"],"Session preserves exact kernel SRAM, captives, RNG and mobility");
    check(restored.restore(save).empty()&&restored.save()==save,"Settled result survives replay after selected general leaves");
    check(!session.advance_clash_surrender().empty()&&!session.request_clash_surrender().empty()&&session.save()==save,"Settlement cannot run twice");
    for(const auto key:{"captives","selected"}){
        invalid=save;if(std::string(key)=="captives")invalid["battle"]["tactics"][key][0]=254;else invalid["battle"]["tactics"][key]=10;
        check(!restored.restore(invalid).empty()&&restored.save()==save,"Forged result metadata rejected atomically");
    }
    invalid=save;invalid["battle"]["tactics"]["attack"]["result"]["can_continue"]=false;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Forged continuation permission rejected");
    check(session.finish_clash_result().empty()&&restored.restore(session.save()).empty(),"Continue with surviving deployed armies");
    const auto resumed=session.save();
    check(!resumed["battle"]["tactics"].contains("attack")&&resumed["battle"]["tactics"]["captives"]==captives&&resumed["sram"][0xdc2]==255,"Captured general leaves ledger but stays in captive roster");
    check(session.begin_tactical_retreat(2).empty()&&session.confirm_tactical_retreat().empty(),"Commander can withdraw survivors after a capture");save=session.save();
    check(save["battle"]["tactics"]["retreat"]["ended"]==true&&restored.restore(save).empty()&&restored.save()==save,"Pending post-withdrawal captive disposition is resumable");
    check(session.finish_tactical_retreat().empty()&&session.save()["battle"]["tactics"]["turn_reason"]==36&&session.save()["battle"]["tactics"]["captives"]==captives&&restored.restore(session.save()).empty(),"Attacker withdrawal opens captive settlement without discarding prisoners");
    // A surrendered expedition commander must not be treated as an ordinary
    // loss, even while other expedition units are still deployed.
    auto commander_base=configured;
    commander_base["sram"]=configured["battle"]["tactics"]["deployment_sram"];
    commander_base["battle"].erase("tactics");commander_base["battle"]["leader"]=145;
    for(int i=0;i<11;++i){
        auto &s=commander_base["sram"];if(s[0xdc2+i*3]==255)continue;
        const bool leader=s[0xdc2+i*3]==145;
        s[0xdc4+i*3]=(s[0xdc4+i*3].get<int>()&127)|(leader?128:0);
        s[0xe1a+s[0xdc3+i*3].get<int>()]=0x30|i|(leader?64:0);
    }
    auto replay_commander_approach=[&](){
    for(const auto &step:configured["battle"]["tactics"]["moves"]){
        const auto kind=step.value("kind",std::string("move"));std::string error;
        if(kind=="move")error=session.move_tactical(step["slot"],step["direction"]);
        else if(kind=="attack")error=session.attack_tactical(step["slot"],step["direction"]);
        else if(kind=="confirm_attack")error=session.confirm_tactical_attack();
        else if(kind=="begin_clash")error=session.begin_clash();
        else if(kind=="clash_order")error=session.cycle_clash_order(step["side"],step["unit_kind"]);
        else check(false,"Unexpected commander approach action");
        check(error.empty(),"Replay valid commander approach: "+error);
    }
    };
    check(session.restore(commander_base).empty()&&session.begin_tactics().empty(),"Valid deployment with the approaching general as commander");
    replay_commander_approach();
    check(session.request_clash_surrender().empty()&&session.answer_clash_surrender(true).empty()&&session.advance_clash_surrender().empty()&&session.advance_clash_surrender().empty(),"Expedition commander surrender settles and retains captives");save=session.save();
    check(save["battle"]["tactics"]["attack"]["result"]["can_continue"]==false&&!session.finish_clash_result().empty()&&session.save()==save,"Commander departure holds final battle disposition");
    check(restored.restore(save).empty()&&restored.save()==save,"Commander departure result survives replay");
    invalid=save;invalid["battle"]["tactics"]["attack"]["result"]["can_continue"]=true;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Cannot forge a return to tactics after commander capture");
    check(session.restore(commander_base).empty()&&session.begin_tactics().empty(),"Prepare final surviving attacker case");
    for(int slot:{1,2})check(session.begin_tactical_retreat(slot).empty()&&session.confirm_tactical_retreat().empty()&&session.finish_tactical_retreat().empty(),"Other attackers return home before last commander surrenders");
    replay_commander_approach();
    check(session.request_clash_surrender().empty()&&session.answer_clash_surrender(true).empty()&&session.advance_clash_surrender().empty()&&session.advance_clash_surrender().empty(),"Last attacker surrender reaches result");save=session.save();
    check(save["sram"][0xdc2]==255&&save["sram"][0xdc5]==255&&save["sram"][0xdc8]==255&&restored.restore(save).empty()&&restored.save()==save,"Result with no selectable attackers remains loadable");
    check(restored.snapshot()["battle"].is_object()&&!restored.finish_clash_result().empty()&&restored.save()==save,"Empty-army snapshot preserves pending battle result");
    check(session.restore(support_start).empty(),"Restore retreat integration baseline");
    const int retreat_points=session.save()["battle"]["tactics"]["points"];
    check(session.begin_tactical_retreat(0).empty(),"Open ordinary retreat confirmation");save=session.save();
    check(restored.restore(save).empty()&&restored.save()==save,"Retreat confirmation resumes without charge");
    check(session.cancel_tactical_retreat().empty()&&session.save()["battle"]["tactics"]["points"]==retreat_points,"Cancel before retreat spends nothing");
    check(session.begin_tactical_retreat(0).empty()&&session.confirm_tactical_retreat().empty(),"Return ordinary attacker");save=session.save();
    check(save["battle"]["tactics"]["points"]==retreat_points-1&&save["sram"][0xdc2]==255&&restored.restore(save).empty()&&restored.save()==save,"Removed unit and charged retreat report replay");
    check(!session.confirm_tactical_retreat().empty()&&!session.move_tactical(1,0).empty()&&!session.adjust_tactical_formation(1,0).empty()&&session.save()==save,"Retreat report locks conflicting commands");
    check(session.finish_tactical_retreat().empty()&&restored.restore(session.save()).empty(),"Continue battle after ordinary retreat");
    check(session.begin_tactical_retreat(2).empty()&&session.confirm_tactical_retreat().empty(),"Commander orders remaining army home");save=session.save();
    check(save["battle"]["tactics"]["retreat"]["ended"]==true&&restored.restore(save).empty()&&restored.save()==save,"Completed retreat report persists with empty army");
    invalid=save;invalid["battle"]["tactics"]["retreat"]["ended"]=false;
    check(!restored.restore(invalid).empty()&&restored.save()==save,"Forged battle completion rejected");
    check(session.finish_tactical_retreat().empty(),"Finish retreat and resume strategic phase");save=session.save();
    check(save["phase"]=="player_commands"&&save["battle"].is_null()&&save["sram"][12*36+32]==255&&restored.restore(save).empty(),"Battle flags clear and strategic state survives save");
    check(!session.finish_tactical_retreat().empty()&&session.save()==save,"Cannot apply retreat results twice");
    std::cout<<"PASS: original automatic defense, preparation and movement fixtures; deployment/save/phase guards\n";
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
