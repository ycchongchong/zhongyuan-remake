extends SceneTree
var failures=0
var scene:Control
var path="user://native-computer-scan-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"scan boundary reloads with exact origin, unit and RNG")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize computer scan continuation")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/computer-flank-scan.json").is_empty(),"load old no-ray flank boundary")
	scene.refresh()
	scene.show_battle_details()
	scene.computer_next.emit_signal("pressed")
	check(scene.computer_next.text=="检查下一部队" and not scene.computer_next.disabled,"previous held scan now has an actionable control")
	reload_step()
	var selected:Array[int]=[]
	for step in range(25):
		if not scene.state.battle.tactics.has("turn_boundary"):break
		if scene.computer_next.text=="检查下一部队":
			scene.computer_next.emit_signal("pressed")
			if scene.state.battle.tactics.has("computer_plan"):
				selected.append(int(scene.state.battle.tactics.computer_plan.slot))
				check(int(scene.state.battle.tactics.computer_scan.marker)==11,"sparse roster retains empty first candidate as scan origin")
		else:
			check(not scene.computer_next.disabled,"every supported no-action stage can progress")
			scene.computer_next.emit_signal("pressed")
		reload_step()
	check(selected==[2,1,0],"UI visits remaining defender units in original descending order")
	var t:Dictionary=scene.state.battle.tactics
	check(not t.has("turn_boundary") and int(t.side)==128 and int(t.round)==1 and int(t.points)==30 and int(t.carry)==0,"full scan returns player turn without inventing computer carry")
	check(not scene.computer_next.visible and scene.tactical_end.visible,"player tactical controls return after full scan")
	check(t.has("last_computer_scan") and int(t.last_computer_scan.marker)==11 and int(t.last_computer_scan.command)==4,"completed scan report survives the handoff")
	var before:Dictionary=scene.native.session_snapshot()
	check(not scene.native.continue_computer_scan().is_empty() and scene.native.session_snapshot()==before,"cannot repeat computer end command on player turn")
	check(scene.native.end_tactical_turn().is_empty() and scene.native.advance_computer_tactics().is_empty() and scene.native.plan_computer_tactics().is_empty(),"next computer turn can start normally")
	reload_step()
	scene.refresh()
	scene.show_battle_details()
	check(not scene.state.battle.tactics.has("computer_scan") and int(scene.state.battle.tactics.points)==24,"fresh scan has no stale marker or fabricated bonus")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL COMPUTER SCAN: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
