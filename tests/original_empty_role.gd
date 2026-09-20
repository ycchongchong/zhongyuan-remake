extends SceneTree
var failures=0
var scene:Control
var path="user://native-empty-role-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func replay():
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"empty-slot continuation saves and reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize empty-slot continuation interface")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/computer-empty-role.json").is_empty(),"load real two-retreat and role-adjustment history")
	scene.refresh()
	scene.show_battle_details()
	check(int(scene.state.sram[0xdaa])==255 and int(scene.state.battle.tactics.computer.role_slot)==5,"first defender is absent after actual retreat")
	check(scene.computer_next.is_visible_in_tree() and not scene.computer_next.disabled and scene.computer_next.text=="继续角色调整后的行动","empty first slot no longer disables computer continuation")
	check(scene.battle_description.text.contains("第 1 队已离场") and not scene.battle_description.text.contains("尝试移动第 1 队"),"report accurately describes departed unit")
	var before:Dictionary=scene.native.session_snapshot()
	check(not scene.native.continue_computer_role().is_empty() and scene.native.session_snapshot()==before,"old API retains exact legacy boundary")
	scene.computer_next.emit_signal("pressed")
	check(scene.state.battle.tactics.computer.kind=="acted" and scene.state.battle.tactics.computer_motion.branch=="empty_role_return","button resumes through dedicated empty-slot event")
	check(scene.state.sram==before.sram and scene.state.random_cursor==before.random_cursor and scene.state.battle.tactics.points==before.battle.tactics.points,"blocked empty-slot action changes neither armies nor resources nor randomness")
	check(int(scene.state.battle.tactics.computer_motion.direction)==-1 and scene.state.battle.tactics.computer_motion.kind=="blocked","unanimous blocked directions do not invent a previous command")
	replay()
	before=scene.native.session_snapshot()
	check(not scene.native.continue_empty_computer_role().is_empty() and not scene.native.can_continue_computer_role() and scene.native.session_snapshot()==before,"duplicate continuation is atomic")
	for i in range(2):scene.computer_next.emit_signal("pressed")
	check(scene.state.battle.tactics.has("computer_plan") and int(scene.state.battle.tactics.computer_plan.slot)!=0,"subsequent assessment plans a surviving defender")
	check(scene.computer_next.is_visible_in_tree() and not scene.computer_next.disabled,"remaining computer command remains reachable")
	replay()
	check(scene.native.load_session("res://tests/computer-role-unknown.json").is_empty(),"load legacy unknown-direction normal troop case")
	scene.refresh()
	scene.show_battle_details()
	before=scene.native.session_snapshot()
	check(scene.computer_next.disabled and not scene.native.continue_empty_computer_role().is_empty() and scene.native.session_snapshot()==before,"empty-slot API cannot bypass unknown direction on a living troop")
	scene.queue_free()
	await process_frame
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL EMPTY ROLE: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
