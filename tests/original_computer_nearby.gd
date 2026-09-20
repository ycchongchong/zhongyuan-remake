extends SceneTree
var failures=0
var scene:Control
var path="user://native-computer-nearby-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"nearby role boundary reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize nearby role continuation")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	for role in [1,3]:
		for kind in ["move","attack"]:
			check(scene.native.load_session("res://tests/computer-nearby-%d-%s.json" % [role,kind]).is_empty(),"load naturally assigned role with legal human movement history")
			scene.refresh()
			scene.show_battle_details()
			check(scene.computer_next.visible and not scene.computer_next.disabled and scene.computer_next.text=="搜索附近敌军","previous role stop offers nearby continuation")
			var before:Dictionary=scene.native.session_snapshot()
			var slot:int=int(before.battle.tactics.selected)
			scene.computer_next.emit_signal("pressed")
			check(scene.state.battle.tactics.computer_motion.kind==kind and scene.state.random_cursor==before.random_cursor,"nearby decision executes with no extra RNG")
			reload_step()
			var after:Dictionary=scene.native.session_snapshot()
			check(not scene.native.continue_computer_nearby().is_empty() and scene.native.session_snapshot()==after,"nearby command cannot repeat")
			if kind=="attack":
				check(not scene.computer_next.visible and scene.state.battle.tactics.attack.stage=="clash_ready" and int(scene.state.battle.tactics.points)==int(before.battle.tactics.points)-3,"nearby attack charges once and exposes clash controls")
				scene.clash_start.emit_signal("pressed")
				check(scene.state.battle.tactics.attack.stage=="clash_orders","enter encounter initiated by nearby search")
				reload_step()
				var clash:Dictionary=scene.native.session_snapshot().battle.tactics.attack.clash
				var human:int=0 if int(clash.players[0])!=0 else 1
				for i in range(3):scene.clash_orders[human*4+3].emit_signal("pressed")
				scene.surrender_submit.emit_signal("pressed")
				scene.surrender_yes.emit_signal("pressed")
				scene.surrender_next.emit_signal("pressed")
				scene.surrender_next.emit_signal("pressed")
				check(scene.state.battle.tactics.attack.stage=="clash_result","nearby attack resolves through real battle result")
				reload_step()
				scene.clash_result_close.emit_signal("pressed")
				check(not scene.state.battle.tactics.has("attack") and scene.computer_next.visible,"return to computer tactical turn")
				reload_step()
			else:
				check(int(scene.state.battle.tactics.points)<int(before.battle.tactics.points) and scene.state.sram!=before.sram,"nearby fallback movement updates board and mobility")
			scene.computer_next.emit_signal("pressed")
			scene.computer_next.emit_signal("pressed")
			check(int(scene.state.battle.tactics.computer_plan.slot)==slot-1,"ordinary descending scan resumes after action")
			reload_step()
	for fixture in ["computer-exhausted-attack.json","natural-nearby-exhausted.json"]:
		check(scene.native.load_session("res://tests/"+fixture).is_empty(),"load legal history reaching insufficient attack mobility: "+fixture+"")
		scene.refresh()
		scene.show_battle_details()
		var exhausted_before:Dictionary=scene.native.session_snapshot()
		check(int(exhausted_before.battle.tactics.points) in [1,2] and scene.computer_next.text=="搜索附近敌军","original exhausted attack boundary remains actionable")
		scene.computer_next.emit_signal("pressed")
		check(not scene.state.battle.tactics.has("turn_boundary") and not scene.state.battle.tactics.has("attack") and scene.tactical_end.visible and not scene.computer_next.visible,"insufficient computer attack automatically returns player control")
		check(scene.state.sram==exhausted_before.sram and scene.state.random_cursor==exhausted_before.random_cursor and int(scene.state.battle.tactics.carry)==0,"exhaustion adds no damage, random draw or carry bonus")
		reload_step()
		check(not scene.native.finish_exhausted_computer_attack().is_empty(),"cannot repeat exhausted attack transition")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL COMPUTER NEARBY: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
