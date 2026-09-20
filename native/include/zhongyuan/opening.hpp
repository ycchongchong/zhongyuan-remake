#pragma once
#include "campaign.hpp"
namespace zhongyuan {
// Native menu/diagnosis state. Branches come from all 26 original input samples.
class Opening {
public:
    Json snapshot() const;
    std::string press(const std::string &key);
    void reset();
    void tick(int frames);
    int answer(bool yes);
private:
    std::string press_impl(const std::string &key);
    int presentation_frame_=0;
    std::string presentation_key_;
    std::string screen_="title";
    int cursor_=0, question_=1, ruler_=-1, difficulty_=0;
    int players_=1,selecting_=0,first_=-1,second_=-1;
};
}
