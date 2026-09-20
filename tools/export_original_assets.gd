extends SceneTree
## Pixel assets are decoded by C++ from the user-selected ROM, without emulation.
func _init() -> void:
	var manifest = JSON.parse_string(FileAccess.get_file_as_string("res://reference/manifest.json"))
	var native = ZhongyuanOriginalData.new()
	var result = native.load_rom(manifest.local_path)
	if result.has("error"):
		printerr(result.error)
		quit(1)
		return
	var names = Image.create(48, 16*271, false, Image.FORMAT_RGBA8)
	names.fill(Color(0,0,0,0))
	for i in range(30): names.blit_rect(native.name_image(false,i),Rect2i(0,0,48,16),Vector2i(0,i*16))
	for i in range(241): names.blit_rect(native.name_image(true,i),Rect2i(0,0,48,16),Vector2i(0,(i+30)*16))
	if names.save_png("res://assets/original/names.png") != OK:
		quit(1)
		return
	for i in range(6):
		if native.town_image(i).save_png("res://assets/original/town-%d.png" % i) != OK:
			quit(1)
			return
	print("Exported 271 original names and 6 faction town sprites using C++ decoding")
	quit()
