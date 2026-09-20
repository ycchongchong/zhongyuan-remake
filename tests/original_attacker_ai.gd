extends SceneTree
var failures=0
var scene:Control
var path="user://native-attacker-ai-test.json"
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func reload_step():
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"invading AI state reloads exactly")
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize invading AI interface")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	for fixture in ["start","far","strategy"]:
		check(scene.native.load_session("res://tests/attacker-invasion-%s.json" % fixture).is_empty(),"load completed human defense")
		scene.refresh()
		scene.show_battle_details()
		check(scene.tactical_start.is_visible_in_tree() and scene.tactical_start.text=="进入电脑先攻","defender deployment offers actual computer-first entry")
		scene.tactical_start.emit_signal("pressed")
		check(int(scene.state.battle.tactics.side)==128 and int(scene.state.battle.tactics.round)==1 and int(scene.state.battle.tactics.points)==24,"first invading turn uses original side,round and mobility")
		check(scene.computer_next.is_visible_in_tree() and not scene.tactical_end.visible and not scene.human_strategy_controls.visible,"computer turn locks human tactical actions")
		reload_step()
		var complete:bool=false
		var effects:int=0
		var phases:Dictionary={}
		for i in range(120):
			var before:Dictionary=scene.native.session_snapshot()
			var stage:String=before.battle.tactics.get("attacker_ai",{}).get("stage","assess")
			scene.computer_next.emit_signal("pressed")
			if before==scene.native.session_snapshot():
				check(false,"computer stage must advance: "+stage+" "+scene.status.text)
				break
			var tactics:Dictionary=scene.state.battle.tactics
			if not phases.has(stage):
				phases[stage]=true
				reload_step()
			if stage=="strategy_execute":
				effects+=1
				check(tactics.attacker_ai.stage=="strategy_result" and scene.computer_next.text=="确认进攻军计策结果","invading strategy shows one-time charge and acknowledgement")
				check(int(tactics.points)==int(before.battle.tactics.points)-int(tactics.attacker_ai.result.cost),"invading strategy charges original mobility")
			if stage=="strategy_result":check(scene.state.sram==before.sram and int(scene.state.random_cursor)==int(before.random_cursor),"strategy acknowledgement preserves resolved effect")
			if tactics.has("attack"):
				check(fixture!="far" and scene.clash_start.is_visible_in_tree() and not scene.computer_next.visible,"computer attack exposes reachable clash button")
				scene.clash_start.emit_signal("pressed")
				check(scene.state.battle.tactics.attack.stage=="clash_orders" and scene.clash_controls.is_visible_in_tree(),"invading attack enters combat controls")
				var players:Array=scene.state.battle.tactics.attack.clash.players
				var human:int=0 if int(players[0])!=0 else 1
				check(not scene.clash_orders[human*4+3].disabled and scene.clash_orders[(human^1)*4+3].disabled,"only human defender combat orders are editable")
				reload_step()
				complete=true
				break
			if not tactics.has("turn_boundary"):
				check(int(tactics.side)==0 and int(tactics.points)==15 and scene.tactical_end.is_visible_in_tree() and scene.human_strategy_controls.visible,"completed invading turn returns usable defender controls")
				check(scene.retreat_button.visible,"human defender can now request strategic withdrawal")
				var moved:bool=false
				for direction in range(4):
					if scene.native.move_tactical(int(tactics.selected),direction).is_empty():moved=true;break
				check(moved,"human defender can move after AI completion")
				scene.refresh()
				scene.show_battle_details()
				reload_step()
				scene.tactical_end.emit_signal("pressed")
				check(int(scene.state.battle.tactics.side)==128 and int(scene.state.battle.tactics.round)==2 and scene.computer_next.is_visible_in_tree(),"defender can hand back to next invading round")
				reload_step()
				complete=true
				break
		check(complete,"invasion reaches combat or playable defense")
		if fixture=="strategy":check(effects>0,"controlled strategy fixture exercises actual effect")
	scene.battle_details.hide()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("ORIGINAL ATTACKER AI: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
