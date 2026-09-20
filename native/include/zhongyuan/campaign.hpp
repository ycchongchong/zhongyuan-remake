#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace zhongyuan {
using Number = std::int64_t;
using Json = nlohmann::json;

struct City {
    Number id{}, owner{}, gold{}, grain{}, troops{}, development{}, ability{};
    std::string name, general;
    Json reference; // Immutable ROM initial values; separate from prototype economy.
    std::vector<int> neighbors;
    std::array<double, 2> pos{};
};

// No Godot dependencies: this owns all mutable campaign state and rules.
class Campaign {
public:
    explicit Campaign(const Json &scenario);
    void reset();
    bool start_campaign(int owner, int difficulty = 0);
    int player_owner() const { return player_owner_; }
    std::string reason(const std::string &action, int source, int target = -1) const;
    std::string act(const std::string &action, int source, int target = -1);
    void end_turn();
    Json snapshot() const;
    bool restore(const Json &save, std::string &error);
    bool load_json(const std::string &text, std::string &error);
    std::string save_json() const;
    static std::string faction(int owner);
    const std::vector<City> &cities() const { return cities_; }
    const std::vector<std::string> &history() const { return history_; }
    Number month() const { return month_; }
    int orders() const { return orders_; }
    int winner() const { return winner_; }

private:
    std::vector<City> initial_, cities_;
    std::vector<std::string> history_;
    std::string scenario_id_, opening_;
    int max_owner_ = 2, player_owner_ = 1, difficulty_ = 0;
    Number month_ = 1;
    int orders_ = 3, winner_ = 0;
    std::string record(const std::string &message);
    std::string march(int source, int target);
    void ai_turn();
    void check_winner();
};
} // namespace zhongyuan
