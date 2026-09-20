extends Control
## Original visual assets and inputs; diagnosis transitions are owned by C++.
const ORIGIN = Vector2(216, 12)
const SCALE = 3.0
const ROM_OWNER = [3,4,2,5,1,6]
var model = ZhongyuanOpening.new()
var state: Dictionary
var picture: Texture2D
var hint: Label
var message: AcceptDialog
var transitioning = false
const PRESENTATION_DIRECTORY = "res://assets/original/presentation/"
var presentations: Dictionary = {}
var presentation_choices: Dictionary = {}
var presentation_textures: Dictionary = {}
var presentation_clip = ""
var presentation_file = ""
var presented_frame = -1
var frame_remainder = 0.0

func _ready() -> void:
	OriginalSound.attach_scene(self)
	var presentation_data:Dictionary=JSON.parse_string(FileAccess.get_file_as_string(PRESENTATION_DIRECTORY+"manifest.json"))
	presentations=presentation_data.clips
	presentation_choices=presentation_data.get("choices",{})
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	var t = Theme.new()
	t.default_font = load("res://assets/chinese.ttf")
	t.default_font_size = 18
	theme = t
	hint = Label.new()
	hint.position = Vector2(35,737)
	hint.size = Vector2(1130,23)
	hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	hint.add_theme_font_size_override("font_size",14)
	add_child(hint)
	message = AcceptDialog.new()
	message.title = "重制进度"
	message.ok_button_text = "返回"
	add_child(message)
	OriginalSound.add_mute_button(self, Vector2(36, 24))
	refresh()
	if "--capture" in OS.get_cmdline_user_args():
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("res://opening-preview.png")
		get_tree().quit()

func refresh() -> void:
	state = model.snapshot()
	if not transitioning: OriginalSound.set_music(OriginalSound.opening_cue(state.screen))
	var file = "title"
	match state.screen:
		"menu": file = "main-menu"
		"difficulty": file = "difficulty"
		"intro": file = "quiz-intro-second" if int(state.selecting_player)==1 else "quiz-intro"
		"quiz": file = "quiz-%02d" % int(state.question)
		"result": file = "ruler-%d" % int(state.ruler)
	picture = load("res://assets/original/%s.png" % file)
	update_presentation()
	hint.text = "方向键选择 · Z / Enter 确认 · X 返回 · 支持鼠标及手柄"
	if int(state.player_count)==2: hint.text = "玩家%s选择 · 键盘 Z 确认 · 对应手柄操作" % ("二" if int(state.selecting_player)==1 else "一")
	if state.screen == "result" and state.duplicate: hint.text = "该君主已被玩家一选择，确认后重新诊断。"
	queue_redraw()

func update_presentation() -> void:
	var clip:String=state.get("presentation_key","")
	if not presentations.has(clip):clip=""
	if clip != presentation_clip:
		OriginalSound.stop_effect("text")
		presentation_clip=clip
		presentation_file=""
		presented_frame=-1
		frame_remainder=0.0
	if clip.is_empty():return
	var data:Dictionary=presentations[clip]
	var frame:int=int(state.presentation_frame)
	var visual_frame:int=frame
	if visual_frame>=int(data.loop_start):
		visual_frame=int(data.loop_start)+(visual_frame-int(data.loop_start))%int(data.loop_frames)
	var file:String=data.frames[0].file
	for row in data.frames:
		if int(row.frame)>visual_frame:break
		file=row.file
	presentation_file=file
	picture=presentation_texture(file)
	# At low frame rates, replace missed pulse retriggers with the latest one;
	# never replay a burst of delayed text sounds after a pause.
	for sound_frame in data.text_frames:
		if int(sound_frame)>presented_frame and int(sound_frame)<=frame:
			OriginalSound.play_effect("text")
	presented_frame=frame

func presentation_texture(file:String) -> Texture2D:
	if not presentation_textures.has(file):
		presentation_textures[file]=ImageTexture.create_from_image(Image.load_from_file(PRESENTATION_DIRECTORY+file))
		# Stream long transitions with a small cache instead of retaining every frame.
		if presentation_textures.size()>4:presentation_textures.erase(presentation_textures.keys()[0])
	return presentation_textures[file]

func choice_ready() -> bool:
	return state.screen=="quiz" and (presentation_clip.is_empty() or int(state.presentation_frame)>=int(presentations[presentation_clip].loop_start))

func choice_texture() -> Texture2D:
	var key="quiz-%02d" % int(state.question)
	return presentation_texture(presentation_choices[key].file)

