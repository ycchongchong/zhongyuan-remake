#include "bridge.hpp"
#include "original_data.hpp"
#include "opening.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;
void initialize_zhongyuan(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        ClassDB::register_class<ZhongyuanGame>();
        ClassDB::register_class<ZhongyuanOriginalData>();
        ClassDB::register_class<ZhongyuanOpening>();
    }
}
void uninitialize_zhongyuan(ModuleInitializationLevel) {}

extern "C" GDExtensionBool GDE_EXPORT zhongyuan_library_init(
    GDExtensionInterfaceGetProcAddress get_proc_address,
    GDExtensionClassLibraryPtr library, GDExtensionInitialization *initialization) {
    GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize_zhongyuan);
    init.register_terminator(uninitialize_zhongyuan);
    init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
