extends SceneTree
var failures = 0
func check(ok: bool, message: String):
	print(("PASS: " if ok else "FAIL: ") + message)
	if not ok: failures += 1
func _init(): call_deferred("run")
func run():
	var scene = load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(3, 0).is_empty(), "initialize natural-progress UI")
	root.add_child(scene); current_scene = scene; scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/natural-no-war-target.json").is_empty(), "load unmodified 29-city campaign stop")
	scene.refresh()
	check(scene.state.phase == "ai_turn", "last enemy still owns its strategic turn")
	scene.advance_ai()
	check(not scene.ai_paused and scene.state.phase == "player_commands", "no-target computer plan returns control through normal UI")
	var path = "user://natural-progress-test.json"
	var before: Dictionary = scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot() == before, "continued strategic state saves and resumes exactly")
	check(scene.native.load_session("res://tests/natural-unification.json").is_empty(), "load actual natural-campaign victory")
	scene.ending_seen = false; scene.refresh()
	await process_frame
	before = scene.native.session_snapshot()
	check(before.phase == "ending" and before.ending.kind == "unification" and int(before.ending.winner) == 3, "natural campaign ends with Sun Quan victorious")
	var owned = 0
	for city in range(30):
		if (int(before.sram[city * 36]) & 7) == 3: owned += 1
	check(owned == 30 and before.battle == null, "all 30 cities belong to the winner after settlement")
	check(scene.ending_report.visible and scene.ending_picture.visible and scene.ending_picture.texture.get_size() == Vector2(256, 240), "natural victory opens the full original score picture")
	check(scene.ending_text.text.contains("孙权") and scene.ending_text.text.contains("30") and scene.ending_text.text.contains(str(int(before.unification.score))), "report identifies natural winner and score")
	var sound = root.get_node("OriginalSound")
	check(sound.current_cue == "unification_" + str(int(before.unification.variant)) and sound.music.playing, "natural victory plays its matching original ending score")
	check(scene.end_button.disabled and scene.expedition_button.disabled and scene.orders_button.disabled, "terminal state locks further campaigns")
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot() == before, "natural victory round trip preserves score and ownership")
	scene._process(1.0)
	check(scene.native.session_snapshot() == before, "ending display cannot advance the clock")
	if "--capture-score" in OS.get_cmdline_user_args():
		check(scene.native.unification_image().save_png("user://natural-unification-score.png") == OK, "save native score image without a display server")
	if "--capture-natural" in OS.get_cmdline_user_args():
		await process_frame; await RenderingServer.frame_post_draw
		root.get_texture().get_image().save_png("user://natural-unification.png")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	await sound.shutdown()
	print("ORIGINAL NATURAL PROGRESS: %d failures" % failures)
	quit(1 if failures else 0)
