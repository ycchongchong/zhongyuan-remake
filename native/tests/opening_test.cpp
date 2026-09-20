#include "zhongyuan/opening.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <limits>
using namespace zhongyuan;
void require(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
Opening at_quiz(){Opening o;o.press("START");o.press("A");o.press("A");o.press("A");return o;}
int main(){try{
    std::ifstream input(std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/personality-quiz.json");
    auto evidence=Json::parse(input);std::map<int,std::vector<bool>> paths{{1,{}}};
    std::queue<int> pending;pending.push(1);int count=0;std::set<int> rulers;
    while(!pending.empty()){
        int q=pending.front();pending.pop();
        for(const auto&e:evidence["edges"])if(e["question"]==q){
            auto o=at_quiz();for(bool yes:paths[q])o.answer(yes);
            require(o.snapshot()["question"]==q,"Reach observed question");
            o.answer(e["yes"]);
            require(o.snapshot()["presentation_key"]=="answer-"+std::to_string(q)+(e["yes"].get<bool>()?"-yes":"-no"),"Every original answer selects its own transition");
            if(e["next_question"].is_null()){
                require(o.snapshot()["screen"]=="result"&&o.snapshot()["ruler"]==e["ruler"],"Original result ruler");rulers.insert(e["ruler"].get<int>());
            }else{
                int next=e["next_question"];
                require(o.snapshot()["question"]==next,"Original diagnosis branch");
                if(!paths.count(next)){paths[next]=paths[q];paths[next].push_back(e["yes"]);pending.push(next);}
            }
            ++count;
        }
    }
    require(count==26&&paths.size()==13&&rulers.size()==6,"All original quiz branches covered");
    Opening o;o.press("START");o.press("DOWN");o.press("A");
    require(o.snapshot()["player_count"]==2&&o.snapshot()["screen"]=="difficulty","Two-player difficulty selection");
    o.press("A");o.press("A");for(int i=0;i<5;++i)o.answer(true);o.press("A");
    require(o.snapshot()["screen"]=="intro"&&o.snapshot()["selecting_player"]==1,"Player two introduction follows first diagnosis");
    o.press("A");for(int i=0;i<5;++i)o.answer(true);
    require(o.snapshot()["presentation_key"]=="duplicate-4","Duplicate diagnosis uses rejection animation");
    require(o.snapshot()["duplicate"]==true,"Same ruler is rejected for second player");o.press("A");o.press("A");
    for(int i=0;i<4;++i)o.answer(true);o.answer(false);
    require(o.snapshot()["first_ruler"]==4&&o.snapshot()["second_ruler"]==5&&o.press("A")=="start","Distinct rulers start two-player campaign");
    o.reset();o.press("START");o.press("UP");require(o.press("A")=="continue","Continue action");o.reset();
    o.press("A");o.press("A");o.press("DOWN");o.press("DOWN");o.press("A");require(o.snapshot()["difficulty"]==2,"Difficulty selection retained");
    Opening timing;timing.tick(17);require(timing.snapshot()["presentation_frame"]==17,"Native presentation clock advances");
    auto saved=timing.snapshot();timing.tick(0);timing.tick(-1);require(timing.snapshot()==saved,"Non-positive ticks are inert");
    timing.press("START");require(timing.snapshot()["presentation_frame"]==0,"New screen resets presentation clock");
    timing.tick(25);timing.press("DOWN");require(timing.snapshot()["presentation_frame"]==25,"Menu cursor does not restart presentation");
    timing.tick(std::numeric_limits<int>::max());timing.tick(100);require(timing.snapshot()["presentation_frame"]==std::numeric_limits<int>::max(),"Presentation clock saturates without overflow");
    timing.reset();require(timing.snapshot()["presentation_frame"]==0,"Reset clears presentation clock");
    auto quiz=at_quiz();quiz.tick(99);quiz.answer(true);require(quiz.snapshot()["presentation_frame"]==0,"Direct diagnosis answer resets presentation clock");
    std::cout<<"PASS: 13 original questions, 26 observed branches, six rulers, menu state\n";
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
