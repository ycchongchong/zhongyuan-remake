extends SceneTree
var failures=0
var scene:Control
var path="user://native-clash-tail-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"recovered battle saves and reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize original memory recovery interface")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/clash-tail-boundary.json").is_empty(),"load actual formerly ambiguous invasion")
	scene.refresh()
	scene.show_battle_details()
	check(scene.clash_strategy_resume.is_visible_in_tree() and not scene.clash_step.visible,"ambiguous computer decision exposes continuation")
	var before:Dictionary=scene.native.session_snapshot()
	check(not scene.native.resume_clash_strategy().is_empty() and scene.native.session_snapshot()==before,"legacy consensus keeps ambiguous scene unchanged")
	scene.clash_strategy_resume.emit_signal("pressed")
	check(scene.state.battle.tactics.attack.stage=="clash_running" and scene.state.battle.tactics.attack.clash.runtime.tail.size()==3 and int(scene.state.battle.tactics.attack.clash.runtime.tail[0])==0 and int(scene.state.battle.tactics.attack.clash.runtime.tail[1])==0 and int(scene.state.battle.tactics.attack.clash.runtime.tail[2])==0,"continue button restores original reset-memory decision")
	check(scene.state.sram==before.sram and scene.state.battle.tactics.points==before.battle.tactics.points and scene.state.battle.tactics.attack.clash.runtime.units==before.battle.tactics.attack.clash.runtime.units,"recovery does not change armies or charge mobility")
	check(not scene.clash_strategy_resume.visible and scene.clash_step.is_visible_in_tree() and scene.clash_auto.is_visible_in_tree(),"recovered battle exposes normal combat controls")
	reload_step()
	before=scene.native.session_snapshot()
	check(not scene.native.recover_clash_strategy().is_empty() and scene.native.session_snapshot()==before,"duplicate recovery is atomic")
	var steps:int=0
	for i in range(600):
		if scene.state.battle.tactics.attack.stage!="clash_running":break
		scene.clash_step.emit_signal("pressed")
		steps+=1
	check(steps==112 and scene.state.battle.tactics.attack.get("boundary","")=="general_defeat" and scene.clash_defeat_next.is_visible_in_tree(),"112 real combat steps reach defeat report")
	reload_step()
	for i in range(2):scene.clash_defeat_next.emit_signal("pressed")
	check(scene.clash_result_close.is_visible_in_tree() and scene.state.battle.tactics.attack.stage=="clash_result","defeat reaches result confirmation")
	scene.clash_result_close.emit_signal("pressed")
	check(scene.withdrawal_next.is_visible_in_tree() and int(scene.state.battle.tactics.turn_reason)==3,"result enters defender army settlement")
	for i in range(45):
		scene.withdrawal_next.emit_signal("pressed")
		if int(scene.state.battle.tactics.settlement.stage)==29:break
	check(int(scene.state.battle.tactics.settlement.stage)==29,"recovered battle completes army and captive settlement")
	reload_step()
	scene.withdrawal_next.emit_signal("pressed")
	check(scene.state.battle==null and scene.state.phase=="player_commands" and not scene.battle_details.visible and int(scene.state.sram[0xd8b])==4 and (int(scene.state.sram[21*36])&7)==3,"recovered battle transfers city and resumes next ruler")
	reload_step()
	scene.queue_free()
	await process_frame
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL CLASH TAIL: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
