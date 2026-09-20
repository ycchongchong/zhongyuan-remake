#include "zhongyuan/original_state.hpp"
#include <algorithm>

namespace zhongyuan {
Json OriginalState::unification_summary() const {
    const auto result=ending();
    if(result.is_null()||result["kind"]!="unification")return nullptr;
    // Original A17B-A29F scans city slots in ascending order. Loyalty includes
    // rulers (including FF), while the speaker is the last non-ruler tied for
    // greatest loyalty. Keep truncation at each original division boundary.
    unsigned residents=0,loyalty=0,control=0;
    int adviser=-1,best=0;
    for(int city=0;city<30;++city){
        control+=bytes_[city*36+14];
        for(int slot=0;slot<12;++slot){
            const int officer=bytes_[city*36+16+slot];
            if(officer==255)continue;
            if(officer>=241)return {{"error","统一结局包含无效驻城武将"}};
            const int value=bytes_[0x438+officer*8+5];
            ++residents;loyalty+=value;
            if(officer>=6&&value>=best){best=value;adviser=officer;}
        }
    }
    // A valid campaign has at most 241 distinct residents. Never invent the
    // original's stale-speaker/division-by-zero result for a malformed ledger.
    if(residents==0||residents>241||adviser<0)return {{"error","统一结局驻城名册不完整"}};
    const unsigned average_loyalty=loyalty/residents;
    const unsigned recruitment=residents*100/120;
    const unsigned average_control=control/30;
    const unsigned score=std::min(100u,(average_loyalty+recruitment+average_control)/3);
    return {{"score",score},{"variant",score<60?0:score<80?2:1},
            {"adviser",adviser},{"residents",residents},{"average_loyalty",average_loyalty},
            {"recruitment_score",recruitment},{"average_control",average_control}};
}
} // namespace zhongyuan
