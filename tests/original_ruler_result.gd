extends SceneTree
var failures=0
var scene:Control
var path="user://native-ruler-result-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var snapshot:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==snapshot,"ruler result step reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize ruler result")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/clash-npc-ruler-defeat.json").is_empty(),"load NPC ruler defeat from actual action history")
	scene.refresh()
	scene.show_battle_details()
	for i in range(2):scene.clash_defeat_next.emit_signal("pressed")
	check(scene.state.battle.ruler_defeat_result_available and not scene.clash_result_close.disabled and scene.clash_result_close.text.contains("君主败北"),"NPC ruler defeat exposes separate result entry")
	reload_step()
	scene.clash_result_close.emit_signal("pressed")
	check(int(scene.state.battle.tactics.turn_reason)==131 and scene.withdrawal_next.visible and not scene.tactical_end.visible,"ruler result begins with original battle settlement")
	for i in range(42):
		scene.withdrawal_next.emit_signal("pressed")
		reload_step()
		if int(scene.state.battle.tactics.settlement.stage)==29:break
	check(int(scene.state.battle.tactics.settlement.stage)==29 and scene.withdrawal_next.text.contains("核对") and not scene.native.finish_withdrawal_result().is_empty(),"battle report requires defeated ruler territory processing")
	var target_resources:Array=scene.state.sram.slice(12*36,12*36+16)
	for city in range(30):
		scene.withdrawal_next.emit_signal("pressed")
		reload_step()
		check(int(scene.state.battle.tactics.annexation.next_city)==city+1,"annexation processes next city exactly once")
		if city==14:
			await process_frame
			if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://ruler-annexation-progress-preview.png")
	check(scene.withdrawal_next.text.contains("返回战略地图") and scene.battle_description.text.contains("已全部处理"),"all thirty cities reach final annexation report")
	check(scene.state.sram.slice(12*36,12*36+16)==target_resources,"battle target resources are not reduced twice")
	var no_loser_city:bool=true
	for city in range(30):no_loser_city=no_loser_city and (int(scene.state.sram[city*36])&7)!=2
	check(no_loser_city and scene.state.battle.tactics.annexation.outcomes.size()>0,"all defeated ruler cities transfer to winner")
	await process_frame
	if DisplayServer.get_name()!="headless":root.get_texture().get_image().save_png("res://ruler-annexation-result-preview.png")
	scene.withdrawal_next.emit_signal("pressed")
	check(scene.state.battle==null and scene.state.phase=="player_commands" and not scene.battle_details.visible,"NPC ruler defeat returns to strategic commands")
	reload_step()
	check(scene.native.load_session("res://tests/clash-defeat-3.json").is_empty(),"load separate human ruler defeat")
	scene.refresh()
	scene.show_battle_details()
	for i in range(2):scene.clash_defeat_next.emit_signal("pressed")
	check(not scene.state.battle.ruler_defeat_result_available and scene.state.battle.human_failure_available and not scene.clash_result_close.disabled and not scene.native.begin_ruler_defeat_result().is_empty(),"human ruler failure cannot enter NPC territory transfer")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL RULER RESULT: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
