extends SceneTree
var failures=0
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func _init():call_deferred("run")
func run():
	var sound=root.get_node("OriginalSound")
	# The original regression: deferred play requests can run before queue_free.
	for i in range(12):
		var owner=Node.new();root.add_child(owner);sound.attach_scene(owner)
		var music_count:int=sound.music_starts
		var effect_count:int=sound.effect_starts
		sound.set_music("diagnosis");sound.play_effect("confirm")
		owner.queue_free()
		await process_frame
		check(sound.music_starts==music_count and sound.effect_starts==effect_count,"queued scene starts no music or effect voice: "+str(i))
		check(sound.scene_owner==0 and sound.current_cue=="" and sound.last_effect=="","queued scene clears ownership and requests: "+str(i))
	var live=Node.new();root.add_child(live);sound.attach_scene(live)
	var music_count:int=sound.music_starts
	var effect_count:int=sound.effect_starts
	sound.set_music("campaign");sound.play_effect("cursor")
	await process_frame
	check(sound.music.playing and sound.effect.playing and sound.music_starts==music_count+1 and sound.effect_starts==effect_count+1,"live scene still starts both players")
	await create_timer(0.03).timeout
	music_count=sound.music_starts;effect_count=sound.effect_starts
	sound.set_music("duel");sound.play_effect("text");live.queue_free()
	await process_frame
	check(sound.music_starts==music_count and sound.effect_starts==effect_count,"scene removed while active cancels replacement voices")
	check(not sound.music.playing and not sound.effect.playing,"existing scene audio stops on removal")
	var old=Node.new();root.add_child(old);sound.attach_scene(old)
	sound.set_music("campaign")
	await process_frame
	await create_timer(0.03).timeout
	old.queue_free()
	var next=Node.new();root.add_child(next);sound.attach_scene(next)
	sound.set_music("tactical");sound.play_effect("confirm")
	await process_frame
	check(sound.scene_owner==next.get_instance_id() and sound.current_cue=="tactical" and sound.music.playing,"old scene deletion preserves new scene music")
	check(sound.last_effect=="confirm" and sound.effect.playing,"old scene deletion preserves new scene effect")
	await create_timer(0.03).timeout
	next.queue_free();await process_frame
	# Direct sound use without a scene owner remains supported by audio tools.
	sound.set_music("title");sound.play_effect("cursor")
	await process_frame
	check(sound.music.playing and sound.effect.playing,"unowned diagnostic playback remains supported")
	await create_timer(0.03).timeout
	await sound.shutdown()
	# Retain weak references only: a successful stop must release mixer voices.
	for i in range(6):
		sound.set_music("tactical");sound.play_effect("confirm")
		await process_frame
		var music_voice=weakref(sound.music.get_stream_playback())
		var effect_voice=weakref(sound.effect.get_stream_playback())
		# Model a slow battle calculation within the current frame.
		OS.delay_msec(120)
		var shutdown_started=Time.get_ticks_msec()
		await sound.shutdown()
		check(Time.get_ticks_msec()-shutdown_started>=100,"shutdown waits real mixer time after a slow frame: "+str(i))
		check(music_voice.get_ref()==null and effect_voice.get_ref()==null,"immediate shutdown releases both mixer voices: "+str(i))
	check(sound.music.stream==null and sound.effect.stream==null,"shutdown retires both streams")
	print("ORIGINAL AUDIO LIFECYCLE: %d failures" % failures)
	quit(1 if failures else 0)
