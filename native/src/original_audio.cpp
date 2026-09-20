#include "zhongyuan/original_session.hpp"

namespace zhongyuan {
std::string OriginalSession::music_cue() const {
    // Only scenes whose music entry has been traced in the selected ROM.
    if (phase_ == "ending") {
        const auto summary=state_.unification_summary();
        if(summary.is_object()&&summary.contains("variant"))
            return "unification_"+std::to_string(summary["variant"].get<int>());
        return {};
    }
    if (!battle_.is_object()) return "campaign";
    if (!battle_.contains("tactics")) return "tactical";
    const auto &t = battle_.at("tactics");
    if (t.contains("human_failure"))
        return t.at("human_failure").value("can_continue", false) ? "battle_result" : "defeat";
    if (t.value("turn_boundary", "") == "battle_result") return "battle_result";
    if (!t.contains("attack")) return "tactical";
    const auto &a = t.at("attack");
    const auto stage = a.value("stage", "");
    if (stage.compare(0, 5, "duel_") == 0) return "duel";
    if (stage == "clash_ready" || stage == "clash_orders")
        return a.value("duel_return", false) ? "clash" : "clash_orders";
    if (stage.compare(0, 10, "surrender_") == 0 && a.contains("surrender") &&
        a.at("surrender").value("resume_stage", "clash_orders") == "clash_orders") return "clash_orders";
    if (stage == "clash_running" || stage == "clash_boundary" ||
        stage == "clash_retreat" || stage == "clash_defeat" ||
        stage == "surrender_confirm" || stage == "surrender_notice" || stage == "surrender_accepted") return "clash";
    return "tactical";
}
} // namespace zhongyuan
