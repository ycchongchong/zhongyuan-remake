extends SceneTree
var failures=0
var scene:Control
var path="user://native-computer-fort-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var snapshot:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"computer fort step reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize computer fort movement")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/computer-fort-start.json").is_empty(),"load real ruler withdrawal and assessment history")
	scene.refresh()
	scene.show_battle_details()
	check(scene.computer_next.visible and not scene.computer_next.disabled and scene.computer_next.text.contains("补位") and not scene.tactical_end.visible,"computer can continue assessment into fort planning")
	scene.computer_next.emit_signal("pressed")
	reload_step()
	var plan:Dictionary=scene.state.battle.tactics.computer_plan
	check(plan.kind=="move" and int(plan.slot)==3 and int(plan.destination)==84 and int(scene.state.sram[0xdaa+3*2+1])==84,"original descending scan moves fourth defending unit into fort")
	check(int(scene.state.battle.tactics.points)==19 and int(scene.state.sram[0xdec+6])==15 and scene.battle_description.text.contains("补入空置城堡"),"movement charges four points and records fort role")
	check(not scene.computer_next.disabled and not scene.native.plan_computer_tactics().is_empty(),"completed move permits fresh assessment, not duplicate movement")
	await process_frame
	if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://computer-fort-move-preview.png")
	scene.computer_next.emit_signal("pressed")
	reload_step()
	scene.computer_next.emit_signal("pressed")
	reload_step()
	check(scene.state.battle.tactics.computer_plan.kind=="role_plan" and int(scene.state.battle.tactics.computer_plan.slot)==2 and not scene.computer_next.disabled,"next defender reaches strategy evaluation with remembered cursor")
	scene.computer_next.emit_signal("pressed")
	reload_step()
	check(not scene.computer_next.disabled and scene.state.battle.tactics.computer_strategy.kind=="no_strategy","no-strategy decision permits motion continuation")
	var held:Dictionary=scene.native.session_snapshot()
	check(not scene.native.plan_computer_tactics().is_empty() and not scene.native.advance_computer_tactics().is_empty() and scene.native.session_snapshot()==held,"held role planning cannot reroll or skip turn")
	await process_frame
	if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://computer-fort-next-unit-preview.png")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL COMPUTER FORT: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
