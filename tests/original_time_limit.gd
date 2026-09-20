extends SceneTree
var failures=0
var scene:Control
var path="user://native-time-limit-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var snapshot:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"time-limit step reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize time-limit result")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	for prisoner in [0,12,145]:
		check(scene.native.load_session("res://tests/tactical-turn-start.json" if prisoner==0 else "res://tests/tactical-defender-clash.json").is_empty(),"load legal tactical history")
		scene.refresh()
		scene.show_battle_details()
		check(not scene.time_limit_next.visible and not scene.native.advance_time_limit_result().is_empty(),"time-limit cannot be invoked early")
		if prisoner:
			var clash:Dictionary=scene.state.battle.tactics.attack.clash
			var army:int=0 if int(clash.first)==prisoner else 1
			for i in range(3):scene.clash_orders[army*4+3].emit_signal("pressed")
			scene.surrender_submit.emit_signal("pressed")
			scene.deployment_action("surrender_yes")
			for i in range(2):scene.deployment_action("surrender_next")
			scene.clash_result_close.emit_signal("pressed")
		for i in range(22):
			if scene.state.battle.tactics.has("turn_boundary"):break
			scene.tactical_end.emit_signal("pressed")
		check(scene.state.battle.tactics.turn_boundary=="time_limit" and int(scene.state.battle.tactics.round)==11 and scene.time_limit_next.visible and not scene.withdrawal_next.visible and not scene.tactical_end.visible,"eleven actual rounds open time-limit notice")
		reload_step()
		var resources:Array=scene.state.sram.slice(12*36,12*36+16)
		for i in range(3):
			scene.time_limit_next.emit_signal("pressed")
			reload_step()
			if i<2:
				check(scene.time_limit_identity.visible and scene.time_limit_identity.texture.region==Rect2(0,(30+int(scene.state.battle.tactics.time_limit.speaker))*16,48,16) and scene.battle_description.text.contains("战斗期限已到"),"original spokesperson name and limit report display")
				await process_frame
				if DisplayServer.get_name()!="headless" and prisoner==0 and i==1:root.get_texture().get_image().save_png("res://time-limit-notice-preview.png")
		check(scene.state.battle.tactics.turn_boundary=="battle_result" and int(scene.state.battle.tactics.turn_reason)==36 and scene.withdrawal_next.visible and not scene.time_limit_next.visible and not scene.time_limit_identity.visible,"notice opens forced withdrawal without keeping stale controls")
		for i in range(42):
			scene.withdrawal_next.emit_signal("pressed")
			reload_step()
			if int(scene.state.battle.tactics.settlement.stage)==17:break
		check(int(scene.state.battle.tactics.settlement.stage)==17 and scene.withdrawal_next.text.contains("返回战略地图") and scene.state.sram.slice(12*36,12*36+16)==resources,"limit result preserves city and reaches final report")
		await process_frame
		if DisplayServer.get_name()!="headless" and prisoner==12:root.get_texture().get_image().save_png("res://time-limit-result-preview.png")
		scene.withdrawal_next.emit_signal("pressed")
		check(scene.state.battle==null and scene.state.phase=="player_commands" and not scene.battle_details.visible,"forced withdrawal returns to strategic commands")
		reload_step()
		if prisoner:
			var destination:int=13 if prisoner==12 else 12
			check(scene.state.sram.slice(destination*36+16,destination*36+28).has(float(prisoner)),"time-limit captive reaches exact original destination")
	check(scene.native.load_session("res://tests/tactical-turn-single.json").is_empty() and scene.native.end_tactical_turn().is_empty(),"load pending computer turn")
	scene.refresh()
	scene.show_battle_details()
	check(not scene.time_limit_next.visible and not scene.native.advance_time_limit_result().is_empty(),"time-limit control cannot bypass computer turn")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL TIME LIMIT: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
