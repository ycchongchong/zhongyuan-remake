extends Node
## Offline captures of the original ROM sound driver. No emulator runs in game.
signal muted_changed(value: bool)
const DIRECTORY = "res://assets/original/audio/"
const SETTINGS = "user://audio.cfg"
var manifest: Dictionary = {}
var streams: Dictionary = {}
var music: AudioStreamPlayer
var effect: AudioStreamPlayer
var current_cue = ""
var last_effect = ""
var music_starts = 0
var effect_starts = 0
var muted = false
var music_pending = false
var effect_pending = false
var scene_owner = 0
var closing = false

class MuteButton extends Button:
	func update_state(value: bool) -> void:
		text = "声音：关" if value else "声音：开"

func _ready() -> void:
	var parsed = JSON.parse_string(FileAccess.get_file_as_string(DIRECTORY + "manifest.json"))
	if parsed is Dictionary: manifest = parsed
	music = AudioStreamPlayer.new()
	effect = AudioStreamPlayer.new()
	add_child(music)
	add_child(effect)
	var settings = ConfigFile.new()
	if settings.load(SETTINGS) == OK: muted = bool(settings.get_value("sound", "muted", false))
	apply_volume()
	get_tree().auto_accept_quit = false
	get_tree().root.close_requested.connect(close_game)

func stream_for(cue: String) -> AudioStreamWAV:
	if streams.has(cue): return streams[cue]
	var tracks: Dictionary = manifest.get("tracks", {})
	if not tracks.has(cue): return null
	var data: Dictionary = tracks[cue]
	var stream = AudioStreamWAV.load_from_file(DIRECTORY + str(data.file))
	if stream == null: return null
	if data.end == "tracker_repeat":
		stream.loop_mode = AudioStreamWAV.LOOP_FORWARD
		stream.loop_begin = int(data.loop_start_sample)
		stream.loop_end = int(data.loop_end_sample)
	streams[cue] = stream
	return stream

func set_music(cue: String) -> void:
	if closing: return
	# A finished one-shot must stay finished; frequent UI refreshes are harmless.
	if cue == current_cue: return
	current_cue = cue
	# Coalesce intermediate transitions in one UI frame. Starting and replacing
	# WAV playback immediately can strand a playback object in Godot 4.5.2.
	if not music_pending:
		music_pending = true
		call_deferred("apply_music")

func can_start_audio() -> bool:
	if closing: return false
	if scene_owner == 0: return true
	var owner = instance_from_id(scene_owner)
	# Deferred calls run before queued scene deletion. Do not create a voice
	# that tree_exiting would stop immediately in the same frame.
	return owner is Node and owner.is_inside_tree() and not owner.is_queued_for_deletion()

func apply_music() -> void:
	music_pending = false
	if not is_inside_tree(): return
	if not current_cue.is_empty() and not can_start_audio(): return
	music.stop()
	music.stream = null
	music.stream = stream_for(current_cue)
	if music.stream != null:
		music.play()
		music_starts += 1

func play_effect(cue: String) -> void:
	if closing: return
	# One captured effect per action. The PCM bank does not emulate the original
	# per-channel music preemption; concurrent channel mixing remains a limitation.
	if cue not in ["confirm", "cursor", "text", "clash_hit", "clash_bow"]: return
	last_effect = cue
	if not effect_pending:
		effect_pending = true
		call_deferred("apply_effect")

func stop_effect(cue: String) -> void:
	if last_effect != cue: return
	last_effect = ""
	effect.stop()
	effect.stream = null

func apply_effect() -> void:
	effect_pending = false
	if not is_inside_tree(): return
	if not last_effect.is_empty() and not can_start_audio(): return
	effect.stop()
	effect.stream = stream_for(last_effect)
	if effect.stream != null:
		effect.play()
		effect_starts += 1

func opening_cue(screen: String) -> String:
	if screen == "title": return "title"
	if screen in ["intro", "quiz", "result"]: return "diagnosis"
	return ""

func opening_input(before: Dictionary, after: Dictionary, key: String) -> void:
	# Confirm and cursor IDs 16/17 were traced in the original main/difficulty menus.
	if (before.screen in ["intro","quiz"] or (before.screen == "result" and after.screen == "intro")) and key in ["A", "START"]:
		play_effect("confirm")
	if before.screen not in ["menu", "difficulty"]: return
	if before.cursor != after.cursor and key in ["UP", "DOWN", "LEFT", "RIGHT"]:
		play_effect("cursor")
	elif key in ["A", "START"]:
		play_effect("confirm")

func set_muted(value: bool, persist := true) -> void:
	muted = value
	apply_volume()
	if persist:
		var settings = ConfigFile.new()
		settings.set_value("sound", "muted", muted)
		settings.save(SETTINGS)
	muted_changed.emit(muted)

func apply_volume() -> void:
	# Keep playback moving while muted, so unmuting does not restart the score.
	if music != null: music.volume_linear = 0.0 if muted else 1.0
	if effect != null: effect.volume_linear = 0.0 if muted else 1.0

func shutdown() -> void:
	current_cue = ""
	last_effect = ""
	music.stop()
	effect.stop()
	music.stream = null
	effect.stream = null
	# Audio runs on a separate thread. SceneTreeTimer consumes the current
	# frame delta, so a long frame can expire a new timer immediately.
	# Give the mixer actual wall-clock time to retire voices before quitting.
	var deadline = Time.get_ticks_msec() + 100
	while Time.get_ticks_msec() < deadline:
		await get_tree().create_timer(0.01, true, false, true).timeout

func close_game() -> void:
	if closing: return
	closing = true
	await shutdown()
	get_tree().quit()

func attach_scene(scene: Node) -> void:
	scene_owner = scene.get_instance_id()
	scene.tree_exiting.connect(release_scene.bind(scene_owner))

func release_scene(id: int) -> void:
	# Opening and campaign can coexist briefly during a successful transition.
	if scene_owner != id: return
	scene_owner = 0
	set_music("")
	effect.stop()
	effect.stream = null
	last_effect = ""

func add_mute_button(parent: Control, position: Vector2) -> Button:
	var button = MuteButton.new()
	button.position = position
	button.size = Vector2(110, 36)
	button.text = "声音：关" if muted else "声音：开"
	button.pressed.connect(func(): set_muted(not muted))
	muted_changed.connect(button.update_state)
	parent.add_child(button)
	return button

func _exit_tree() -> void:
	music.stop()
	effect.stop()
	music.stream = null
	effect.stream = null
	streams.clear()
