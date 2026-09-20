extends SceneTree

func _init() -> void:
	var manifest = JSON.parse_string(FileAccess.get_file_as_string("res://reference/manifest.json"))
	var data = ZhongyuanOriginalData.new()
	var inventory: Dictionary = data.load_rom(manifest.local_path)
	if inventory.has("error"):
		printerr(inventory.error)
		quit(1)
		return
	assert(inventory.cities.size() == 30)
	assert(inventory.officers.size() == 241)
	assert(inventory.cities[13].name == "新野")
	assert(inventory.cities[13].gold == 200)
	assert(inventory.officers[145].name == "赵云")
	for i in range(30):
		var glyph: Image = data.name_image(false,i)
		assert(glyph != null and glyph.get_width() == 48 and glyph.get_height() == 16)
	for i in range(241):
		assert(data.name_image(true,i) != null)
	assert(data.name_image(true,241) == null)
	var error: Dictionary = data.load_rom("res://data/scenario.json")
	assert(error.has("error"))
	assert(data.name_image(true,145) != null)
	print("PASS: native original-data binding, 30 cities, 241 names, invalid input and state preservation")
	quit()
