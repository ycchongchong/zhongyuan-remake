#include "zhongyuan/campaign.hpp"
#include <algorithm>
#include <cmath>
#include <queue>
#include <stdexcept>

namespace zhongyuan {
namespace {
Number integer(const Json &j, Number minimum = 0, Number maximum = 100000000) {
    if (!j.is_number()) throw std::runtime_error("Expected an integer");
    const double value = j.get<double>();
    if (!std::isfinite(value) || std::floor(value) != value || value < minimum || value > maximum)
        throw std::runtime_error("Integer out of range");
    return static_cast<Number>(value);
}
City read_city(const Json &j) {
    City c;
    c.reference = j.value("reference", Json());
    c.id = integer(j.at("id"));
    c.owner = integer(j.at("owner"), 0, 6);
    c.gold = integer(j.at("gold")); c.grain = integer(j.at("grain"));
    c.troops = integer(j.at("troops")); c.development = integer(j.at("development"));
    c.ability = integer(j.at("ability"), 0, 100);
    c.name = j.at("name").get<std::string>(); c.general = j.at("general").get<std::string>();
    if (!j.at("neighbors").is_array()) throw std::runtime_error("Invalid neighbors");
    for (const auto &v : j.at("neighbors")) c.neighbors.push_back(static_cast<int>(integer(v)));
    const auto &position = j.at("pos");
    if (!position.is_array() || position.size() != 2) throw std::runtime_error("Invalid position");
    for (int i = 0; i < 2; ++i) {
        if (!position[i].is_number()) throw std::runtime_error("Invalid position");
        c.pos[i] = position[i].get<double>();
        if (!std::isfinite(c.pos[i]) || c.pos[i] < 0 || c.pos[i] > 1) throw std::runtime_error("Invalid position");
    }
    return c;
}
Json write_city(const City &c) {
    Json result = {{"id", c.id}, {"owner", c.owner}, {"gold", c.gold}, {"grain", c.grain},
            {"troops", c.troops}, {"development", c.development}, {"ability", c.ability},
            {"name", c.name}, {"general", c.general}, {"neighbors", c.neighbors}, {"pos", c.pos}};
    if (!c.reference.is_null()) result["reference"] = c.reference;
    return result;
}
bool adjacent(const City &c, int target) {
    return std::find(c.neighbors.begin(), c.neighbors.end(), target) != c.neighbors.end();
}
Number expedition(const City &c) { return static_cast<Number>(double(c.troops) * 0.7); }
double attack_power(const City &c) { return expedition(c) * (1.0 + double(c.ability) / 200.0); }
double defence_power(const City &c) { return double(c.troops) * (1.0 + double(c.ability) / 200.0) * 1.15; }
std::string number(Number n) { return std::to_string(n); }
} // namespace

Campaign::Campaign(const Json &scenario) {
    scenario_id_ = scenario.value("scenario_id", std::string());
    opening_ = scenario.value("opening", std::string("你率刘备军驻守洛阳。先发展军力，再东进许昌。"));
    max_owner_ = scenario.value("max_owner", 2);
    if (max_owner_ < 2 || max_owner_ > 6) throw std::runtime_error("Invalid faction count");
    const auto &entries = scenario.at("cities");
    if (!entries.is_array() || entries.empty() || entries.size() > 1000) throw std::runtime_error("Invalid scenario");
    for (const auto &entry : entries) initial_.push_back(read_city(entry));
    for (std::size_t i = 0; i < initial_.size(); ++i) {
        const auto &c = initial_[i];
        if (c.owner > max_owner_) throw std::runtime_error("Invalid faction");
        if (c.id != static_cast<Number>(i)) throw std::runtime_error("City IDs must match array indices");
        for (int neighbor : c.neighbors) {
            if (neighbor < 0 || neighbor >= static_cast<int>(initial_.size()) || neighbor == static_cast<int>(i)
                || !adjacent(initial_[neighbor], static_cast<int>(i))) throw std::runtime_error("Invalid city connection");
        }
    }
    reset();
}
bool Campaign::start_campaign(int owner, int difficulty) {
    if (owner<1 || owner>max_owner_ || difficulty<0 || difficulty>2) return false;
    player_owner_=owner;difficulty_=difficulty;reset();return true;
}
void Campaign::reset() {
    cities_ = initial_; month_ = 1; orders_ = 3; winner_ = 0;
    history_ = {player_owner_ == 1 ? opening_ : "你统领" + faction(player_owner_) + "，从所属城池开始争夺天下。"};
}
std::string Campaign::faction(int owner) {
    static const std::array<std::string, 7> names = {"中立", "刘备军", "曹操军", "袁绍军", "马腾军", "孙权军", "刘璋军"};
    return owner >= 0 && owner < 7 ? names[owner] : "未知势力";
}
std::string Campaign::record(const std::string &message) {
    history_.insert(history_.begin(), message);
    const std::size_t limit = scenario_id_.empty() ? 40 : 200;
    if (history_.size() > limit) history_.resize(limit);
    return message;
}
std::string Campaign::reason(const std::string &action, int source, int target) const {
    if (winner_ != 0) return "战局已结束，请重新开局。";
    if (source < 0 || source >= static_cast<int>(cities_.size())) return "请先选择城池。";
    const auto &c = cities_[source];
    if (c.owner != player_owner_) return "只能在己方城池下达命令。";
    if (orders_ <= 0) return "本月命令已用完，请结束回合。";
    if (action == "develop") {
        if (c.gold < 60) return "开发需要 60 金。";
    } else if (action == "recruit") {
        if (c.gold < 70 || c.grain < 50) return "征兵需要 70 金和 50 粮。";
    } else if (action == "march") {
        if (target < 0 || target >= static_cast<int>(cities_.size()) || !adjacent(c, target)) return "请选择有道路相连的目标城池。";
        if (c.troops < 200) return "出征或调兵至少需要 200 兵。";
        if (c.grain < 40) return "出征或调兵需要 40 粮。";
    } else return "未知命令。";
    return {};
}
std::string Campaign::act(const std::string &action, int source, int target) {
    const auto problem = reason(action, source, target);
    if (!problem.empty()) return problem;
    --orders_;
    auto &c = cities_[source];
    if (action == "develop") {
        c.gold -= 60; ++c.development;
        return record(c.name + "开发完成，发展等级提升至 " + number(c.development) + "。");
    }
    if (action == "recruit") {
        c.gold -= 70; c.grain -= 50; c.troops += 300;
        return record(c.name + "征得新兵 300 人。");
    }
    return march(source, target);
}
std::string Campaign::march(int source, int target) {
    auto &a = cities_[source]; auto &b = cities_[target];
    const Number sent = expedition(a);
    a.troops -= sent; a.grain -= 40;
    if (a.owner == b.owner) {
        b.troops += sent;
        return record(a.name + "向" + b.name + "调拨 " + number(sent) + " 兵。");
    }
    const double attack = sent * (1.0 + double(a.ability) / 200.0);
    const double defence = defence_power(b);
    const Number defenders = b.troops;
    std::string result;
    if (attack > defence) {
        const Number lost = std::min(sent - 1, static_cast<Number>(defenders * 0.55));
        b.owner = a.owner; b.troops = sent - lost;
        b.general = "驻城校尉"; b.ability = 65;
        result = faction(static_cast<int>(a.owner)) + "攻占" + b.name + "！出兵 " + number(sent)
            + "，损失 " + number(lost) + "，驻军 " + number(b.troops) + "。";
    } else {
        const Number lost = static_cast<Number>(sent * 0.65);
        a.troops += sent - lost;
        b.troops = std::max<Number>(0, defenders - static_cast<Number>(sent * 0.35));
        result = faction(static_cast<int>(a.owner)) + "进攻" + b.name + "失利，损失 " + number(lost)
            + " 兵，余部撤回" + a.name + "。";
    }
    record(result); check_winner();
    return result;
}
void Campaign::check_winner() {
    const auto player = std::count_if(cities_.begin(), cities_.end(), [this](const City &c) { return c.owner == player_owner_; });
    if (player == static_cast<std::ptrdiff_t>(cities_.size())) {
        winner_ = 1; record(scenario_id_.empty() ? "三城归一，刘备军获胜！" : "天下归一，" + faction(player_owner_) + "占领全部 " + number(cities_.size()) + " 城！");
    } else if (player == 0) {
        winner_ = 2; record("己方城池全部失守。可读档，或重新开局。");
    }
}
void Campaign::ai_turn() {
    // Freeze both identity and owner: captured cities never act twice in one month.
    std::vector<std::pair<int, Number>> sources;
    for (const auto &c : cities_) if (c.owner != 0 && c.owner != player_owner_) sources.emplace_back(static_cast<int>(c.id), c.owner);
    for (const auto &[source, owner] : sources) {
        if (winner_ != 0) return;
        auto &c = cities_[source];
        if (c.owner != owner) continue;
        bool attacked = false;
        for (int target : c.neighbors) {
            if (cities_[target].owner != owner && c.troops >= 200 && c.grain >= 40
                && attack_power(c) > defence_power(cities_[target]) * 1.1) {
                march(source, target); attacked = true; break;
            }
        }
        if (attacked) continue;
        bool frontier = false;
        for (int target : c.neighbors) if (cities_[target].owner != owner) frontier = true;
        int transfer = c.neighbors.empty() ? -1 : c.neighbors.front();
        if (!scenario_id_.empty() && !frontier) {
            // Route rear garrisons toward the nearest frontier through friendly cities.
            transfer = -1;
            std::queue<std::pair<int, int>> queue;
            std::vector<bool> visited(cities_.size(), false);
            visited[source] = true;
            for (int neighbor : c.neighbors) { queue.emplace(neighbor, neighbor); visited[neighbor] = true; }
            while (!queue.empty()) {
                const auto [current, first] = queue.front(); queue.pop();
                for (int neighbor : cities_[current].neighbors) {
                    if (cities_[neighbor].owner != owner) { transfer = first; break; }
                    if (!visited[neighbor]) { visited[neighbor] = true; queue.emplace(neighbor, first); }
                }
                if (transfer >= 0) break;
            }
        }
        if (!frontier && transfer >= 0 && c.troops >= 500 && c.grain >= 40) {
            march(source, transfer);
        } else if (c.gold >= 70 && c.grain >= 50) {
            c.gold -= 70; c.grain -= 50; c.troops += 300;
            record(faction(static_cast<int>(owner)) + "在" + c.name + "征兵 300。");
        }
    }
}
void Campaign::end_turn() {
    if (winner_ != 0) return;
    ai_turn();
    if (winner_ != 0) return;
    ++month_;
    for (auto &c : cities_) {
        if (c.owner == 0) continue;
        c.gold += 40 + c.development * 20;
        c.grain += 60 + c.development * 25;
        const Number upkeep = static_cast<Number>(std::ceil(double(c.troops) / 20.0));
        if (c.grain < upkeep) {
            c.troops = std::max<Number>(0, c.troops - (upkeep - c.grain) * 10);
            record(c.name + "粮草不足，部分士兵离队。");
        }
        c.grain = std::max<Number>(0, c.grain - upkeep);
    }
    orders_ = 3;
    record("第 " + number(month_) + " 月开始：税收、收粮与军粮消耗已结算。");
}
Json Campaign::snapshot() const {
    Json entries = Json::array();
    for (const auto &c : cities_) entries.push_back(write_city(c));
    Json result = {{"version", scenario_id_.empty() ? 1 : 2}, {"cities", entries}, {"month", month_}, {"orders", orders_}, {"winner", winner_}, {"history", history_}};
    if (!scenario_id_.empty()) {result["scenario_id"] = scenario_id_; result["player_owner"] = player_owner_; result["difficulty"] = difficulty_;}
    return result;
}
bool Campaign::restore(const Json &save, std::string &error) {
    try {
        if (integer(save.at("version")) != (scenario_id_.empty() ? 1 : 2)) throw std::runtime_error("Unsupported save version");
        if (!scenario_id_.empty() && save.value("scenario_id", std::string()) != scenario_id_)
            throw std::runtime_error("Wrong scenario");
        const int player_owner = static_cast<int>(integer(save.value("player_owner", Json(1)),1,max_owner_));
        const int difficulty = static_cast<int>(integer(save.value("difficulty",Json(0)),0,2));
        const Number month = integer(save.at("month"), 1);
        const int orders = static_cast<int>(integer(save.at("orders"), 0, 3));
        const int winner = static_cast<int>(integer(save.at("winner"), 0, 2));
        const auto &entries = save.at("cities");
        if (!entries.is_array() || entries.size() != initial_.size()) throw std::runtime_error("Wrong city count");
        std::vector<City> cities;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            auto c = read_city(entries[i]);
            if (c.owner > max_owner_ || c.name != initial_[i].name || c.reference != initial_[i].reference
                || c.id != static_cast<Number>(i) || c.neighbors != initial_[i].neighbors || c.pos != initial_[i].pos)
                throw std::runtime_error("City topology changed");
            cities.push_back(std::move(c));
        }
        if (!save.at("history").is_array() || save.at("history").size() > (scenario_id_.empty() ? 40u : 200u)) throw std::runtime_error("Invalid history");
        auto history = save.at("history").get<std::vector<std::string>>();
        // Commit only after the entire incoming state is validated.
        cities_ = std::move(cities); history_ = std::move(history);
        month_ = month; orders_ = orders; winner_ = winner; player_owner_ = player_owner; difficulty_ = difficulty;
        error.clear(); return true;
    } catch (const std::exception &e) { error = e.what(); return false; }
}
bool Campaign::load_json(const std::string &text, std::string &error) {
    try { return restore(Json::parse(text), error); }
    catch (const std::exception &e) { error = e.what(); return false; }
}
std::string Campaign::save_json() const { return snapshot().dump(2); }
} // namespace zhongyuan
