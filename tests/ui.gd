extends SceneTree
var failures: int = 0
func _init() -> void: call_deferred("run")
func check(condition: bool, description: String) -> void:
	if condition: print("PASS: " + description)
	else:
		printerr("FAIL: " + description)
		failures += 1
func run() -> void:
	var scene = load("res://main.tscn").instantiate()
	root.add_child(scene)
	await process_frame
	check(scene.city_buttons.size() == 30 and scene.city_picker.item_count == 30, "thirty map buttons and city selector entries")
	check(scene.selected == 13 and scene.holdings.text.contains("3 / 30"), "opens at Xinye with three player cities")
	var bounds_ok = true
	for i in range(30):
		var rect = scene.city_buttons[i].get_rect()
		bounds_ok = bounds_ok and Rect2(36, 126, 786, 491).encloses(rect)
		for j in range(i): bounds_ok = bounds_ok and not rect.intersects(scene.city_buttons[j].get_rect())
		scene.city_buttons[i].pressed.emit()
		check(scene.selected == i and scene.info.text.contains(scene.game.cities[i].name), "city %02d selects and displays" % i)
	check(bounds_ok, "all city buttons fit without overlap")
	scene.city_buttons[13].pressed.emit()
	var state = scene.game.snapshot()
	state.cities[12].troops = 100
	check(scene.game.restore_snapshot(state), "attack fixture loaded")
	scene.recruit_button.pressed.emit()
	scene.recruit_button.pressed.emit()
	scene.march_button.pressed.emit()
	check(scene.marching, "march button enters target selection")
	scene.city_buttons[29].pressed.emit()
	check(scene.marching and scene.game.orders == 1, "distant target rejected without order cost")
	scene.city_buttons[12].pressed.emit()
	check(int(scene.game.cities[12].owner) == 1 and not scene.marching, "adjacent target resolves attack")
	check(scene.recruit_button.disabled and scene.march_button.disabled, "UI disables orders at zero")
	scene.end_button.pressed.emit()
	check(scene.game.month == 2 and scene.month_label.text.contains("02"), "end-turn refreshes month")
	scene.city_picker.item_selected.emit(29)
	check(scene.selected == 29 and scene.recruit_button.disabled, "selector reaches distant enemy city")
	scene.show_history()
	check(scene.history_dialog.visible and scene.history_full.text.contains("第 2 月"), "full battle log opens after AI turn")
	scene.history_dialog.hide()
	scene.reset_game()
	check(scene.game.month == 1 and scene.selected == 13 and scene.game.cities.size() == 30, "new game restores 30-city opening")
	scene.queue_free()
	await process_frame
	print("UI RESULT: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures > 0 else 0)
