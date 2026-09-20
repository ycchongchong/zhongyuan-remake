extends SceneTree
var failures=0
var scene:Control
var path="user://native-defender-retreat-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var snapshot:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"withdrawal step reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize defender withdrawal UI")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/tactical-turn-start.json").is_empty(),"load two-player entry")
	scene.refresh()
	scene.deployment_action("end_tactical_turn")
	check(scene.retreat_button.visible and not scene.retreat_button.disabled,"defender has an enabled retreat command")
	scene.battle_defenders.emit_signal("item_selected",1)
	var points:int=scene.state.battle.tactics.points
	scene.retreat_button.emit_signal("pressed")
	check(scene.retreat_confirm.visible and scene.retreat_cancel.visible and not scene.tactical_end.visible and scene.battle_description.text.contains("驻将最少"),"confirmation describes original destination rule and holds commands")
	reload_step()
	scene.retreat_cancel.emit_signal("pressed")
	check(not scene.state.battle.tactics.has("retreat") and int(scene.state.battle.tactics.points)==points,"cancel does not consume mobility")
	reload_step()
	# Retire ordinary officer, ruler, then the other two; roster row indexes shrink.
	for slot in [1,0,2,3]:
		for row in range(scene.battle_defenders.item_count):
			if int(scene.battle_defenders.get_item_metadata(row))==slot:
				scene.battle_defenders.emit_signal("item_selected",row)
				break
		check(int(scene.state.battle.tactics.selected)==slot,"roster holes still select the actual defender slot")
		points=int(scene.state.battle.tactics.points)
		scene.retreat_button.emit_signal("pressed")
		scene.retreat_confirm.emit_signal("pressed")
		check(int(scene.state.battle.tactics.points)==points-1 and int(scene.state.sram[0xdaa+slot*2])==255 and scene.retreat_finish.visible,"successful defender withdrawal removes one unit and costs one point")
		check(scene.battle_description.text.contains("守军已撤往"),"result names the destination city")
		reload_step()
		if slot==1:
			await process_frame
			if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://defender-retreat-preview.png")
		scene.retreat_finish.emit_signal("pressed")
		reload_step()
	check(scene.state.battle.tactics.get("turn_boundary","")=="battle_result" and int(scene.state.battle.tactics.turn_reason)==2 and not scene.tactical_end.visible and not scene.attack_controls.visible,"last defender retains a saveable final-result boundary")
	check(int(scene.state.sram[0xdc2])==145 and (int(scene.state.sram[12*36])&7)==2,"pending settlement preserves attackers and city ownership")
	check(scene.native.load_session("res://tests/tactical-defender-retreat-full.json").is_empty(),"load controlled full friendly-city rosters")
	scene.refresh()
	scene.deployment_action("end_tactical_turn")
	points=int(scene.state.battle.tactics.points)
	var before:Array=scene.state.sram.duplicate()
	scene.battle_defenders.emit_signal("item_selected",1)
	scene.retreat_button.emit_signal("pressed")
	scene.retreat_confirm.emit_signal("pressed")
	check(int(scene.state.battle.tactics.points)==points and scene.state.sram==before and scene.battle_description.text.contains("机动力未扣除"),"full cities refuse retreat without charging or changing SRAM")
	reload_step()
	scene.retreat_finish.emit_signal("pressed")
	check(scene.tactical_end.visible and scene.retreat_button.visible,"failed withdrawal returns to defending actions")
	reload_step()
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL DEFENDER RETREAT: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
