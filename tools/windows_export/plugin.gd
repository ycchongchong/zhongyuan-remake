@tool
extends EditorPlugin
var exporter:EditorExportPlugin
class RawAssets extends EditorExportPlugin:
	func _get_name() -> String:
		return "ZhongyuanRawAssets"
	func _export_begin(_features:PackedStringArray,_debug:bool,_path:String,_flags:int) -> void:
		add_directory("res://assets/original")
	func add_directory(path:String) -> void:
		for file in DirAccess.get_files_at(path):
			if file.get_extension() in ["png","wav"]:
				var source=path.path_join(file)
				add_file(source,FileAccess.get_file_as_bytes(source),false)
		for directory in DirAccess.get_directories_at(path):
			add_directory(path.path_join(directory))
func _enter_tree() -> void:
	exporter=RawAssets.new()
	add_export_plugin(exporter)
func _exit_tree() -> void:
	remove_export_plugin(exporter)
