extends SceneTree
var failures=0
var scene:Control
var path="user://native-computer-motion-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"computer motion and clash boundary reload exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize computer movement and attack")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/computer-motion-attack.json").is_empty(),"load legal five-step human approach and computer no-strategy decision")
	scene.refresh()
	scene.show_battle_details()
	check(scene.computer_next.visible and not scene.computer_next.disabled and scene.computer_next.text=="继续电脑移动与攻击","no-strategy decision exposes continuation")
	scene.computer_next.emit_signal("pressed")
	var snapshot:Dictionary=scene.native.session_snapshot()
	var t:Dictionary=snapshot.battle.tactics
	check(t.computer_motion.kind=="attack" and int(t.computer_motion.slot)==3 and int(t.computer_motion.direction)==3 and int(t.points)==21,"defender selects adjacent target and spends exactly three mobility")
	check(t.attack.stage=="clash_ready" and t.attack.computer and not scene.computer_next.visible,"computer attack enters clash setup and hides advance control")
	check(int(t.attack.clash.first)==128 and int(t.attack.clash.second)==145,"original terrain order preserves correct clash identities")
	reload_step()
	check(not scene.native.continue_computer_motion().is_empty() and not scene.native.cancel_tactical_attack().is_empty() and not scene.native.advance_computer_tactics().is_empty() and scene.native.session_snapshot()==snapshot,"computer attack cannot repeat, cancel or skip")
	scene.clash_start.emit_signal("pressed")
	check(scene.state.battle.tactics.attack.stage=="clash_orders","computer-initiated encounter starts")
	scene.refresh()
	reload_step()
	var clash:Dictionary=scene.native.session_snapshot().battle.tactics.attack.clash
	var human:int=0 if int(clash.first)==145 else 1
	check(int(clash.players[human])!=0 and int(clash.players[human^1])==0 and not scene.native.cycle_clash_order(human^1,3).is_empty(),"only player target can receive human orders")
	for i in range(3):
		scene.clash_orders[human*4+3].emit_signal("pressed")
		check(int(scene.state.battle.tactics.attack.clash.runtime.orders[human*4+3])==i+1,"select human surrender order")
	scene.surrender_submit.emit_signal("pressed")
	scene.surrender_yes.emit_signal("pressed")
	scene.surrender_next.emit_signal("pressed")
	scene.surrender_next.emit_signal("pressed")
	check(scene.state.battle.tactics.attack.stage=="clash_result","actual encounter reaches surrender result")
	reload_step()
	scene.clash_result_close.emit_signal("pressed")
	check(not scene.state.battle.tactics.has("attack"),"return to tactical computer turn")
	scene.refresh()
	check(scene.computer_next.visible and not scene.computer_next.disabled and scene.state.battle.tactics.turn_boundary=="computer","computer advance control returns after result")
	reload_step()
	scene.computer_next.emit_signal("pressed")
	scene.computer_next.emit_signal("pressed")
	check(int(scene.state.battle.tactics.computer_plan.slot)==2,"computer continues original descending unit scan")
	reload_step()
	check(scene.native.load_session("res://tests/computer-motion-held.json").is_empty(),"load original flanking boundary")
	scene.refresh()
	scene.computer_next.emit_signal("pressed")
	check(scene.state.battle.tactics.computer_motion.kind=="pending" and not scene.computer_next.disabled and scene.computer_next.text=="继续电脑绕行","saved flank boundary exposes new continuation")
	scene.computer_next.emit_signal("pressed")
	check(scene.state.battle.tactics.computer_motion.kind=="scan" and not scene.computer_next.disabled,"completed empty-ray decision offers next-unit scan")
	reload_step()
	snapshot=scene.native.session_snapshot()
	check(not scene.native.advance_computer_tactics().is_empty() and not scene.native.continue_computer_motion().is_empty() and scene.native.session_snapshot()==snapshot,"pending flanking cannot reroll or silently skip")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL COMPUTER MOTION: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
