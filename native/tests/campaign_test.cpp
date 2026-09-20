#include "zhongyuan/campaign.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace zhongyuan;
Json read(const std::string &path) { std::ifstream input(path); return Json::parse(input); }
void require(bool ok, const std::string &message) { if (!ok) throw std::runtime_error(message); }

int main() {
    try {
        const std::string root = ZHONGYUAN_PROJECT_DIR;
        const auto scenario = read(root + "/tests/legacy_scenario.json");
        const auto traces = read(root + "/tests/legacy_traces.json");
        int steps = 0;
        std::string error;
        for (const auto &trace : traces) {
            Campaign game(scenario);
            require(game.restore(trace.at("start"), error), error);
            for (const auto &step : trace.at("steps")) {
                const auto action = step.at("action").get<std::string>();
                if (action == "end_turn") game.end_turn();
                else require(game.act(action, step.at("source"), step.at("target")) == step.at("message"), "Message mismatch at " + std::to_string(steps));
                require(game.snapshot() == step.at("expected"), "State mismatch at " + std::to_string(steps));
                ++steps;
            }
        }
        std::cout << "PASS: " << steps << " legacy reference steps (state and messages)\n";
        Campaign game(scenario);
        game.act("recruit", 0); game.act("recruit", 0); game.act("march", 0, 1);
        const auto before = game.snapshot();
        Campaign restored(scenario);
        require(restored.load_json(game.save_json(), error) && restored.snapshot() == before, "Save round trip");
        require(!game.load_json("{broken", error) && game.snapshot() == before, "Bad JSON changed state");
        for (const auto &value : {Json(-1), Json(0.5), Json("1"), Json(nullptr), Json(1e30)}) {
            auto invalid = before; invalid["cities"][0]["troops"] = value;
            require(!game.restore(invalid, error) && game.snapshot() == before, "Invalid numeric state accepted");
        }
        auto invalid = before; invalid["cities"][0]["neighbors"] = Json::array({2});
        require(!game.restore(invalid, error) && game.snapshot() == before, "Invalid topology accepted");
        invalid = before; invalid["history"] = Json::array({42});
        require(!game.restore(invalid, error) && game.snapshot() == before, "Invalid history accepted");
        auto view = game.snapshot(); view["cities"][0]["gold"] = 99999;
        require(game.snapshot() == before, "Snapshot exposed mutable state");
        std::cout << "PASS: save round trip, corrupt saves, range validation, topology, snapshot isolation\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
