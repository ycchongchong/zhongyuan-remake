extends SceneTree
var failures=0
func check(ok:bool,message:String):
	print(("PASS: " if ok else "FAIL: ")+message)
	if not ok:failures+=1
func _init():call_deferred("run")
func run():
	var opening=load("res://opening.tscn").instantiate()
	root.add_child(opening);opening.set_process(false)
	opening.press("START");opening.press("DOWN");opening.press("A");opening.press("A");opening.press("A")
	var before:Dictionary=opening.model.snapshot()
	var pad=InputEventJoypadButton.new();pad.button_index=JOY_BUTTON_DPAD_RIGHT;pad.pressed=true;pad.device=1
	opening._input(pad)
	check(opening.model.snapshot()==before,"second controller cannot change first diagnosis")
	pad.device=0;opening._input(pad)
	check(opening.choice_ready() and int(opening.state.cursor)==1,"first controller reveals and selects NO during transition")
	var mouse=InputEventMouseButton.new();mouse.button_index=MOUSE_BUTTON_LEFT;mouse.pressed=true
	mouse.position=opening.ORIGIN+Vector2(110,204)*opening.SCALE
	opening._input(mouse)
	check(int(opening.state.question)==2 and opening.presentation_clip=="answer-1-yes","mouse YES replaces prior NO and enters the correct native branch")
	var key=InputEventKey.new();key.keycode=KEY_RIGHT;key.pressed=true;opening._input(key)
	key.keycode=KEY_Z;opening._input(key)
	check(int(opening.state.question)==5 and opening.presentation_clip=="answer-2-no","keyboard NO routes through its own animation")
	# Enter a measured duplicate result with the native model, then exercise port routing.
	opening.model.reset();opening.refresh()
	opening.model.press("START");opening.model.press("DOWN");opening.model.press("A");opening.model.press("A");opening.model.press("A")
	for i in range(5):opening.model.press("A")
	opening.model.press("A");opening.model.press("A")
	for i in range(5):opening.model.press("A")
	opening.refresh()
	check(opening.state.duplicate and opening.presentation_clip=="duplicate-4","second player displays captured duplicate rejection")
	before=opening.model.snapshot();pad.button_index=JOY_BUTTON_A;pad.device=0;opening._input(pad)
	check(opening.model.snapshot()==before,"first controller cannot confirm second-player rejection")
	pad.device=1;opening._input(pad)
	check(opening.state.screen=="intro" and opening.presentation_clip=="quiz-intro-second","second controller confirms rejection and restarts its diagnosis")
	check(int(opening.state.first_ruler)==4 and int(opening.state.second_ruler)==-1,"rejection retains first player and clears only duplicate selection")
	pad=null;mouse=null;key=null
	opening.queue_free();await process_frame
	await root.get_node("OriginalSound").shutdown()
	print("ORIGINAL QUIZ INPUT: %d failures" % failures)
	quit(1 if failures else 0)
