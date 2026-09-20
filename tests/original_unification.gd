extends SceneTree
var failures=0
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func _init():call_deferred("run")
func run():
	var scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,0,false,2).is_empty(),"initialize unification UI")
	root.add_child(scene);current_scene=scene;scene.set_process(false)
	await process_frame
	await create_timer(0.05).timeout
	var sound=root.get_node("OriginalSound")
	var assets:Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://assets/original/endings/manifest.json"))
	for variant in [0,2,1]:
		check(scene.native.load_session("res://tests/unification-ending-%d.json" % variant).is_empty(),"load final-city outcome "+str(variant))
		scene.ending_seen=false;scene.refresh()
		await process_frame
		await create_timer(0.05).timeout
		var before:Dictionary=scene.native.session_snapshot()
		check(before.phase=="ending" and before.ending.kind=="unification" and int(before.unification.variant)==variant,"native score selects ending variant "+str(variant))
		check(scene.ending_report.visible and scene.ending_picture.visible and scene.ending_button.visible,"unification report and original painting are visible")
		check(scene.ending_report.dialog_text.contains("统治度：%d" % int(before.unification.score)) and scene.ending_text.text==scene.ending_report.dialog_text,"report displays native score and consistent text")
		var picture:Image=scene.ending_picture.texture.get_image()
		picture.convert(Image.FORMAT_RGB8)
		var hash=HashingContext.new();hash.start(HashingContext.HASH_SHA256);hash.update(picture.get_data())
		check(picture.get_size()==Vector2i(144,96) and hash.finish().hex_encode()==assets.paintings[str(variant)].rgb_sha256,"all painting pixels match original capture")
		check(sound.current_cue=="unification_"+str(variant) and sound.music.playing,"matching original score plays")
		var starts:int=sound.music_starts
		scene.ending_report.hide();scene.refresh();scene.ending_button.pressed.emit();scene.refresh()
		check(sound.music_starts==starts,"reopening report preserves musical progression")
		scene._process(1.0)
		check(scene.native.session_snapshot()==before,"viewing ending leaves native state and clock unchanged")
		check(scene.end_button.disabled and scene.expedition_button.disabled and scene.orders_button.disabled,"unification locks further gameplay commands")
		var save_path="user://unification-ui-test.json"
		check(scene.native.save_session(save_path).is_empty() and scene.native.load_session(save_path).is_empty() and scene.native.session_snapshot()==before,"unification save/load preserves outcome and score")
		DirAccess.remove_absolute(ProjectSettings.globalize_path(save_path))
	check(scene.native.load_session("res://tests/natural-dual-year-limit.json").is_empty(),"load year-limit outcome after unification")
	scene.ending_seen=false;scene.refresh()
	await process_frame
	await create_timer(0.05).timeout
	check(not scene.ending_picture.visible and scene.ending_picture.texture==null and not scene.ending_report.dialog_text.contains("统治度"),"year limit clears stale unification painting and score")
	check(sound.current_cue=="" and not sound.music.playing,"year limit does not reuse unification music")
	scene.ending_report.custom_action.emit(&"title")
	await scene_changed
	await process_frame
	await create_timer(0.05).timeout
	check(current_scene.scene_file_path=="res://opening.tscn" and sound.current_cue=="title","ending returns to playable title with title music")
	await sound.shutdown()
	print("ORIGINAL UNIFICATION: %d failures" % failures)
	quit(1 if failures else 0)
