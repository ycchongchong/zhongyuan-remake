#include "zhongyuan/opening.hpp"
#include <array>
#include <algorithm>
#include <limits>
namespace zhongyuan {
void Opening::tick(int frames){
    if(frames>0)presentation_frame_=static_cast<int>(std::min<long long>(std::numeric_limits<int>::max(),static_cast<long long>(presentation_frame_)+frames));
}
void Opening::reset(){presentation_key_.clear();presentation_frame_=0;screen_="title";cursor_=0;question_=1;ruler_=-1;difficulty_=0;players_=1;selecting_=0;first_=second_=-1;}
Json Opening::snapshot() const{return {{"presentation_key",presentation_key_},{"presentation_frame",presentation_frame_},{"screen",screen_},{"cursor",cursor_},{"question",question_},{"ruler",ruler_},{"difficulty",difficulty_},
    {"player_count",players_},{"selecting_player",selecting_},{"first_ruler",first_},{"second_ruler",second_},{"duplicate",selecting_==1&&first_==second_&&second_>=0}};}
int Opening::answer(bool yes){
    if(screen_!="quiz")return -1;
    presentation_frame_=0;
    presentation_key_="answer-"+std::to_string(question_)+(yes?"-yes":"-no");
    // Positive values select another question; negative values encode -(ruler+1).
    static constexpr std::array<std::array<int,2>,13> branches={{{2,3},{4,5},{8,5},{6,7},{4,7},{11,9},{9,6},{13,10},{12,10},{13,12},{-5,-6},{-2,-4},{-1,-3}}};
    const int next=branches[question_-1][yes?0:1];cursor_=0;
    if(next<0){ruler_=-next-1;if(selecting_==0)first_=ruler_;else second_=ruler_;screen_="result";if(selecting_==1&&first_==second_)presentation_key_="duplicate-"+std::to_string(ruler_);return ruler_;}
    question_=next;return -1;
}
std::string Opening::press(const std::string &key){
    const auto screen=screen_;const int question=question_,selecting=selecting_,ruler=ruler_;
    auto result=press_impl(key);
    if(screen!=screen_||question!=question_||selecting!=selecting_||ruler!=ruler_){
        presentation_frame_=0;
        if(screen_=="intro")presentation_key_=selecting_==1?"quiz-intro-second":"quiz-intro";
        else if(screen_!="quiz"&&screen_!="result")presentation_key_.clear();
    }
    return result;
}
std::string Opening::press_impl(const std::string &key){
    const bool confirm=key=="A"||key=="START";
    if(screen_=="title") {if(confirm){screen_="menu";cursor_=0;}return {};}
    if(screen_=="menu"||screen_=="difficulty") {
        if(key=="UP"||key=="LEFT")cursor_=(cursor_+2)%3;
        if(key=="DOWN"||key=="RIGHT")cursor_=(cursor_+1)%3;
        if(key=="B"){screen_=screen_=="menu"?"title":"menu";cursor_=0;}
        else if(confirm && screen_=="menu"){
            if(cursor_==2)return "continue";
            players_=cursor_+1;selecting_=0;first_=second_=-1;
            screen_="difficulty";cursor_=0;
        }else if(confirm && screen_=="difficulty"){
            difficulty_=cursor_;screen_="intro";cursor_=0;
        }
    }else if(screen_=="intro") {if(confirm){screen_="quiz";question_=1;cursor_=0;presentation_key_="quiz-01";}}
    else if(screen_=="quiz"){
        if(key=="LEFT"||key=="RIGHT")cursor_=1-cursor_;
        if(confirm)answer(cursor_==0);
    }else if(screen_=="result" && confirm){
        if(players_==2&&(selecting_==0||first_==second_)){
            selecting_=1;second_=-1;ruler_=-1;screen_="intro";cursor_=0;question_=1;
        }else return "start";
    }
    return {};
}
}
