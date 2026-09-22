#include "bridge.hpp"
#include "zhongyuan/save_file.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {
namespace {
std::string utf8(const String &s) { const auto chars = s.utf8(); return {chars.get_data(), static_cast<std::size_t>(chars.length())}; }
String text(const std::string &s) { return String::utf8(s.c_str()); }
Variant to_variant(const zhongyuan::Json &value) {
    if (value.is_number_integer()) return static_cast<int64_t>(value.get<int64_t>());
    if (value.is_number_float()) return value.get<double>();
    if (value.is_string()) return text(value.get<std::string>());
    if (value.is_boolean()) return value.get<bool>();
    if (value.is_array()) {
        Array result;
        for (const auto &entry : value) result.append(to_variant(entry));
        return result;
    }
    if (value.is_object()) {
        Dictionary result;
        for (auto it = value.begin(); it != value.end(); ++it) result[text(it.key())] = to_variant(it.value());
        return result;
    }
    return Variant();
}
} // namespace

void ZhongyuanGame::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start_campaign","owner","difficulty"), &ZhongyuanGame::start_campaign, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("get_player_owner"), &ZhongyuanGame::get_player_owner);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "player_owner"), "", "get_player_owner");
    ClassDB::bind_method(D_METHOD("reset"), &ZhongyuanGame::reset);
    ClassDB::bind_method(D_METHOD("faction", "owner"), &ZhongyuanGame::faction);
    ClassDB::bind_method(D_METHOD("reason", "action", "source", "target"), &ZhongyuanGame::reason, DEFVAL(-1));
    ClassDB::bind_method(D_METHOD("act", "action", "source", "target"), &ZhongyuanGame::act, DEFVAL(-1));
    ClassDB::bind_method(D_METHOD("end_turn"), &ZhongyuanGame::end_turn);
    ClassDB::bind_method(D_METHOD("snapshot"), &ZhongyuanGame::snapshot);
    ClassDB::bind_method(D_METHOD("restore_snapshot", "data"), &ZhongyuanGame::restore_snapshot);
    ClassDB::bind_method(D_METHOD("save_game", "path"), &ZhongyuanGame::save_game, DEFVAL("user://campaign-original-map.json"));
    ClassDB::bind_method(D_METHOD("load_game", "path"), &ZhongyuanGame::load_game, DEFVAL("user://campaign-original-map.json"));
    ClassDB::bind_method(D_METHOD("get_cities"), &ZhongyuanGame::get_cities);
    ClassDB::bind_method(D_METHOD("get_history"), &ZhongyuanGame::get_history);
    ClassDB::bind_method(D_METHOD("get_month"), &ZhongyuanGame::get_month);
    ClassDB::bind_method(D_METHOD("get_orders"), &ZhongyuanGame::get_orders);
    ClassDB::bind_method(D_METHOD("get_winner"), &ZhongyuanGame::get_winner);
    ClassDB::bind_method(D_METHOD("get_last_error"), &ZhongyuanGame::get_last_error);
    ClassDB::bind_method(D_METHOD("backend_name"), &ZhongyuanGame::backend_name);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "cities"), "", "get_cities");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "history"), "", "get_history");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "month"), "", "get_month");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "orders"), "", "get_orders");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "winner"), "", "get_winner");
}
ZhongyuanGame::ZhongyuanGame() {
    try {
        const auto source = FileAccess::get_file_as_string("res://data/scenario.json");
        core_ = std::make_unique<zhongyuan::Campaign>(zhongyuan::Json::parse(utf8(source)));
    } catch (const std::exception &e) {
        last_error_ = text(e.what());
        UtilityFunctions::push_error("C++ campaign initialization failed: ", last_error_);
    }
}
bool ZhongyuanGame::start_campaign(int owner, int difficulty) {return core_ && core_->start_campaign(owner,difficulty);}
int ZhongyuanGame::get_player_owner() const {return core_ ? core_->player_owner() : 1;}
void ZhongyuanGame::reset() { ERR_FAIL_NULL(core_.get()); core_->reset(); }
String ZhongyuanGame::faction(int owner) const { return text(zhongyuan::Campaign::faction(owner)); }
String ZhongyuanGame::reason(const String &action, int source, int target) const {
    ERR_FAIL_NULL_V(core_.get(), "核心初始化失败");
    return text(core_->reason(utf8(action), source, target));
}
String ZhongyuanGame::act(const String &action, int source, int target) {
    ERR_FAIL_NULL_V(core_.get(), "核心初始化失败");
    return text(core_->act(utf8(action), source, target));
}
void ZhongyuanGame::end_turn() { ERR_FAIL_NULL(core_.get()); core_->end_turn(); }
Dictionary ZhongyuanGame::snapshot() const {
    ERR_FAIL_NULL_V(core_.get(), Dictionary());
    return to_variant(core_->snapshot());
}
bool ZhongyuanGame::restore_snapshot(const Dictionary &data) {
    ERR_FAIL_NULL_V(core_.get(), false);
    std::string error;
    const bool ok = core_->load_json(utf8(JSON::stringify(data, "", true, true)), error);
    last_error_ = text(error);
    return ok;
}
Array ZhongyuanGame::get_cities() const { return snapshot().get("cities", Array()); }
Array ZhongyuanGame::get_history() const { return snapshot().get("history", Array()); }
int64_t ZhongyuanGame::get_month() const { return core_ ? core_->month() : 1; }
int ZhongyuanGame::get_orders() const { return core_ ? core_->orders() : 0; }
int ZhongyuanGame::get_winner() const { return core_ ? core_->winner() : 0; }
Error ZhongyuanGame::save_game(const String &path) {
    ERR_FAIL_NULL_V(core_.get(), ERR_UNCONFIGURED);
    const String absolute = ProjectSettings::get_singleton()->globalize_path(path);
    const String temporary = absolute + String(".tmp");
    Ref<FileAccess> file = FileAccess::open(temporary, FileAccess::WRITE);
    if (file.is_null()) return FileAccess::get_open_error();
    file->store_string(text(core_->save_json()));
    file->flush();
    const Error result = file->get_error();
    file->close();
    if (result != OK) { DirAccess::remove_absolute(temporary); return result; }
    const Error rename_result = zhongyuan::replace_save_file(std::filesystem::u8path(utf8(temporary)), std::filesystem::u8path(utf8(absolute))) ? FAILED : OK;
    if (rename_result != OK) DirAccess::remove_absolute(temporary);
    return rename_result;
}
Error ZhongyuanGame::load_game(const String &path) {
    ERR_FAIL_NULL_V(core_.get(), ERR_UNCONFIGURED);
    if (!FileAccess::file_exists(path)) return ERR_FILE_NOT_FOUND;
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (file.is_null()) return FileAccess::get_open_error();
    if (file->get_length() > 16 * 1024 * 1024) return ERR_FILE_CORRUPT;
    const auto contents = file->get_as_text();
    std::string error;
    const bool ok = core_->load_json(utf8(contents), error);
    last_error_ = text(error);
    return ok ? OK : ERR_FILE_CORRUPT;
}
} // namespace godot
