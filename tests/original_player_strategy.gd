extends SceneTree
var failures=0
var scene:Control
var path="user://native-player-strategy-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"player strategy reloads exactly")
func choose(option:OptionButton,id:int):
	for i in range(option.item_count):
		if option.get_item_id(i)==id:option.select(i);return
	check(false,"requested option exists")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize player strategy UI")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	for fixture in ["single","start"]:
		check(scene.native.load_session("res://tests/tactical-turn-%s.json" % fixture).is_empty(),"load canonical tactical deployment")
		if fixture=="start":check(scene.native.end_tactical_turn().is_empty(),"defending human receives control")
		scene.refresh()
		scene.show_battle_details()
		check(scene.human_strategy_controls.visible and scene.human_strategy_prepare.visible,"human strategy controls available")
		var side:int=int(scene.state.battle.tactics.get("side",128))
		var target:int=-1
		for slot in range(12 if side==128 else 11):
			var base:int=0xdaa+slot*2 if side==128 else 0xdc2+slot*3
			if int(scene.state.sram[base])>=241:continue
			if int(scene.state.sram[0xf1a+int(scene.state.sram[base+1])]) in [4,5]:target=slot;break
		check(target>=0,"canonical enemy terrain supports fire strategy")
		choose(scene.human_strategy_choices,0)
		choose(scene.human_strategy_target,target)
		var before:Dictionary=scene.native.session_snapshot()
		scene.human_strategy_prepare.emit_signal("pressed")
		var pending:Dictionary=scene.native.session_snapshot()
		check(pending.battle.tactics.human_strategy.stage=="confirm" and pending.sram==before.sram and int(pending.battle.tactics.points)==int(before.battle.tactics.points),"proposal is free until confirmation")
		check(scene.human_strategy_confirm.visible and scene.human_strategy_cancel.visible and not scene.tactical_end.visible,"confirmation locks other tactical controls")
		reload_step()
		scene.human_strategy_cancel.emit_signal("pressed")
		check(scene.state.sram==before.sram and int(scene.state.random_cursor)==int(before.random_cursor) and not scene.state.battle.tactics.has("human_strategy"),"cancel returns without cost or random draw")
		reload_step()
		scene.human_strategy_prepare.emit_signal("pressed")
		scene.human_strategy_confirm.emit_signal("pressed")
		var applied:Dictionary=scene.native.session_snapshot()
		check(applied.battle.tactics.human_strategy.stage=="result" and int(applied.battle.tactics.points)==int(before.battle.tactics.points)-4,"explicit confirmation charges original cost exactly once")
		check(scene.human_strategy_finish.visible and not scene.human_strategy_cancel.visible and not scene.human_strategy_prepare.visible,"result offers acknowledgement without reroll")
		check(not scene.native.confirm_tactical_strategy().is_empty() and scene.native.session_snapshot()==applied,"duplicate confirmation leaves state untouched")
		reload_step()
		scene.human_strategy_finish.emit_signal("pressed")
		check(not scene.state.battle.tactics.has("human_strategy") and scene.human_strategy_prepare.visible and scene.tactical_end.visible,"acknowledgement restores human actions")
		reload_step()
	check(scene.native.load_session("res://tests/tactical-turn-single.json").is_empty(),"reload attacker menu")
	check(scene.native.select_tactical_unit(1).is_empty(),"select lower-intelligence officer")
	scene.refresh()
	scene.show_battle_details()
	check(scene.human_strategy_choices.item_count==4,"menu follows intelligence tier")
	var unchanged:Dictionary=scene.native.session_snapshot()
	check(not scene.native.prepare_tactical_strategy(1,2,7).is_empty() and scene.native.session_snapshot()==unchanged,"unavailable strategy cannot bypass native validation")
	check(scene.native.end_tactical_turn().is_empty(),"pass to computer")
	scene.refresh()
	scene.show_battle_details()
	check(not scene.human_strategy_controls.visible,"human strategy controls hidden during computer turn")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL PLAYER STRATEGY: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
