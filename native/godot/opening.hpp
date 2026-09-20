#pragma once
#include "zhongyuan/opening.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
namespace godot {
class ZhongyuanOpening : public RefCounted {
    GDCLASS(ZhongyuanOpening,RefCounted)
    zhongyuan::Opening core_;
protected: static void _bind_methods();
public:
    Dictionary snapshot() const;
    String press(const String &key);
    void tick(int frames){core_.tick(frames);}
    void reset(){core_.reset();}
};
}
