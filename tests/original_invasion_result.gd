extends SceneTree
var failures=0
var scene:Control
var path="user://native-invasion-result-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"invasion result reloads exactly")
func settle(reason:int):
	check(int(scene.state.battle.tactics.turn_reason)==reason and scene.withdrawal_next.is_visible_in_tree() and not scene.computer_next.visible and not scene.tactical_end.visible,"invasion exposes settlement with tactical controls locked")
	check(not scene.native.finish_withdrawal_result().is_empty(),"unfinished settlement cannot return to map")
	var final_stage:int=13 if reason==2 else 4 if reason==20 else 17
	for i in range(50):
		scene.withdrawal_next.emit_signal("pressed")
		reload_step()
		if int(scene.state.battle.tactics.settlement.stage)==final_stage:break
	check(int(scene.state.battle.tactics.settlement.stage)==final_stage and scene.withdrawal_next.text.contains("返回战略地图"),"completed invasion report offers strategic return")
	var before:Dictionary=scene.native.session_snapshot()
	check(not scene.native.advance_withdrawal_result().is_empty() and scene.native.session_snapshot()==before,"settlement cannot repeat transfers")
	scene.withdrawal_next.emit_signal("pressed")
	check(scene.state.battle==null and scene.state.phase=="player_commands" and int(scene.state.sram[0xd8b])==4 and not scene.battle_details.visible,"close invasion and reach next human ruler")
	check(int(scene.state.sram[0xd9f])==(2 if reason==2 else 3),"next ruler receives original command books")
	reload_step()
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize invasion result interface")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/attacker-invasion-far.json").is_empty(),"load completed defense")
	scene.refresh()
	scene.show_battle_details()
	scene.tactical_start.emit_signal("pressed")
	for i in range(100):
		if not scene.state.battle.tactics.has("turn_boundary"):break
		scene.computer_next.emit_signal("pressed")
	for slot in range(12):
		if int(scene.state.sram[0xdaa+slot*2])==255:continue
		check(scene.native.select_tactical_unit(slot).is_empty(),"select withdrawing human defender")
		scene.refresh()
		scene.show_battle_details()
		check(scene.retreat_button.is_visible_in_tree() and not scene.retreat_button.disabled,"human withdrawal is reachable during invasion")
		scene.retreat_button.emit_signal("pressed")
		check(scene.retreat_confirm.is_visible_in_tree() and scene.retreat_cancel.is_visible_in_tree(),"human withdrawal offers confirmation and cancel")
		reload_step()
		scene.retreat_cancel.emit_signal("pressed")
		scene.retreat_button.emit_signal("pressed")
		scene.retreat_confirm.emit_signal("pressed")
		check(scene.retreat_finish.is_visible_in_tree() and not scene.retreat_confirm.visible,"human withdrawal report is reachable")
		reload_step()
		scene.retreat_finish.emit_signal("pressed")
	settle(2)
	for fixture in ["open","full","commander"]:
		check(scene.native.load_session("res://tests/invasion-retreat-%s.json" % fixture).is_empty(),"load computer withdrawal reached through legal tactical events")
		scene.refresh()
		scene.show_battle_details()
		check(scene.computer_next.is_visible_in_tree() and not scene.computer_next.disabled and scene.computer_next.text=="执行电脑进攻军撤退","held computer withdrawal now has an executable button")
		var retreats:int=0
		for i in range(80):
			var t:Dictionary=scene.state.battle.tactics
			if t.get("turn_boundary","")=="battle_result":break
			var before:Dictionary=scene.native.session_snapshot()
			var withdrawing:bool=t.attacker_ai.stage=="held_retreat"
			scene.computer_next.emit_signal("pressed")
			if withdrawing:
				retreats+=1
				check(scene.retreat_finish.is_visible_in_tree() and not scene.computer_next.visible and not scene.retreat_confirm.visible,"computer withdrawal goes directly to reachable result")
				check(scene.battle_description.text.contains("撤退已消耗 1 点机动力"),"computer retreat report is not overwritten by turn instructions")
				check(int(scene.state.battle.tactics.points)==int(before.battle.tactics.points)-1 and int(scene.state.battle.tactics.computer_cursor)==int(before.battle.tactics.computer_cursor),"computer withdrawal charges once and retains scan position")
				reload_step()
				scene.retreat_finish.emit_signal("pressed")
				if fixture=="full":
					check(scene.native.session_snapshot().sram==before.sram and scene.computer_next.is_visible_in_tree(),"full city keeps the unit and resumes computer decision")
					break
		check(retreats==(4 if fixture=="open" else 1),"expected original computer withdrawals")
		if fixture!="full":settle(20 if fixture=="commander" else 36)
	scene.queue_free()
	await process_frame
	print("ORIGINAL INVASION RESULT: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
