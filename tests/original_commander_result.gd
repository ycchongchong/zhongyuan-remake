extends SceneTree
var failures=0
var scene:Control
var path="user://native-commander-result-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var snapshot:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"commander result reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize commander result")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	for variant in range(3):
		var filename:String=["tactical-commander-clash","tactical-last-commander-clash","clash-defeat-2"][variant]
		check(scene.native.load_session("res://tests/%s.json" % filename).is_empty(),"load legal commander clash history")
		scene.refresh()
		scene.show_battle_details()
		if variant<2:
			var clash:Dictionary=scene.state.battle.tactics.attack.clash
			var army:int=0 if int(clash.first)==145 else 1
			for i in range(3):scene.clash_orders[army*4+3].emit_signal("pressed")
			scene.surrender_submit.emit_signal("pressed")
			scene.deployment_action("surrender_yes")
			for i in range(2):scene.deployment_action("surrender_next")
		else:
			for i in range(2):scene.clash_defeat_next.emit_signal("pressed")
		check(scene.state.battle.commander_defeat_result_available and not scene.state.battle.army_defeat_result_available and not scene.clash_result_close.disabled and scene.clash_result_close.text.contains("主将离场"),"commander loss exposes its exact result branch")
		reload_step()
		scene.clash_result_close.emit_signal("pressed")
		check(scene.withdrawal_next.visible and not scene.tactical_end.visible and int(scene.state.battle.tactics.turn_reason)==20,"commander result holds tactical commands")
		reload_step()
		var resources:Array=scene.state.sram.slice(12*36,12*36+16)
		var previous_stage:int=0
		for i in range(42):
			scene.withdrawal_next.emit_signal("pressed")
			reload_step()
			var stage:int=int(scene.state.battle.tactics.settlement.stage)
			check(stage==previous_stage or stage==previous_stage+1,"captives settle before surviving army returns")
			previous_stage=stage
			if stage==4:break
		check(previous_stage==4 and scene.withdrawal_next.text.contains("返回战略地图"),"commander result reaches final report")
		check(scene.state.sram.slice(12*36,12*36+16)==resources and scene.battle_description.text.contains("守方保有城池"),"defender ownership and resources remain unchanged")
		await process_frame
		if DisplayServer.get_name()!="headless" and variant==0:root.get_texture().get_image().save_png("res://commander-result-preview.png")
		scene.withdrawal_next.emit_signal("pressed")
		check(scene.state.battle==null and scene.state.phase=="player_commands" and not scene.battle_details.visible,"commander result returns to strategic commands")
		reload_step()
		if variant<2:check(scene.state.sram.slice(12*36+16,12*36+28).has(145.0),"surrendered commander remains in defending city")
		for id in [143.0,144.0]:check(scene.state.sram.slice(13*36+16,13*36+28).has(id),"surviving attackers return to source roster")
	check(scene.native.load_session("res://tests/clash-defeat-3.json").is_empty(),"load separate ruler result")
	scene.refresh()
	scene.show_battle_details()
	for i in range(2):scene.clash_defeat_next.emit_signal("pressed")
	check(not scene.state.battle.commander_defeat_result_available and scene.state.battle.human_failure_available and not scene.clash_result_close.disabled and not scene.native.begin_commander_defeat_result().is_empty(),"ruler defeat cannot use ordinary commander result")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL COMMANDER RESULT: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
