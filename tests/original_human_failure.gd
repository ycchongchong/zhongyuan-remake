extends SceneTree
var failures=0
var scene:Control
var path="user://native-human-failure-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var snapshot:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"human failure step reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize human failure")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	for fixture in ["clash-single-ruler-defeat","clash-defeat-3","clash-second-ruler-defeat"]:
		var single:bool=fixture=="clash-single-ruler-defeat"
		check(scene.native.load_session("res://tests/%s.json" % fixture).is_empty(),"load actual human ruler defeat history")
		scene.refresh()
		scene.show_battle_details()
		for i in range(2):scene.clash_defeat_next.emit_signal("pressed")
		check(scene.state.battle.human_failure_available and not scene.clash_result_close.disabled and scene.clash_result_close.text.contains("失败报告"),"human failure has an explicit report entry")
		var before:Array=scene.state.sram.duplicate()
		scene.clash_result_close.emit_signal("pressed")
		reload_step()
		check(scene.human_failure_controls.visible and scene.human_failure_next.disabled==single and not scene.clash_result_close.visible and not scene.battle_overlay.visible and not scene.battle_forces.visible,"failure presentation replaces combat and locks terminal continuation")
		check(scene.battle_description.text.contains("刘备") and scene.battle_description.text.contains("曹操") and scene.battle_description.text.contains("请重新来过") and scene.state.sram==before,"report uses actual ruler identities without settlement side effects")
		check(not scene.native.begin_ruler_defeat_result().is_empty() and not scene.native.finish_clash_result().is_empty() and not scene.native.end_tactical_turn().is_empty(),"failure screen cannot bypass report through other commands")
		await process_frame
		if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://human-failure-%s-preview.png" % ("single" if single else "dual"))
		if single:
			check(not scene.native.finish_human_failure().is_empty() and scene.native.session_snapshot().sram==before,"last player defeat cannot transfer territory or resume")
			continue
		scene.human_failure_next.emit_signal("pressed")
		reload_step()
		check(not scene.human_failure_controls.visible and scene.withdrawal_next.visible and int(scene.state.battle.tactics.turn_reason)==148,"surviving player resumes original attacking-ruler result")
		for i in range(42):
			scene.withdrawal_next.emit_signal("pressed")
			reload_step()
			if int(scene.state.battle.tactics.settlement.stage)==4:break
		check(not scene.native.finish_withdrawal_result().is_empty(),"territory processing cannot be skipped")
		for city in range(30):
			scene.withdrawal_next.emit_signal("pressed")
			reload_step()
		check(int(scene.state.battle.tactics.annexation.next_city)==30 and int(scene.state.sram[0xd8f+4*4])==0,"all thirty cities processed and defeated ruler commands cleared")
		scene.withdrawal_next.emit_signal("pressed")
		check(scene.state.battle==null and scene.state.phase!="ending" and not scene.battle_details.visible,"second player campaign survives first player defeat")
		reload_step()
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL HUMAN FAILURE: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
