extends SceneTree
const Game = preload("res://scripts/game.gd")
var failures: int = 0

func check(condition: bool, description: String) -> void:
	if condition: print("PASS: " + description)
	else:
		failures += 1
		printerr("FAIL: " + description)

func _init() -> void:
	var game = Game.new()
	var state: Dictionary = game.snapshot()
	var exposed: Array = game.cities
	exposed[0].gold = 99999
	check(game.snapshot() == state, "Godot array mutation cannot change C++ state")
	check(game.cities.size() == 30 and game.cities[29].name == "永昌", "all thirty cities cross GDExtension")
	var owners = {}
	for c in game.cities: owners[int(c.owner)] = true
	check(owners.size() == 6 and game.faction(6) == "刘璋军", "all six faction IDs cross GDExtension")
	game.act("recruit", 13)
	game.act("march", 13, 21)
	var changed = game.snapshot()
	game.reset()
	check(game.restore_snapshot(changed) and game.snapshot() == changed, "30-city state restores through GDExtension")
	var legacy = JSON.parse_string(FileAccess.get_file_as_string("res://tests/legacy_traces.json"))[0].start
	check(not game.restore_snapshot(legacy) and game.snapshot() == changed, "old three-city save rejected without mutation")
	game.reset()
	var path = "user://native-integration.json"
	check(game.save_game(path) == OK, "native save created")
	game.act("recruit", 13)
	state = game.snapshot()
	check(game.save_game(path) == OK, "existing native save replaced")
	game.reset()
	check(game.load_game(path) == OK and game.snapshot() == state, "native save reload preserves exact state")
	var file = FileAccess.open(path, FileAccess.WRITE)
	file.store_string('{"version":999}')
	file.close()
	check(game.load_game(path) == ERR_FILE_CORRUPT and game.snapshot() == state, "bad native save does not mutate state")
	check(game.load_game("user://missing-native-file.json") == ERR_FILE_NOT_FOUND, "missing file reported")
	check(game.save_game("user://missing-directory/no-save.json") != OK, "unwritable destination reported")
	print("NATIVE INTEGRATION: %d failures" % failures)
	quit(1 if failures > 0 else 0)
