extends SceneTree
const Game = preload("res://scripts/game.gd")
var failures: int = 0

func patch_city(game, index: int, values: Dictionary) -> void:
	var state: Dictionary = game.snapshot()
	for key in values: state.cities[index][key] = values[key]
	if not game.restore_snapshot(state):
		failures += 1
		printerr("FAIL: fixture restore " + game.get_last_error())

func check(condition: bool, description: String) -> void:
	if not condition:
		failures += 1
		printerr("FAIL: " + description)
	else:
		print("PASS: " + description)

func _init() -> void:
	var game = Game.new()
	check(game.backend_name() == "C++17 / GDExtension", "native C++ backend loaded")
	var before: Dictionary = game.snapshot()
	game.act("recruit", 2)
	check(game.snapshot() == before, "enemy city commands do not mutate state")
	game.act("march", 13, 29)
	check(game.snapshot() == before, "non-adjacent march rejected without spending")
	patch_city(game, 12, {"troops":100})
	game.act("recruit", 13)
	game.act("recruit", 13)
	game.act("march", 13, 12)
	check(int(game.cities[12].owner) == 1 and game.orders == 0, "recruit, recruit, capture adjacent enemy city")
	before = game.snapshot()
	game.act("develop", 13)
	check(game.snapshot() == before, "order limit enforced")
	check(game.save_game("user://test-save.json") == OK, "save successful")
	var restored = Game.new()
	check(restored.load_game("user://test-save.json") == OK and restored.snapshot() == game.snapshot(), "save round trip preserves exact state")
	var broken = FileAccess.open("user://test-broken.json", FileAccess.WRITE)
	broken.store_string('{"version":1,"cities":null}')
	broken.close()
	before = restored.snapshot()
	check(restored.load_game("user://test-broken.json") == ERR_FILE_CORRUPT and restored.snapshot() == before, "corrupt save preserves active game")
	game.end_turn()
	check(game.month == 2 and game.orders == 3, "AI and month settlement advance")
	# Both terminal conditions, including a distant final city.
	var state = game.snapshot()
	state.orders = 3
	for city in state.cities: city.owner = 1
	state.cities[29].owner = 6
	state.cities[29].troops = 100
	state.cities[28].troops = 10000
	state.cities[28].grain = 1000
	check(game.restore_snapshot(state), "decisive fixture loaded")
	game.act("march", 28, 29)
	check(game.winner == 1, "capturing thirtieth city wins")
	before = game.snapshot()
	game.end_turn()
	check(game.snapshot() == before, "terminal state does not advance")
	game.reset()
	state = game.snapshot()
	for city in state.cities:
		city.owner = 2
		city.troops = 100
	state.cities[13].owner = 1
	state.cities[12].troops = 10000
	state.cities[12].grain = 1000
	check(game.restore_snapshot(state), "defeat fixture loaded")
	game.end_turn()
	check(game.winner == 2, "AI capture of final player city loses")
	game.reset()
	patch_city(game, 13, {"troops":200})
	game.act("march", 13, 12)
	check(int(game.cities[12].owner) == 2 and int(game.cities[13].troops) > 0, "failed attack retreats survivors")
	game.reset()
	var total: int = int(game.cities[13].troops) + int(game.cities[21].troops)
	game.act("march", 13, 21)
	check(int(game.cities[13].troops) + int(game.cities[21].troops) == total, "friendly transfer conserves troops")
	game.reset()
	for turn in range(100):
		if game.winner != 0: break
		for c in game.cities:
			if int(c.owner) == 1: game.act("recruit", int(c.id))
		game.end_turn()
	var valid: bool = true
	for c in game.cities:
		valid = valid and c.gold >= 0 and c.grain >= 0 and c.troops >= 0
	check(valid, "long campaign keeps resources nonnegative")
	print("RESULT: %d failures" % failures)
	quit(1 if failures > 0 else 0)
