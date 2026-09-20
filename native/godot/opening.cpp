#include "opening.hpp"
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/core/class_db.hpp>
namespace godot {
void ZhongyuanOpening::_bind_methods(){
    ClassDB::bind_method(D_METHOD("snapshot"),&ZhongyuanOpening::snapshot);
    ClassDB::bind_method(D_METHOD("press","key"),&ZhongyuanOpening::press);
    ClassDB::bind_method(D_METHOD("tick","frames"),&ZhongyuanOpening::tick);
    ClassDB::bind_method(D_METHOD("reset"),&ZhongyuanOpening::reset);
}
Dictionary ZhongyuanOpening::snapshot() const{return JSON::parse_string(String::utf8(core_.snapshot().dump().c_str()));}
String ZhongyuanOpening::press(const String &key){return String::utf8(core_.press(key.utf8().get_data()).c_str());}
}
