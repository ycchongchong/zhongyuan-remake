extends SceneTree
var failures=0
var checks=0
func check(ok:bool,message:String):
	checks+=1
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func _init():call_deferred("run")
func run():
	check(OS.get_name()=="Windows","Windows executable runtime")
	check(ClassDB.class_exists("ZhongyuanOriginalData"),"Windows C++ extension registered")
	var manifest:Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://reference/manifest.json"))
	check(manifest.local_path=="res://reference/original.nes","portable ROM path")
	var hash=HashingContext.new();hash.start(HashingContext.HASH_SHA256);hash.update(FileAccess.get_file_as_bytes(manifest.local_path))
	check(hash.finish().hex_encode()==manifest.sha256,"packaged ROM fingerprint")
	var sound=root.get_node("OriginalSound")
	for cue in sound.manifest.tracks:
		var stream=sound.stream_for(cue)
		check(stream!=null and stream.get_length()>0,"packaged audio "+cue)
	var opening=load("res://opening.tscn").instantiate()
	root.add_child(opening);current_scene=opening
	await process_frame
	check(opening.picture!=null and opening.theme.default_font!=null,"title texture and Chinese font")
	for v in [0,1,2]:
		var picture=Image.load_from_file("res://assets/original/endings/%d.png"%v)
		check(picture!=null and picture.get_size()==Vector2i(144,96),"raw ending painting "+str(v))
	var scene=load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4,0).is_empty(),"start campaign using embedded ROM")
	opening.queue_free();root.add_child(scene);current_scene=scene;scene.set_process(false)
	await process_frame
	check(scene.state.cities.size()==30 and scene.state.officers.size()==241,"30 cities and 241 officers")
	var file="user://Windows打包验收存档.json"
	var before:Dictionary=scene.native.session_snapshot()
	check(scene.native.save_session(file).is_empty(),"save with Chinese filename")
	scene.native.start_session(0,2)
	check(scene.native.load_session(file).is_empty() and scene.native.session_snapshot()==before,"load exact campaign state")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(file))
	var args=OS.get_cmdline_user_args()
	var fixture_dir=""
	for i in range(args.size()-1):
		if args[i]=="--fixtures":fixture_dir=args[i+1]
	if not fixture_dir.is_empty():
		for variant in [0,2,1]:
			check(scene.native.load_session(fixture_dir.path_join("unification-ending-%d.json"%variant)).is_empty(),"load controlled ending "+str(variant))
			scene.ending_seen=false;scene.refresh()
			await process_frame
			await create_timer(0.1).timeout
			check(scene.ending_picture.texture!=null and sound.current_cue=="unification_"+str(variant),"ending picture and music "+str(variant))
		check(scene.native.load_session(fixture_dir.path_join("natural-role-handover-next.json")).is_empty(),"load long battle history on Windows")
		check(scene.native.resume_computer_role_after_handover().is_empty(),"Windows battle handover continuation")
	for i in range(args.size()-1):
		if args[i]=="--asset-hashes":
			var expected:Dictionary=JSON.parse_string(FileAccess.get_file_as_string(args[i+1]))
			var matched=0
			for path in expected:
				var digest=HashingContext.new();digest.start(HashingContext.HASH_SHA256)
				digest.update(FileAccess.get_file_as_bytes(path))
				if digest.finish().hex_encode()==expected[path]:matched+=1
				else:printerr("Asset mismatch: "+path)
			check(matched==expected.size(),"all %d raw audio/image files match source bytes"%expected.size())
	if "--screenshot" in args:
		scene.ending_report.hide();scene.refresh()
		await process_frame
		await RenderingServer.frame_post_draw
		root.get_texture().get_image().save_png("user://windows-package-preview.png")
	await sound.shutdown()
	print("WINDOWS PACKAGE: %d checks, %d failures"%[checks,failures])
	quit(1 if failures else 0)
