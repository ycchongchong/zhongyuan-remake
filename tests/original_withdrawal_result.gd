extends SceneTree
var failures=0
var scene:Control
var path="user://native-withdrawal-result-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var snapshot:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"settlement step reloads exactly")
func retire():
	for slot in range(12):
		if int(scene.state.sram[0xdaa+slot*2])==255:continue
		check(scene.native.begin_tactical_retreat(slot).is_empty() and scene.native.confirm_tactical_retreat().is_empty() and scene.native.finish_tactical_retreat().is_empty(),"defender withdraws before settlement")
		scene.refresh()
	scene.show_battle_details()
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize post-withdrawal result UI")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	for prisoner in [-1,12,145]:
		var file="res://tests/tactical-turn-start.json" if prisoner==-1 else "res://tests/tactical-defender-clash.json"
		check(scene.native.load_session(file).is_empty(),"load legal tactical/clash entry")
		scene.refresh()
		scene.show_battle_details()
		if prisoner==-1:
			check(not scene.withdrawal_next.visible and not scene.native.advance_withdrawal_result().is_empty(),"cannot start settlement during ongoing tactics")
			scene.deployment_action("end_tactical_turn")
		else:
			var clash:Dictionary=scene.state.battle.tactics.attack.clash
			var army:int=0 if int(clash.first)==prisoner else 1
			for i in range(3):scene.clash_orders[army*4+3].emit_signal("pressed")
			scene.surrender_submit.emit_signal("pressed")
			scene.deployment_action("surrender_yes")
			for i in range(2):scene.deployment_action("surrender_next")
			scene.clash_result_close.emit_signal("pressed")
		retire()
		check(scene.withdrawal_next.visible and not scene.tactical_end.visible and scene.battle_description.text.contains("战后结算"),"withdrawal result offers settlement while tactical commands stay locked")
		check(not scene.native.finish_withdrawal_result().is_empty(),"cannot close while transfers are pending")
		for i in range(45):
			scene.withdrawal_next.emit_signal("pressed")
			reload_step()
			if int(scene.state.battle.tactics.settlement.stage)==13:break
		check(int(scene.state.battle.tactics.settlement.stage)==13 and (int(scene.state.sram[12*36])&7)==4 and scene.battle_description.text.contains("已归进攻方"),"report shows city occupation and final resources")
		check(scene.withdrawal_next.text.contains("返回战略地图"),"report offers strategic return")
		var snapshot:Dictionary=scene.native.session_snapshot()
		check(not scene.native.advance_withdrawal_result().is_empty() and scene.native.session_snapshot()==snapshot,"duplicate settlement cannot charge resources again")
		if prisoner==12:
			await process_frame
			if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://withdrawal-result-preview.png")
		scene.withdrawal_next.emit_signal("pressed")
		check(scene.state.battle==null and scene.state.phase=="player_commands" and not scene.battle_details.visible and scene.status.text.contains("战后结算完成"),"confirm returns to the same ruler's strategic commands")
		reload_step()
		if prisoner!=-1:
			var owner:int=-1
			for city in range(30):
				for slot in range(12):
					if int(scene.state.sram[city*36+16+slot])==prisoner:owner=int(scene.state.sram[city*36])&7
			check(owner==(4 if prisoner==12 else 2),"captives remain in the capturing side's city rosters")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL WITHDRAWAL RESULT: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
