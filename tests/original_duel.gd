extends SceneTree
var failures=0
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func _init():call_deferred("run")
func run():
	var scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize original duel UI")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	var path="user://native-duel-test.json"
	for variant in [0,3,4]:
		check(scene.native.load_session("res://tests/clash-duel-%d.json" % variant).is_empty(),"load replayable duel entry %d" % variant)
		scene.refresh()
		scene.show_battle_details()
		check(scene.duel_next.visible and scene.duel_next.text=="进入单挑" and not scene.clash_step.visible,"original duel boundary has explicit entry")
		scene.duel_next.emit_signal("pressed")
		var initial:Dictionary=scene.native.session_snapshot()
		check(scene.state.battle.tactics.attack.stage=="duel_orders" and scene.duel_controls.visible and scene.battle_description.text.contains("单挑"),"duel orders display through native binding")
		var exchanges:int=0
		for step in range(80):
			var attack:Dictionary=scene.state.battle.tactics.attack
			if attack.stage in ["clash_result","clash_orders"]:break
			if attack.stage=="duel_orders":
				var side:int=int(attack.duel.side)
				var human:bool=int(attack.clash.players[1 if side else 0])!=0
				if human:
					var who:int=int(attack.clash.second if side else attack.clash.first)
					if who<6:check(not scene.duel_commands[4].visible,"ruler surrender is unavailable")
					scene.duel_commands[exchanges%3].emit_signal("pressed")
					exchanges+=1
				else:
					check(scene.duel_next.visible and scene.duel_commands[0].disabled,"computer duelist chooses through native AI")
					scene.duel_next.emit_signal("pressed")
			elif attack.stage=="duel_surrender_confirm":scene.duel_yes.emit_signal("pressed")
			else:scene.duel_next.emit_signal("pressed")
			var snapshot:Dictionary=scene.native.session_snapshot()
			check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"duel stage saves and reloads exactly")
			if variant==0 and step==0:
				await process_frame
				if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://duel-exchange-preview.png")
		check(scene.state.battle.tactics.attack.stage in ["clash_result","clash_orders"],"duel reaches a verified result or retreat")
		if variant==0:
			check(scene.native.load_session("res://tests/clash-duel-0.json").is_empty(),"reset duel for withdrawal")
			scene.refresh()
			scene.duel_next.emit_signal("pressed")
			scene.duel_commands[3].emit_signal("pressed")
			scene.duel_next.emit_signal("pressed")
			check(scene.state.battle.tactics.attack.stage=="clash_orders" and int(scene.state.battle.tactics.attack.clash.runtime.orders[3])==1 and not scene.duel_controls.visible,"duel retreat preserves general and restores original army retreat order")
			check(scene.native.load_session("res://tests/clash-duel-0.json").is_empty(),"reset duel for surrender")
			scene.refresh()
			scene.duel_next.emit_signal("pressed")
			scene.duel_commands[4].emit_signal("pressed")
			check(scene.duel_yes.visible and scene.duel_no.visible and not scene.duel_next.visible,"human duel surrender asks for confirmation")
			scene.duel_no.emit_signal("pressed")
			check(scene.state.battle.tactics.attack.stage=="duel_orders","cancel returns to same duelist")
			scene.duel_commands[4].emit_signal("pressed")
			scene.duel_yes.emit_signal("pressed")
			for i in range(2):scene.duel_next.emit_signal("pressed")
			check(scene.state.battle.tactics.attack.result.outcome=="surrender" and scene.battle_description.text.contains("俘虏") and not scene.duel_controls.visible,"duel surrender shows original captive result")
			await process_frame
			if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://duel-surrender-preview.png")
			scene.clash_result_close.emit_signal("pressed")
			check(not scene.state.battle.tactics.has("attack"),"remaining army resumes tactics after duel surrender")
		scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL DUEL: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
