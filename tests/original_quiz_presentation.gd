extends SceneTree
var failures=0
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func digest(data:PackedByteArray)->String:
	var hash=HashingContext.new()
	hash.start(HashingContext.HASH_SHA256)
	hash.update(data)
	return hash.finish().hex_encode()
func _init():call_deferred("run")
func run():
	var sound=root.get_node("OriginalSound")
	var opening=load("res://opening.tscn").instantiate()
	root.add_child(opening)
	opening.set_process(false)
	var evidence:Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://reference/fixtures/quiz-presentation.json"))
	var checked_frames=0
	var checked_sounds=0
	for name in evidence:
		var row:Dictionary=evidence[name]
		opening.model.reset();opening.refresh()
		opening.model.press("START")
		if name.begins_with("duplicate-"):opening.model.press("DOWN")
		opening.model.press("A");opening.model.press("A");opening.model.press("A")
		for yes in row.path:opening.model.press("A" if yes else "RIGHT");if not yes:opening.model.press("A")
		if name.begins_with("duplicate-"):
			opening.model.press("A");opening.model.press("A")
			for yes in row.path:opening.model.press("A" if yes else "RIGHT");if not yes:opening.model.press("A")
		opening.refresh()
		check(opening.presentation_clip==name,"native branch selects "+name)
		var before:Dictionary=opening.state.duplicate(true)
		await process_frame
		var starts:int=sound.effect_starts
		var text_frames=PackedInt32Array(row.text_frames)
		var expected=0
		var image_errors=0
		var sound_errors=0
		var cache_errors=0
		for frame in range(row.rgb_sha256.size()):
			if frame>0:opening._process(1.0/60.0)
			# Flush each logical frame so original pulse retriggers can be observed.
			await process_frame
			if digest(opening.picture.get_image().get_data())!=row.rgb_sha256[frame]:image_errors+=1
			if text_frames.has(frame):expected+=1
			if sound.effect_starts-starts!=expected:sound_errors+=1
			if opening.presentation_textures.size()>4:cache_errors+=1
		checked_frames+=row.rgb_sha256.size();checked_sounds+=expected
		check(image_errors==0,"original RGB sequence "+name+" mismatches="+str(image_errors))
		check(sound_errors==0 and expected==text_frames.size(),"original text sound frames "+name+" mismatches="+str(sound_errors))
		check(cache_errors==0,"animation texture cache remains bounded "+name)
		var after:Dictionary=opening.state.duplicate(true)
		before.erase("presentation_frame");after.erase("presentation_frame")
		check(before==after,"animation preserves native diagnosis outcome "+name)
		var frame_before:int=int(opening.state.presentation_frame)
		starts=sound.effect_starts
		opening.refresh();opening.refresh()
		await process_frame
		check(int(opening.state.presentation_frame)==frame_before and sound.effect_starts==starts,"refresh does not restart "+name)
		if opening.state.screen=="quiz":
			check(opening.choice_ready(),"question enables cursor after text "+name)
			opening.press("RIGHT")
			check(int(opening.state.cursor)==1 and int(opening.state.presentation_frame)==frame_before,"NO choice preserves animation progress "+name)
			var source=Image.load_from_file("res://assets/original/presentation/choice-quiz-%02d.png" % int(opening.state.question))
			check(opening.choice_texture().get_image().get_data()==source.get_data(),"NO choice uses captured original cursor pixels "+name)
		else:
			check(bool(opening.state.duplicate)==name.begins_with("duplicate-"),"duplicate rejection matches current player selection "+name)
	# Keyboard selection during a transition reveals the choices immediately.
	opening.model.reset();opening.refresh()
	opening.press("START");opening.press("A");opening.press("A");opening.press("A")
	check(not opening.choice_ready(),"new question waits for transition before showing choices")
	opening.press("RIGHT")
	check(opening.choice_ready() and int(opening.state.cursor)==1,"early navigation reveals and selects NO")
	check(sound.last_effect!="text","early navigation suppresses skipped text sound burst")
	check(checked_frames==15780 and checked_sounds==702,"all33new clips and702text events covered")
	opening.queue_free()
	await process_frame
	await sound.shutdown()
	print("ORIGINAL QUIZ PRESENTATION: %d failures" % failures)
	quit(1 if failures else 0)
