extends SceneTree
func png(path: String) -> Image:
	var result = Image.new()
	if result.load_png_from_buffer(FileAccess.get_file_as_bytes(path)) != OK:
		printerr("FAIL: unreadable reference image "+path)
		quit(1)
	return result
func _init():
	if not ClassDB.class_exists("ZhongyuanOriginalData"):
		printerr("FAIL: native original-data extension did not load")
		quit(1)
		return
	var original = png("res://reference/fixtures/world-roads-screen.png").get_region(Rect2i(0,0,256,160))
	original.convert(Image.FORMAT_RGBA8)
	var rebuilt = png("res://assets/original/world-roads.png").get_region(Rect2i(0,0,256,160))
	rebuilt.convert(Image.FORMAT_RGBA8)
	var scenario = JSON.parse_string(FileAccess.get_file_as_string("res://data/scenario.json"))
	var mapping = [-1,4,2,0,1,3,5]
	for c in scenario.cities:
		var icon = png("res://assets/original/town-%d.png" % mapping[int(c.owner)])
		icon.convert(Image.FORMAT_RGBA8)
		rebuilt.blend_rect(icon,Rect2i(0,0,8,8),Vector2i(roundi(float(c.pos[0])*256),roundi(float(c.pos[1])*160)+1))
	if original.get_data()!=rebuilt.get_data():
		printerr("FAIL: original world pixels differ")
		quit(1)
	else:
		print("PASS: native-decoded town sprites and background match all 40,960 original road-screen pixels")
		quit()
