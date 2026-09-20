#include "zhongyuan/campaign.hpp"
#include <fstream>
#include <iostream>
#include <queue>
#include <set>
using namespace zhongyuan;
void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        std::ifstream input(std::string(ZHONGYUAN_PROJECT_DIR) + "/data/scenario.json");
        const auto scenario = Json::parse(input);
        Campaign game(scenario);
        std::string error;
        check(game.cities().size()==30, "Thirty playable cities");
        std::set<int> visited{13}; std::queue<int> queue; queue.push(13);
        while (!queue.empty()) { int i=queue.front(); queue.pop();
            for (int j:game.cities()[i].neighbors) if (visited.insert(j).second) queue.push(j);
        }
        check(visited.size()==30, "Every city reachable from Xinye");
        std::set<Number> owners;
        for (const auto &c:game.cities()) owners.insert(c.owner);
        check(owners==std::set<Number>{1,2,3,4,5,6}, "Six factions loaded");
        check(game.cities()[13].owner==1 && game.cities()[21].owner==1 && game.cities()[22].owner==1, "Liu Bei territory");
        const auto opening=game.snapshot();
        game.act("march",13,29);
        check(game.snapshot()==opening,"Distant city cannot be attacked");
        const auto total=game.cities()[13].troops+game.cities()[21].troops;
        game.act("march",13,21);
        check(game.cities()[13].troops+game.cities()[21].troops==total && game.orders()==2,"Friendly transfer across new map");
        Campaign loaded(scenario);
        check(loaded.load_json(game.save_json(),error) && loaded.snapshot()==game.snapshot(),"Thirty-city save round trip");
        const auto before=game.snapshot();
        for (int kind=0;kind<5;++kind) {
            auto bad=before;
            if(kind==0) bad["cities"][29]["owner"]=7;
            if(kind==1) bad["scenario_id"]="other-map";
            if(kind==2) bad["cities"].erase(29);
            if(kind==3) bad["cities"][29]["reference"]["gold"]=999;
            if(kind==4) bad["version"]=1;
            check(!game.restore(bad,error) && game.snapshot()==before,"Bad save rejected transactionally");
        }
        auto state=opening;
        for(auto &c:state["cities"]) {c["troops"]=200;c["gold"]=1000;c["grain"]=1000;}
        check(game.restore(state,error),error.c_str()); game.end_turn();
        check(game.month()==2 && game.orders()==3,"Month advances");
        for(const auto &c:game.cities()) if(c.owner>1) check(c.troops==500,"All five AI factions recruit");
        // The distant final city must count toward victory, not just the first three.
        state=opening;
        for(auto &c:state["cities"]) {c["owner"]=1;c["troops"]=200;}
        state["cities"][29]["owner"]=6;
        state["cities"][28]["troops"]=10000;
        state["cities"][28]["grain"]=1000;
        check(game.restore(state,error),error.c_str());
        game.act("develop",13); check(game.winner()==0,"Twenty-nine cities is not victory");
        game.act("march",28,29); check(game.winner()==1,"Capturing city thirty wins");
        const auto won=game.snapshot(); game.end_turn(); check(game.snapshot()==won,"Victory is terminal");
        state=opening;
        for(auto &c:state["cities"]) {c["owner"]=2;c["troops"]=200;c["grain"]=1000;}
        state["cities"][13]["owner"]=1; state["cities"][12]["troops"]=10000;
        check(game.restore(state,error),error.c_str());game.end_turn();check(game.winner()==2,"Final player city lost");
        game.reset();
        for(int turn=0;turn<100 && !game.winner();++turn) {
            for(const auto &c:game.cities()) if(c.owner==1) game.act("recruit",int(c.id));
            game.end_turn();
            for(const auto &c:game.cities()) check(c.gold>=0&&c.grain>=0&&c.troops>=0,"Nonnegative campaign resources");
        }
        std::cout<<"PASS: 30 cities, connected map, six factions, all AI, transfers, saves, victory, defeat, long campaign\n";
    } catch(const std::exception &e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
