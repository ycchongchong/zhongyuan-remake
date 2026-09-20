extends SceneTree
var failures=0
var scene:Control
var path="user://native-computer-scratch-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"scratch continuation reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize inherited AI scratch continuation")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	for variant in ["move","attack"]:
		check(scene.native.load_session("res://tests/computer-scratch-resume-%s.json" % variant).is_empty(),"load old history with a blocked second unit")
		scene.refresh()
		scene.show_battle_details()
		var before:Dictionary=scene.native.session_snapshot()
		check(int(before.battle.tactics.points)==2 and before.battle.tactics.has("computer_scan"),"original stop follows scan with two remaining mobility")
		check(not scene.native.continue_computer_occupied_fort().is_empty() and scene.native.session_snapshot()==before,"legacy entry remains atomic without guessed scratch")
		check(scene.computer_next.visible and not scene.computer_next.disabled and scene.computer_next.text=="应对敌军占堡","UI offers inherited-state continuation")
		reload_step()
		scene.computer_next.emit_signal("pressed")
		var after:Dictionary=scene.native.session_snapshot()
		check(after.battle.tactics.computer_motion.kind=="move" and int(after.battle.tactics.points)==0 and after.sram!=before.sram,"button resumes actual movement and charges once")
		check(int(after.battle.tactics.computer_motion.scratch21)==105 and after.battle.tactics.moves[-1].kind=="computer_occupied_fort_resume","result records derived scratch through a separate event")
		reload_step()
		check(not scene.native.resume_computer_occupied_fort().is_empty() and scene.native.session_snapshot()==after,"no repeated move or reroll")
		scene.computer_next.emit_signal("pressed")
		check(not scene.state.battle.tactics.has("turn_boundary") and int(scene.state.battle.tactics.side)==128 and int(scene.state.battle.tactics.round)==int(before.battle.tactics.round)+1,"recovered move returns the next turn to the player")
		check(scene.tactical_end.visible and not scene.computer_next.visible,"player controls return after recovery")
		reload_step()
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL COMPUTER SCRATCH: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
