extends SceneTree
var failures=0
var scene:Control
var path="user://native-strategy-effect-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"strategy execution boundary reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize strategy effects")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/strategy-effect-start.json").is_empty(),"load legal computer choice after player movement")
	scene.refresh()
	scene.show_battle_details()
	check(not scene.computer_next.disabled and scene.computer_next.text=="执行电脑计策","computer has an execution control")
	scene.computer_next.emit_signal("pressed")
	var snapshot:Dictionary=scene.native.session_snapshot()
	var result:Dictionary=snapshot.battle.tactics.strategy_result
	check(result.success and int(result.converted_slot)==4 and int(result.cost)==10 and int(result.points_after)==14,"computer conversion succeeds with original mobility charge")
	check(int(snapshot.sram[0xdc2])==255 and int(snapshot.sram[0xdaa+8])==145 and int(snapshot.sram[0xe1a+0x47])==0x14,"conversion updates both ledgers and board occupancy")
	check(not scene.computer_next.disabled and scene.computer_next.text=="确认计策结果" and scene.battle_description.text.contains("转入守军"),"result is shown before next computer decision")
	reload_step()
	check(not scene.native.execute_computer_strategy().is_empty() and not scene.native.advance_computer_tactics().is_empty() and scene.native.session_snapshot()==snapshot,"pending result cannot execute twice or skip confirmation")
	await process_frame
	if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://strategy-effect-preview.png")
	scene.computer_next.emit_signal("pressed")
	check(scene.state.battle.tactics.computer.kind=="acted" and not scene.state.battle.tactics.has("strategy_result") and not scene.computer_next.disabled,"confirmed result resumes computer control")
	reload_step()
	scene.computer_next.emit_signal("pressed")
	check(scene.state.battle.tactics.computer.kind=="plan","computer performs a fresh assessment after effect")
	reload_step()
	check(scene.native.load_session("res://tests/strategy-effect-failure.json").is_empty(),"load legal failed conversion scenario")
	scene.refresh()
	scene.computer_next.emit_signal("pressed")
	check(not scene.state.battle.tactics.strategy_result.success and int(scene.state.battle.tactics.points)==14 and scene.battle_description.text.contains("未奏效"),"failed strategy still charges mobility and displays failure")
	reload_step()
	scene.computer_next.emit_signal("pressed")
	reload_step()
	check(scene.native.load_session("res://tests/strategy-effect-commander.json").is_empty(),"load commander conversion scenario")
	scene.refresh()
	scene.computer_next.emit_signal("pressed")
	reload_step()
	scene.computer_next.emit_signal("pressed")
	check(scene.withdrawal_next.visible and not scene.computer_next.visible and int(scene.state.battle.tactics.turn_reason)==20,"converted commander enters battle settlement controls")
	reload_step()
	for i in range(25):
		scene.withdrawal_next.emit_signal("pressed")
		reload_step()
		if int(scene.state.battle.tactics.settlement.stage)==4:break
	scene.withdrawal_next.emit_signal("pressed")
	check(scene.state.battle==null and scene.state.phase=="player_commands" and not scene.battle_details.visible,"strategy conversion can complete the battle and return to campaign")
	reload_step()
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL STRATEGY EFFECT: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
