extends SceneTree
var failures=0
var scene:Control
var path="user://native-invasion-endings-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"invading ending reloads exactly")
func load_battle(file:String):
	check(scene.native.load_session("res://tests/"+file+".json").is_empty(),"load real invasion action history")
	scene.refresh()
	scene.show_battle_details()
func settle(reason:int):
	check(scene.withdrawal_next.is_visible_in_tree() and not scene.computer_next.visible and not scene.tactical_end.visible,"final battle exposes settlement instead of tactical actions")
	var last:int=17 if reason==36 else 4 if reason==148 else 29
	for i in range(45):
		scene.withdrawal_next.emit_signal("pressed")
		if int(scene.state.battle.tactics.settlement.stage)==last:break
	check(int(scene.state.battle.tactics.settlement.stage)==last,"invading result reaches final settlement stage")
	reload_step()
	if reason in [131,148]:
		check(not scene.native.finish_withdrawal_result().is_empty(),"ruler territory cannot be skipped")
		for i in range(30):scene.withdrawal_next.emit_signal("pressed")
		check(int(scene.state.battle.tactics.annexation.next_city)==30 and scene.withdrawal_next.text.contains("返回战略地图"),"all thirty cities inspected before strategic return")
		reload_step()
	var before:Dictionary=scene.native.session_snapshot()
	check(not scene.native.advance_withdrawal_result().is_empty() and scene.native.session_snapshot()==before,"completed result cannot repeat transfers")
	scene.withdrawal_next.emit_signal("pressed")
	check(scene.state.battle==null and not scene.battle_details.visible and int(scene.state.sram[0xd8b])==(5 if reason==131 else 4),"result returns to next surviving ruler")
	reload_step()
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize invasion ending interface")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	load_battle("invasion-time-limit")
	check(scene.time_limit_next.is_visible_in_tree() and not scene.withdrawal_next.visible and not scene.tactical_end.visible,"actual invasion limit requires report first")
	var resources:Array=scene.state.sram.slice(21*36,21*36+16)
	for i in range(3):
		scene.time_limit_next.emit_signal("pressed")
		reload_step()
		if i<2:check(scene.time_limit_identity.is_visible_in_tree() and scene.battle_description.text.contains("战斗期限已到"),"limit shows original spokesperson and expiry notice")
	check(scene.withdrawal_next.is_visible_in_tree() and not scene.time_limit_next.visible and int(scene.state.battle.tactics.turn_reason)==36,"last limit acknowledgement opens forced withdrawal")
	settle(36)
	check(scene.state.sram.slice(21*36,21*36+16)==resources,"forced withdrawal keeps defending city and resources")
	for name in ["army","single","second","ruler"]:
		load_battle("invasion-defeat-"+name)
		check(scene.clash_result_close.is_visible_in_tree() and not scene.clash_result_close.disabled,"actual invading defeat offers a usable result button")
		scene.clash_result_close.emit_signal("pressed")
		reload_step()
		if name in ["single","second"]:
			check(scene.human_failure_controls.is_visible_in_tree() and not scene.clash_result_close.visible and scene.human_failure_next.disabled==(name=="single"),"human defeat shows terminal or surviving-player continuation")
			check(scene.battle_description.text.contains("请重新来过") and scene.battle_description.text.contains("刘备"),"human report uses defeated ruler identity")
			if name=="single":
				var before:Dictionary=scene.native.session_snapshot()
				check(not scene.native.finish_human_failure().is_empty() and scene.native.session_snapshot()==before,"single-player failure cannot resume or transfer territory")
				continue
			scene.human_failure_next.emit_signal("pressed")
			check(not scene.human_failure_controls.visible and int(scene.state.battle.tactics.turn_reason)==131,"surviving player resumes defeated defender's territory settlement")
		settle(3 if name=="army" else 148 if name=="ruler" else 131)
		check((int(scene.state.sram[21*36])&7)==(4 if name=="ruler" else 3),"invading defeat assigns city to original winner")
	scene.queue_free()
	await process_frame
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL INVASION ENDINGS: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
