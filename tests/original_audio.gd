extends SceneTree
var failures = 0
func check(ok: bool, message: String):
	print(("PASS: " if ok else "FAIL: ") + message)
	if not ok: failures += 1
func _init(): call_deferred("run")
func run():
	var sound = root.get_node("OriginalSound")
	var original_mute: bool = sound.muted
	for cue in sound.manifest.tracks:
		var stream: AudioStreamWAV = sound.stream_for(cue)
		var data: Dictionary = sound.manifest.tracks[cue]
		check(stream != null and absf(stream.get_length() - float(data.samples)/48000.0) < 0.001, cue + " decodes its complete original capture")
		check((stream.loop_mode == AudioStreamWAV.LOOP_FORWARD) == (data.end == "tracker_repeat"), cue + " uses measured loop or one-shot ending")
		check(stream.format == AudioStreamWAV.FORMAT_16_BITS and not stream.stereo and stream.mix_rate == 48000, cue + " preserves lossless 48 kHz mono PCM")
		var wav = FileAccess.get_file_as_bytes(sound.DIRECTORY + str(data.file))
		check(stream.data == wav.slice(44), cue + " playback samples equal captured PCM bytes")
		if stream.loop_mode != AudioStreamWAV.LOOP_DISABLED: check(stream.loop_begin == int(data.loop_start_sample) and stream.loop_end == int(data.loop_end_sample), cue + " preserves sample-accurate intro and loop bounds")
	var opening = load("res://opening.tscn").instantiate()
	root.add_child(opening)
	await process_frame
	check(sound.current_cue == "title" and sound.music.playing, "title starts original six-channel score")
	var starts: int = sound.music_starts
	opening.refresh(); opening.refresh()
	check(sound.music_starts == starts, "refresh does not restart title")
	sound.music.stop(); opening.refresh()
	check(not sound.music.playing and sound.music_starts == starts, "finished one-shot stays finished")
	opening.press("START")
	await process_frame
	check(sound.current_cue == "" and not sound.music.playing, "original main menu is silent")
	opening.press("DOWN")
	check(sound.last_effect == "cursor", "menu movement plays traced sound ID 17")
	opening.press("UP"); opening.press("A")
	check(sound.last_effect == "confirm", "menu confirmation plays traced sound ID 16")
	opening.press("A")
	await process_frame
	check(sound.current_cue == "diagnosis", "diagnosis introduction starts its score")
	starts = sound.music_starts
	opening.press("A"); opening.press("A")
	check(sound.music_starts == starts, "diagnosis questions share uninterrupted music")
	sound.set_muted(true, false)
	check(sound.music.volume_linear == 0.0 and sound.effect.volume_linear == 0.0 and sound.music.playing, "mute silences both players while keeping music progression")
	sound.set_muted(false, false)
	check(sound.music.volume_db == 0.0 and sound.music_starts == starts, "unmute does not restart music")
	opening.queue_free()
	await process_frame
	var scene = load("res://original_campaign.tscn").instantiate()
	check(scene.initialize(4, 0).is_empty(), "initialize native audio campaign")
	root.add_child(scene); current_scene = scene; scene.set_process(false)
	await process_frame
	check(sound.current_cue == "campaign", "campaign UI uses native strategic cue")
	for row in [["computer-plan-start", "tactical"], ["clash-session-start", "clash_orders"], ["clash-strategy-boundary", "clash"], ["natural-campaign-defeat", "defeat"]]:
		check(scene.native.load_session("res://tests/%s.json" % row[0]).is_empty(), "load audio boundary " + row[0])
		var before: Dictionary = scene.native.session_snapshot()
		scene.refresh()
		await process_frame
		check(sound.current_cue == row[1] and sound.music.playing, "UI selects " + row[1])
		starts = sound.music_starts
		scene.refresh(); scene.refresh()
		check(sound.music_starts == starts and scene.native.session_snapshot() == before, "repeated audio refresh preserves playback and native state")
	scene.return_to_title()
	await scene_changed
	await process_frame
	check(sound.current_cue == "title", "return to title replaces failure music")
	current_scene.queue_free()
	await process_frame
	check(sound.muted_changed.get_connections().is_empty(), "scene removal disconnects mute controls")
	starts = sound.music_starts
	sound.set_music("title"); sound.set_music("campaign"); sound.set_music("tactical")
	await process_frame
	check(sound.music_starts == starts + 1 and sound.music.stream == sound.stream_for("tactical"), "same-frame music transitions start only the final cue")
	var effects: int = sound.effect_starts
	sound.play_effect("cursor"); sound.play_effect("confirm"); sound.play_effect("cursor")
	await process_frame
	check(sound.effect_starts == effects + 1 and sound.effect.stream == sound.stream_for("cursor"), "same-frame menu effects replace earlier requests")
	sound.set_muted(original_mute, false)
	check(not auto_accept_quit and root.close_requested.is_connected(sound.close_game), "window close waits for audio cleanup")
	await sound.shutdown()
	check(not sound.music.playing and not sound.effect.playing and sound.music.stream == null and sound.effect.stream == null, "graceful shutdown retires both audio streams")
	print("ORIGINAL AUDIO: %d failures" % failures)
	quit(1 if failures else 0)
