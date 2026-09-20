extends SceneTree
var failures=0
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func _init():call_deferred("run")
func run():
	var scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize tactical handover UI")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/tactical-turn-start.json").is_empty(),"load legacy two-player tactical start")
	scene.refresh()
	scene.show_battle_details()
	check(scene.tactical_end.visible and scene.battle_description.text.contains("进攻方"),"attacker can finish its tactical action")
	scene.deployment_action("end_tactical_turn")
	check(int(scene.state.battle.tactics.side)==0 and int(scene.state.battle.tactics.carry)==160 and scene.battle_description.text.contains("守城方"),"handover retains up to ten mobility and opens defender controls")
	check(scene.retreat_button.visible and scene.battle_defenders.is_item_selectable(1) and not scene.battle_attackers.is_item_selectable(0),"defender withdrawal is available and only the defending roster is selectable")
	scene.battle_defenders.emit_signal("item_selected",1)
	check(int(scene.state.battle.tactics.selected)==1,"defending roster selects the correct ledger slot")
	scene.formation_next.emit_signal("pressed")
	scene.scout_button.emit_signal("pressed")
	check(scene.state.battle.tactics.has("scout") and not scene.tactical_end.visible,"scouting an attacker holds turn handover")
	scene.scout_close.emit_signal("pressed")
	var moved:bool=false
	for direction in range(4):
		if scene.native.move_tactical(1,direction).is_empty():
			moved=true
			break
	check(moved,"defender movement is connected")
	scene.refresh()
	scene.show_battle_details()
	var path="user://native-tactical-turn-test.json"
	var snapshot:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"defender commands save and restore exactly")
	await process_frame
	if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://tactical-defender-preview.png")
	for i in range(21):
		if i==20:scene.battle_defenders.emit_signal("item_selected",1)
		scene.deployment_action("end_tactical_turn")
		snapshot=scene.native.session_snapshot()
		check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"successive handovers save round and carry exactly")
	check(scene.state.battle.tactics.get("turn_boundary","")=="time_limit" and int(scene.state.battle.tactics.round)==11 and int(scene.state.battle.tactics.selected)==1 and not scene.tactical_end.visible,"eleven rounds enter original time-limit result without a fake victory")
	check(scene.native.load_session("res://tests/tactical-turn-single.json").is_empty(),"load single-player tactical start")
	scene.refresh()
	scene.deployment_action("end_tactical_turn")
	check(scene.state.battle.tactics.get("turn_boundary","")=="computer" and not scene.tactical_end.visible and not scene.support_controls.visible,"computer turn remains held instead of being skipped")
	check(scene.native.load_session("res://tests/tactical-defender-clash.json").is_empty(),"load legal defender counterattack")
	scene.refresh()
	scene.show_battle_details()
	var clash:Dictionary=scene.state.battle.tactics.attack.clash
	var army:int=0 if int(clash.first)==12 else 1
	check(int(clash.tactical_side)==0 and int(clash.players[army])==2,"counterattacking defender remains player two on clash screen")
	for i in range(3):scene.clash_orders[army*4+3].emit_signal("pressed")
	scene.surrender_submit.emit_signal("pressed")
	scene.deployment_action("surrender_yes")
	for i in range(2):scene.deployment_action("surrender_next")
	check(scene.state.battle.tactics.attack.result.can_continue and int(scene.state.battle.tactics.side)==0,"defender surrender settles to remaining defending units")
	scene.clash_result_close.emit_signal("pressed")
	check(not scene.state.battle.tactics.has("attack") and scene.battle_description.text.contains("守城方") and scene.battle_defenders.is_item_selectable(0),"defender can continue after its counterattack")
	snapshot=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"counterattack and captive record reload exactly")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL TACTICAL TURN: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
