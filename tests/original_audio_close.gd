extends SceneTree
var failures = 0
func check(ok: bool, message: String):
	print(("PASS: " if ok else "FAIL: ") + message)
	if not ok: failures += 1
func _init(): call_deferred("run")
func run():
	var sound = root.get_node("OriginalSound")
	sound.set_music("title")
	await process_frame
	check(sound.music.playing, "audio is active before window close")
	root.close_requested.emit()
	check(sound.closing and sound.music.stream == null, "close request begins orderly shutdown")
	sound.set_music("campaign"); sound.play_effect("confirm")
	await process_frame
	check(sound.current_cue == "" and sound.last_effect == "" and not sound.music.playing and not sound.effect.playing, "late scene updates cannot restart sound during close")
	print("ORIGINAL AUDIO CLOSE: %d failures" % failures)
	# The close handler must actually terminate the process; the runner times out otherwise.