func _process(delta:float) -> void:
	if transitioning or state==null:return
	frame_remainder+=maxf(0.0,delta)*60.0
	var frames:int=mini(int(frame_remainder),2147483647)
	if frames<=0:return
	frame_remainder-=frames
	model.tick(frames)
	state=model.snapshot()
	if not presentation_clip.is_empty():
		update_presentation()
		queue_redraw()

func _draw() -> void:
	draw_rect(Rect2(0,0,1200,760),Color.BLACK)
	if not picture: return
	draw_texture_rect(picture,Rect2(ORIGIN,Vector2(768,720)),false)
	if state.screen=="result" and state.duplicate and presentation_clip.is_empty():
		draw_texture_rect_region(preload("res://assets/original/ruler-already-selected.png"),Rect2(ORIGIN+Vector2(88,160)*SCALE,Vector2(152,40)*SCALE),Rect2(88,160,152,40))
	if state.screen == "menu" or state.screen == "difficulty":
		var x = 77 if state.screen == "menu" else 101
		for i in range(3): draw_rect(Rect2(ORIGIN+Vector2(x,82+i*24)*SCALE,Vector2(8,12)*SCALE),Color.BLACK)
		triangle(Vector2(x+2,84+int(state.cursor)*24),Color.WHITE)
	elif choice_ready() and int(state.cursor)==1:
		# Replace only the two original cursor cells, retaining every animated pixel.
		for x in [87,151]:
			draw_texture_rect_region(choice_texture(),Rect2(ORIGIN+Vector2(x,201)*SCALE,Vector2(8,12)*SCALE),Rect2(x,201,8,12))

func triangle(at: Vector2, color: Color) -> void:
	draw_colored_polygon(PackedVector2Array([ORIGIN+at*SCALE,ORIGIN+(at+Vector2(0,7))*SCALE,ORIGIN+(at+Vector2(5,3))*SCALE]),color)

func press(key: String) -> void:
	if transitioning or message.visible: return
	if state.screen=="quiz" and key in ["LEFT","RIGHT"] and not choice_ready():
		model.tick(int(presentations[presentation_clip].loop_start)-int(state.presentation_frame))
		presented_frame=int(model.snapshot().presentation_frame)
		OriginalSound.stop_effect("text")
	var before: Dictionary = model.snapshot()
	var result: String = model.press(key)
	OriginalSound.opening_input(before, model.snapshot(), key)
	if result == "start": start_game(false)
	elif result == "continue": start_game(true)
	refresh()

func start_game(restore_save: bool) -> void:
	var scene = load("res://original_campaign.tscn").instantiate()
	var selected = model.snapshot()
	var error: String = scene.initialize(maxi(0,int(selected.first_ruler)),int(selected.difficulty),restore_save,int(selected.second_ruler) if int(selected.player_count)==2 else -1)
	if not error.is_empty():
		scene.free()
		message.dialog_text = error
		message.popup_centered(Vector2i(620,180))
		return
	transitioning = true
	get_tree().root.add_child(scene)
	get_tree().current_scene = scene
	queue_free()

func _input(event: InputEvent) -> void:
	if message.visible: return
	if event is InputEventKey and event.pressed and not event.echo:
		var keys = {KEY_UP:"UP",KEY_DOWN:"DOWN",KEY_LEFT:"LEFT",KEY_RIGHT:"RIGHT",KEY_Z:"A",KEY_ENTER:"START",KEY_SPACE:"START",KEY_X:"B",KEY_ESCAPE:"B"}
		if keys.has(event.keycode):
			press(keys[event.keycode])
			get_viewport().set_input_as_handled()
	elif event is InputEventJoypadButton and event.pressed:
		if event.device!=int(state.selecting_player): return
		var keys = {JOY_BUTTON_A:"A",JOY_BUTTON_B:"B",JOY_BUTTON_START:"START",JOY_BUTTON_DPAD_UP:"UP",JOY_BUTTON_DPAD_DOWN:"DOWN",JOY_BUTTON_DPAD_LEFT:"LEFT",JOY_BUTTON_DPAD_RIGHT:"RIGHT"}
		if keys.has(event.button_index): press(keys[event.button_index])
	elif event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		var at = (event.position-ORIGIN)/SCALE
		if not Rect2(0,0,256,240).has_point(at): return
		if state.screen in ["menu","difficulty"]:
			var row = int(floor((at.y-78)/24))
			if row<0 or row>2: return
			while int(model.snapshot().cursor) != row: press("DOWN")
		elif state.screen == "quiz":
			var option = 0 if at.x<144 else 1
			if int(state.cursor) != option: press("RIGHT")
		press("A")
