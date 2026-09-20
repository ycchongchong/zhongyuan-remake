extends SceneTree
var failures=0
var scene:Control
var path="user://native-tactical-ai-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var snapshot:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"computer tactical step reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize computer tactics")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/computer-retreat-start.json").is_empty(),"load legal computer turn after human end-turn")
	scene.refresh()
	scene.show_battle_details()
	check(scene.computer_next.visible and not scene.computer_next.disabled and not scene.tactical_end.visible,"computer gets its own progression control")
	var count:int=0
	for i in range(12):
		var points:int=int(scene.state.battle.tactics.points)
		scene.computer_next.emit_signal("pressed")
		reload_step()
		check(scene.state.battle.tactics.has("retreat"),"computer creates retreat result")
		if not scene.state.battle.tactics.has("retreat"):break
		check(scene.retreat_finish.visible and not scene.retreat_confirm.visible and not scene.computer_next.visible and scene.battle_description.text.contains("电脑守军"),"computer withdraws without human confirmation or fake orders")
		check(int(scene.state.battle.tactics.points)==points-1 and scene.state.battle.tactics.retreat.outcomes[0].returned,"withdrawal costs one mobility and relocates the officer")
		check(not scene.native.advance_computer_tactics().is_empty() and not scene.native.cancel_tactical_retreat().is_empty(),"computer result cannot be cancelled or rolled again")
		count+=1
		await process_frame
		if i==0 and DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://computer-retreat-preview.png")
		scene.retreat_finish.emit_signal("pressed")
		reload_step()
		if scene.state.battle.tactics.turn_boundary=="battle_result":break
	check(count==4 and int(scene.state.battle.tactics.turn_reason)==2 and scene.withdrawal_next.visible,"all defenders withdraw into original battle result")
	for i in range(42):
		scene.withdrawal_next.emit_signal("pressed")
		reload_step()
		if int(scene.state.battle.tactics.settlement.stage)==13:break
	scene.withdrawal_next.emit_signal("pressed")
	check(scene.state.battle==null and scene.state.phase=="player_commands" and not scene.battle_details.visible,"computer retreat completes occupation and returns to strategy")
	reload_step()
	# Unmodified legacy deployment reaches the explicit next-unit scan boundary.
	check(scene.native.load_session("res://tests/computer-plan-start.json").is_empty() and scene.native.begin_tactics().is_empty() and scene.native.end_tactical_turn().is_empty(),"normal army reaches computer evaluation")
	scene.refresh()
	scene.show_battle_details()
	scene.computer_next.emit_signal("pressed")
	reload_step()
	scene.computer_next.emit_signal("pressed")
	reload_step()
	check(not scene.computer_next.disabled,"selected unit can evaluate strategies")
	scene.computer_next.emit_signal("pressed")
	reload_step()
	check(not scene.computer_next.disabled and scene.state.battle.tactics.computer_strategy.kind=="no_strategy","no-strategy evaluation can continue to motion")
	scene.computer_next.emit_signal("pressed")
	reload_step()
	scene.computer_next.emit_signal("pressed")
	reload_step()
	check(scene.computer_next.visible and not scene.computer_next.disabled and not scene.tactical_end.visible and scene.state.battle.tactics.computer_motion.kind=="scan" and scene.battle_description.text.contains("下一部队"),"next-unit scan is actionable")
	var held:Dictionary=scene.native.session_snapshot()
	check(not scene.native.advance_computer_tactics().is_empty() and scene.native.session_snapshot()==held,"held planning cannot consume more randomness")
	await process_frame
	if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://computer-planning-boundary-preview.png")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL TACTICAL AI: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
