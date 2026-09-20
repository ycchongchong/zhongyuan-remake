extends SceneTree
var failures=0
var scene:Control
var path="user://native-history-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func replay():
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"segmented battle saves and reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize long-battle interface")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/long-clash-archive-ready.json").is_empty(),"load actual 767-event combat history")
	scene.refresh()
	scene.show_battle_details()
	scene.clash_step.emit_signal("pressed")
	check(scene.state.battle.tactics.moves.size()==768 and not scene.state.battle.tactics.has("history"),"original event prefix remains unchanged until next action")
	var before:Dictionary=scene.native.session_snapshot()
	check(not scene.native.begin_clash().is_empty() and scene.native.session_snapshot()==before,"invalid action cannot commit an automatic archive")
	scene.clash_step.emit_signal("pressed")
	check(scene.state.battle.tactics.history.size()==1 and scene.state.battle.tactics.history[0].size()==768 and scene.state.battle.tactics.moves.size()==1,"combat button automatically starts a new history segment")
	check(scene.clash_step.is_visible_in_tree() and scene.clash_auto.is_visible_in_tree(),"combat controls stay available across segment boundary")
	replay()
	var error:String=""
	for i in range(340):
		error=scene.native.advance_clash()
		if not error.is_empty():break
	scene.refresh()
	scene.show_battle_details()
	check(error.is_empty() and scene.state.battle.tactics.moves.size()==341 and scene.state.battle.tactics.attack.stage=="clash_running","actual combat crosses former 1024-event limit")
	replay()
	check(scene.native.load_session("res://tests/long-clash-result.json").is_empty(),"load complete 3358-step combat and four archive segments")
	scene.refresh()
	scene.show_battle_details()
	check(scene.clash_result_close.is_visible_in_tree() and not scene.clash_result_close.disabled,"long-battle defeat result remains reachable")
	scene.clash_result_close.emit_signal("pressed")
	check(not scene.state.battle.tactics.has("attack") and scene.state.battle.tactics.history.size()==4,"result returns to tactical map with all history retained")
	replay()
	check(scene.native.load_session("res://tests/computer-plan-start.json").is_empty() and scene.native.begin_tactics().is_empty(),"start legal formation-command storage stress")
	for i in range(24580):
		error=scene.native.adjust_tactical_formation(0,0)
		if not error.is_empty():break
	check(error.is_empty(),"24580 legal commands automatically span thirty-two archives")
	check(scene.native.save_session(path).is_empty(),"write large segmented save")
	var file=FileAccess.open(path,FileAccess.READ)
	check(file.get_length()>1024*1024,"valid saved file exceeds old one-megabyte loader limit")
	file.close()
	replay()
	check(scene.native.begin_tactical_retreat(2).is_empty() and scene.native.confirm_tactical_retreat().is_empty() and scene.native.finish_tactical_retreat().is_empty(),"long-history battle can still finish commander withdrawal")
	check(scene.native.session_snapshot().battle==null,"completed battle releases its archived log")
	replay()
	scene.queue_free()
	await process_frame
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL HISTORY: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
