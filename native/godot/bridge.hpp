#pragma once
#include "zhongyuan/campaign.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <memory>

namespace godot {
class ZhongyuanGame : public RefCounted {
    GDCLASS(ZhongyuanGame, RefCounted)
    std::unique_ptr<zhongyuan::Campaign> core_;
    String last_error_;
protected:
    static void _bind_methods();
public:
    ZhongyuanGame();
    void reset();
    bool start_campaign(int owner, int difficulty = 0);
    int get_player_owner() const;
    String faction(int owner) const;
    String reason(const String &action, int source, int target = -1) const;
    String act(const String &action, int source, int target = -1);
    void end_turn();
    Dictionary snapshot() const;
    bool restore_snapshot(const Dictionary &data);
    Error save_game(const String &path = "user://campaign-original-map.json");
    Error load_game(const String &path = "user://campaign-original-map.json");
    Array get_cities() const;
    Array get_history() const;
    int64_t get_month() const;
    int get_orders() const;
    int get_winner() const;
    String get_last_error() const { return last_error_; }
    String backend_name() const { return "C++17 / GDExtension"; }
};
} // namespace godot
