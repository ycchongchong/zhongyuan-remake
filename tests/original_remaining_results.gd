extends SceneTree
var failures=0
var scene:Control
var path="user://native-remaining-results-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var snapshot:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"new result step reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize remaining battle results")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	for variant in [12,145,0]:
		var filename="res://tests/tactical-last-defender-clash.json" if variant==0 else "res://tests/tactical-defender-clash.json"
		check(scene.native.load_session(filename).is_empty(),"load legal clash action history")
		scene.refresh()
		scene.show_battle_details()
		var prisoner:int=12 if variant==0 else variant
		var clash:Dictionary=scene.state.battle.tactics.attack.clash
		var army:int=0 if int(clash.first)==prisoner else 1
		for i in range(3):scene.clash_orders[army*4+3].emit_signal("pressed")
		scene.surrender_submit.emit_signal("pressed")
		scene.deployment_action("surrender_yes")
		for i in range(2):scene.deployment_action("surrender_next")
		if variant==0:
			check(scene.state.battle.army_defeat_result_available and not scene.clash_result_close.disabled and scene.clash_result_close.text.contains("全军战果"),"last defender result exposes supported settlement")
		else:check(not scene.state.battle.army_defeat_result_available,"surviving defenders do not trigger false victory")
		scene.clash_result_close.emit_signal("pressed")
		if variant!=0:
			scene.deployment_action("end_tactical_turn")
			check(scene.native.begin_tactical_retreat(2).is_empty() and scene.native.confirm_tactical_retreat().is_empty(),"attacker commander withdraws surviving units")
			scene.refresh()
			scene.show_battle_details()
			reload_step()
			scene.retreat_finish.emit_signal("pressed")
		check(scene.withdrawal_next.visible and not scene.tactical_end.visible and int(scene.state.battle.tactics.turn_reason)==(3 if variant==0 else 36),"correct branch opens and holds tactical commands")
		reload_step()
		var resources:Array=scene.state.sram.slice(12*36,12*36+16)
		var final_stage:int=29 if variant==0 else 17
		for i in range(42):
			scene.withdrawal_next.emit_signal("pressed")
			reload_step()
			if int(scene.state.battle.tactics.settlement.stage)==final_stage:break
		check(int(scene.state.battle.tactics.settlement.stage)==final_stage and scene.withdrawal_next.text.contains("返回战略地图"),"new result reaches final report")
		if variant==0:check((int(scene.state.sram[12*36])&7)==4 and scene.battle_description.text.contains("已归进攻方"),"defender defeat occupies the city")
		else:check(scene.state.sram.slice(12*36,12*36+16)==resources and scene.battle_description.text.contains("守方保有城池"),"attacker withdrawal preserves defender ownership and resources")
		await process_frame
		if DisplayServer.get_name()!="headless" and variant in [0,12]:root.get_texture().get_image().save_png("res://defender-defeat-result-preview.png" if variant==0 else "res://attacker-result-preview.png")
		scene.withdrawal_next.emit_signal("pressed")
		check(scene.state.battle==null and scene.state.phase=="player_commands" and not scene.battle_details.visible,"result returns to strategic commands")
		reload_step()
		var destination:int=12 if variant in [0,145] else 13
		check(scene.state.sram.slice(destination*36+16,destination*36+28).has(float(prisoner)),"prisoner reaches branch-specific city roster")
	check(scene.native.load_session("res://tests/clash-defeat-3.json").is_empty(),"load ruler defeat boundary")
	scene.refresh()
	scene.show_battle_details()
	for i in range(2):scene.deployment_action("clash_defeat_next")
	check(not scene.state.battle.army_defeat_result_available and scene.state.battle.human_failure_available and not scene.clash_result_close.disabled and not scene.native.begin_defender_defeat_result().is_empty(),"ruler defeat cannot be forced into ordinary army settlement")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL REMAINING RESULTS: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
