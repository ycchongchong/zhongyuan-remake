extends SceneTree
var failures=0
var scene:Control
var path="user://native-computer-strategy-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize computer strategy evaluation")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/computer-strategy-start.json").is_empty(),"load actual player movement, handover and unit selection")
	scene.refresh()
	scene.show_battle_details()
	check(scene.computer_next.visible and not scene.computer_next.disabled and scene.computer_next.text.contains("计策"),"selected computer unit can evaluate strategy")
	var before:Dictionary=scene.native.session_snapshot()
	scene.computer_next.emit_signal("pressed")
	var after:Dictionary=scene.native.session_snapshot()
	var decision:Dictionary=after.battle.tactics.computer_strategy
	check(decision.kind=="strategy" and int(decision.strategy)==7 and int(decision.target_slot)==0,"native computer selects original strategy and target")
	check(after.battle.tactics.points==before.battle.tactics.points,"selection does not invent a cost before effects are executed")
	check(not scene.computer_next.disabled and not scene.tactical_end.visible and scene.computer_next.text.contains("执行电脑计策"),"selected strategy can execute through its own computer control")
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==after,"strategy target and RNG reload exactly")
	check(not scene.native.evaluate_computer_strategy().is_empty() and not scene.native.advance_computer_tactics().is_empty() and scene.native.session_snapshot()==after,"held strategy cannot reroll or skip the computer turn")
	await process_frame
	if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://computer-strategy-preview.png")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL COMPUTER STRATEGY: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
