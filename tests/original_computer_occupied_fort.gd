extends SceneTree
var failures=0
var scene:Control
var path="user://native-computer-occupied-fort-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"occupied-fort boundary reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize occupied-fort continuation")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	for kind in ["move","attack","strategy"]:
		check(scene.native.load_session("res://tests/computer-occupied-fort-%s.json" % kind).is_empty(),"load legal capture-and-occupation history for "+kind)
		scene.refresh()
		scene.show_battle_details()
		check(scene.computer_next.visible and not scene.computer_next.disabled and scene.computer_next.text=="应对敌军占堡","previous occupied-fort stop offers continuation")
		var before:Dictionary=scene.native.session_snapshot()
		scene.computer_next.emit_signal("pressed")
		var t:Dictionary=scene.state.battle.tactics
		check(t.get("computer_motion",t.get("computer_strategy",{})).get("kind","")==kind,"occupied-fort decision executes "+kind)
		reload_step()
		var after:Dictionary=scene.native.session_snapshot()
		check(not scene.native.continue_computer_occupied_fort().is_empty() and scene.native.session_snapshot()==after,"cannot repeat occupied-fort action")
		if kind=="attack":
			check(not scene.computer_next.visible and t.attack.stage=="clash_ready" and int(t.points)==int(before.battle.tactics.points)-3,"occupied-fort attack exposes actual clash")
			scene.clash_start.emit_signal("pressed")
			check(scene.state.battle.tactics.attack.stage=="clash_orders","enter occupied-fort clash")
			reload_step()
			var clash:Dictionary=scene.native.session_snapshot().battle.tactics.attack.clash
			var human:int=0 if int(clash.players[0])!=0 else 1
			for i in range(3):scene.clash_orders[human*4+3].emit_signal("pressed")
			scene.surrender_submit.emit_signal("pressed")
			scene.surrender_yes.emit_signal("pressed")
			scene.surrender_next.emit_signal("pressed")
			scene.surrender_next.emit_signal("pressed")
			check(scene.state.battle.tactics.attack.stage=="clash_result","resolve occupied-fort clash through settlement")
			scene.clash_result_close.emit_signal("pressed")
			reload_step()
		elif kind=="strategy":
			check(scene.computer_next.text=="执行电脑计策","occupied-fort strategy uses existing execution controls")
			scene.computer_next.emit_signal("pressed")
			check(scene.state.battle.tactics.has("strategy_result") and scene.computer_next.text=="确认计策结果","execute occupied-fort strategy")
			reload_step()
			scene.computer_next.emit_signal("pressed")
			check(scene.state.battle.tactics.has("last_strategy") and not scene.state.battle.tactics.has("strategy_result"),"finish occupied-fort strategy")
			reload_step()
		else:check(int(t.points)<int(before.battle.tactics.points) and scene.state.sram!=before.sram,"occupied-fort movement changes board, cache and budget")
		scene.computer_next.emit_signal("pressed")
		scene.computer_next.emit_signal("pressed")
		check(scene.state.battle.tactics.has("computer_plan"),"resume next computer selection")
		reload_step()
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL COMPUTER OCCUPIED FORT: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
