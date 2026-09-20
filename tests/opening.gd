extends SceneTree
var failures = 0
func check(ok: bool, message: String):
	if ok: print("PASS: "+message)
	else:
		failures += 1
		printerr("FAIL: "+message)
func _init(): call_deferred("run")
func run():
	var opening = load("res://opening.tscn").instantiate()
	root.add_child(opening)
	await process_frame
	check(opening.state.screen == "title", "original title opens")
	opening.press("START")
	check(opening.state.screen == "menu", "start enters original menu")
	opening.press("A")
	opening.press("A")
	opening.press("A")
	check(opening.state.screen == "quiz" and int(opening.state.question) == 1, "difficulty and intro enter question one")
	for answer in [true,true,true,true,true]:
		if not answer: opening.press("RIGHT")
		opening.press("A")
	check(opening.state.screen == "result" and int(opening.state.ruler)==4, "original all-yes path matches Liu Bei")
	opening.press("A")
	await process_frame
	var game_scene = current_scene
	check(game_scene != null and int(game_scene.state.player) == 4 and game_scene.state.cities.size()==30 and game_scene.state.officers.size()==241, "original opening starts native thirty-city/241-officer campaign")
	game_scene.queue_free()
	await process_frame
	var duo=load("res://opening.tscn").instantiate()
	root.add_child(duo)
	duo.press("START");duo.press("DOWN");duo.press("A");duo.press("A");duo.press("A")
	for i in range(5):duo.press("A")
	duo.press("A")
	check(duo.state.screen=="intro" and int(duo.state.selecting_player)==1,"two-player mode reaches second diagnosis")
	duo.press("A")
	for i in range(5):duo.press("A")
	check(duo.state.duplicate,"duplicate ruler displays original rejection")
	duo.press("A");duo.press("A")
	for i in range(4):duo.press("A")
	duo.press("RIGHT");duo.press("A");duo.press("A")
	await process_frame
	check(int(current_scene.state.player)==4 and int(current_scene.state.second_player)==5,"two distinct diagnosed rulers reach native session")
	current_scene.queue_free()
	await process_frame
	var core=ZhongyuanGame.new()
	for owner in range(1,7):
		check(core.start_campaign(owner,2) and core.player_owner==owner, "native faction selection %d" % owner)
		var own=-1
		var enemy=-1
		for c in core.cities:
			if int(c.owner)==owner: own=int(c.id)
			else: enemy=int(c.id)
		check(own>=0 and core.reason("develop",own).is_empty() and not core.reason("develop",enemy).is_empty(), "commands follow selected faction %d" % owner)
		var snapshot=core.snapshot()
		check(int(snapshot.player_owner)==owner and int(snapshot.difficulty)==2 and core.restore_snapshot(snapshot), "selected faction and difficulty survive snapshot %d" % owner)
	print("OPENING RESULT: %d failures" % failures)
	# Let the audio mixer retire voices after the last scene has exited.
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
