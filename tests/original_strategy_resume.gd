extends SceneTree
var failures=0
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func _init():call_deferred("run")
func run():
	var scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize computer strategy UI")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/clash-strategy-boundary.json").is_empty(),"load legacy left-army NPC pause")
	scene.refresh()
	scene.show_battle_details()
	check(scene.clash_strategy_resume.visible and not scene.clash_step.visible and not scene.clash_auto.visible,"paused computer exposes safe continuation")
	var path="user://native-strategy-resume-test.json"
	var decisions:int=0
	var actions:int=0
	for step in range(600):
		var attack:Dictionary=scene.state.battle.tactics.attack
		if attack.stage=="clash_boundary" and attack.get("boundary","")=="ai_scratch":
			var before:Dictionary=scene.native.session_snapshot()
			scene.clash_strategy_resume.emit_signal("pressed")
			if scene.state.battle.tactics.attack.stage!="clash_running":break
			decisions+=1
			check(scene.state.sram==before.sram and scene.state.battle.tactics.attack.clash.runtime.units==before.battle.tactics.attack.clash.runtime.units and scene.state.battle.tactics.points==before.battle.tactics.points,"computer orders preserve armies, resources and mobility")
			check(not scene.clash_strategy_resume.visible and scene.clash_step.visible and scene.clash_auto.visible,"continued battle restores its controls")
			var snapshot:Dictionary=scene.native.session_snapshot()
			check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"computer decision survives save and load exactly")
			check(not scene.native.resume_clash_strategy().is_empty() and scene.native.session_snapshot()==snapshot,"repeated decision cannot consume randomness")
			if decisions==1:
				await process_frame
				if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://strategy-resume-preview.png")
		elif attack.stage=="clash_running":scene.clash_step.emit_signal("pressed")
		else:break
		actions+=1
	check(decisions==4 and actions==277,"left-army computer makes four decisions and reaches original retreat")
	check(scene.state.battle.tactics.attack.get("boundary","")=="retreat" and scene.clash_retreat_next.visible and not scene.clash_strategy_resume.visible,"retreat uses its existing result flow")
	var snapshot:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"entire resumed clash reloads exactly")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL STRATEGY RESUME: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
