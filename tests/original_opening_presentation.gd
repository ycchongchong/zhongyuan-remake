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
	var original_mute:bool=sound.muted
	var opening=load("res://opening.tscn").instantiate()
	root.add_child(opening)
	opening.set_process(false)
	var evidence:Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://reference/fixtures/opening-presentation.json"))
	for name in ["quiz-intro","quiz-intro-second"]:
		opening.model.reset()
		opening.refresh()
		opening.press("START")
		if name=="quiz-intro-second":opening.press("DOWN")
		opening.press("A");opening.press("A")
		if name=="quiz-intro-second":
			opening.press("A")
			for i in range(5):opening.press("A")
			opening.press("A")
		check(opening.presentation_clip==name,"enter measured "+name)
		check(int(opening.state.presentation_frame)==0,"new introduction starts at native frame zero")
		await process_frame
		var starts:int=sound.effect_starts
		var mismatch=0
		var sound_mismatch=0
		var expected_sounds=0
		var text_frames=PackedInt32Array(evidence[name].text_frames)
		var original_state:Dictionary=opening.state.duplicate(true)
		for frame in range(evidence[name].rgb_sha256.size()):
			if frame>0:opening._process(1.0/60.0)
			await process_frame
			if digest(opening.picture.get_image().get_data())!=evidence[name].rgb_sha256[frame]:mismatch+=1
			if text_frames.has(frame):expected_sounds+=1
			if sound.effect_starts-starts!=expected_sounds:sound_mismatch+=1
		check(mismatch==0,"every captured RGB frame and repeated blink matches original: "+name+" mismatches="+str(mismatch))
		check(sound_mismatch==0,"every observed text retrigger occurs on its captured frame: "+name+" mismatches="+str(sound_mismatch))
		check(expected_sounds==evidence[name].text_frames.size(),"all text events consumed once")
		var after:Dictionary=opening.state.duplicate(true)
		after.erase("presentation_frame");original_state.erase("presentation_frame")
		check(after==original_state,"visual and audio playback preserves diagnosis state")
		starts=sound.effect_starts
		opening.refresh();opening.refresh()
		await process_frame
		check(sound.effect_starts==starts,"repeated refresh does not replay old syllables")
		opening._process(120.0)
		await process_frame
		check(sound.effect_starts==starts,"long idle never replays typing during blink loop")
		var before_file:String=opening.presentation_file
		opening._process(8.0/60.0)
		check(opening.presentation_file!=before_file,"completed text retains original alternating blink")
		opening.press("A")
		check(opening.state.screen=="quiz" and opening.presentation_clip=="quiz-01","confirmation replaces introduction with question transition")
	# Rapidly leaving an introduction cancels pending and already playing text.
	opening.model.reset();opening.refresh()
	opening.press("START");opening.press("A");opening.press("A")
	opening._process(7.0/60.0)
	check(sound.last_effect=="text","first original glyph requests sound19")
	opening.press("A")
	await process_frame
	check(sound.last_effect=="confirm" and sound.effect.stream==sound.stream_for("confirm"),"skip prevents stale text sound in next screen")
	sound.play_effect("text")
	sound.set_muted(true,false)
	await process_frame
	check(sound.effect.stream==sound.stream_for("text") and sound.effect.volume_linear==0.0,"text effect obeys global mute")
	sound.set_muted(original_mute,false)
	opening.queue_free()
	await process_frame
	check(sound.last_effect=="" and sound.effect.stream==null,"scene removal releases text playback")
	await sound.shutdown()
	print("ORIGINAL OPENING PRESENTATION: %d failures" % failures)
	quit(1 if failures else 0)
