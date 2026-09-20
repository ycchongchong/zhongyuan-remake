extends SceneTree
var failures=0
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func _init():call_deferred("run")
func run():
	var scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,0).is_empty(),"initialize natural campaign failure interface")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.load_session("res://tests/natural-campaign-defeat.json").is_empty(),"load untouched-start2245-action campaign result")
	scene.refresh()
	scene.show_battle_details()
	check(scene.state.year==207 and scene.state.month==9,"natural campaign date retained")
	check(scene.human_failure_controls.is_visible_in_tree() and scene.human_failure_next.disabled,"last human defeat reaches terminal report")
	check(not scene.computer_next.visible and not scene.clash_step.visible and not scene.clash_result_close.visible,"completed defeat cannot expose battle continuation")
	var before:Dictionary=scene.native.session_snapshot()
	check(not scene.native.finish_human_failure().is_empty() and scene.native.session_snapshot()==before,"single-player terminal report cannot resume defeated faction")
	var path="user://natural-defeat-test.json"
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty() and scene.native.session_snapshot()==before,"natural terminal report round trips exactly")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	scene.queue_free()
	await process_frame
	print("ORIGINAL CAMPAIGN DEFEAT: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
