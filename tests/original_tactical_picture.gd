extends SceneTree
var failures=0
var scene:Control
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func _init():call_deferred("run")
func run():
	scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,2).is_empty(),"initialize native tactical picture")
	root.add_child(scene)
	scene.set_process(false)
	await process_frame
	check(scene.native.tactical_image()==null,"strategic map has no stale tactical picture")
	check(scene.native.load_session("res://tests/computer-plan-start.json").is_empty(),"load deployed battle with valid command history")
	var before:Dictionary=scene.native.session_snapshot()
	var board:Image=scene.native.tactical_image()
	check(board!=null and board.get_size()==Vector2i(256,160),"C++ decodes original tactical viewport")
	check(scene.native.session_snapshot()==before,"picture decoding cannot change battle state or RNG")
	var terrain:Image=scene.native.battlefield_image(int(before.battle.target))
	terrain.convert(Image.FORMAT_RGB8)
	check(board.get_data()!=terrain.get_data(),"native board contains troop glyphs beyond terrain")
	scene.refresh()
	scene.show_battle_details()
	check(scene.battle_texture.texture.get_image().get_data()==board.get_data(),"battle window displays the native troop image")
	if DisplayServer.get_name()!="headless":
		await process_frame
		await process_frame
		root.get_texture().get_image().save_png("res://tactical-picture-window.png")
	check(scene.native.begin_tactics().is_empty(),"troop rendering preserves tactical entry")
	var error:String="no movement"
	for direction in range(4):
		error=scene.native.move_tactical(0,direction)
		if error.is_empty():break
	check(error.is_empty(),"legal tactical movement remains available")
	var moved:Image=scene.native.tactical_image()
	check(moved.get_data()!=board.get_data(),"movement updates original unit glyph position")
	var path="user://tactical-picture-test.json"
	check(scene.native.save_session(path).is_empty() and scene.native.load_session(path).is_empty(),"visual state saves and reloads through strict native history")
	check(scene.native.tactical_image().get_data()==moved.get_data(),"restored battle has identical pixels")
	check(scene.native.begin_tactical_retreat(2).is_empty() and scene.native.confirm_tactical_retreat().is_empty() and scene.native.finish_tactical_retreat().is_empty(),"commander withdrawal still returns to campaign")
	check(scene.native.tactical_image()==null,"battle completion releases the tactical picture")
	check(scene.native.load_session("res://tests/long-clash-result.json").is_empty() and scene.native.finish_clash_result().is_empty(),"restore long clash defeat and return to tactical map")
	scene.refresh()
	scene.show_battle_details()
	check(scene.battle_texture.texture.get_image().get_data()==scene.native.tactical_image().get_data(),"post-defeat map uses updated survivor glyphs")
	scene.native.tactical_image().save_png("user://tactical-picture-preview.png")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	scene.queue_free()
	await process_frame
	print("ORIGINAL TACTICAL PICTURE: %d failures" % failures)
	await root.get_node("OriginalSound").shutdown()
	quit(1 if failures else 0)
