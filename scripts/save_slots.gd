extends ConfirmationDialog
## PC save selection only. Native C++ still owns serialization and validation.
signal slot_chosen(path: String)
const PATHS = ["user://campaign-native-original.json", "user://campaign-native-original-2.json", "user://campaign-native-original-3.json"]
const RULERS = ["袁绍", "马腾", "曹操", "孙权", "刘备", "刘璋"]
const MAX_BYTES = 32 * 1024 * 1024
var paths: Array = PATHS.duplicate()
var saving = false
var selected_slot = 0
var entries: Array[Button] = []
var explanation: Label

func _ready() -> void:
	cancel_button_text = "取消"
	var content = VBoxContainer.new()
	content.add_theme_constant_override("separation", 12)
	add_child(content)
	explanation = Label.new()
	explanation.custom_minimum_size = Vector2(580, 44)
	explanation.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	content.add_child(explanation)
	var group = ButtonGroup.new()
	for slot in range(3):
		var entry = Button.new()
		entry.toggle_mode = true
		entry.button_group = group
		entry.alignment = HORIZONTAL_ALIGNMENT_LEFT
		entry.custom_minimum_size = Vector2(580, 66)
		entry.pressed.connect(select_slot.bind(slot))
		entries.append(entry)
		content.add_child(entry)
	confirmed.connect(confirm_slot)

func summary(path: String) -> String:
	if not FileAccess.file_exists(path): return "空存档"
	var file = FileAccess.open(path, FileAccess.READ)
	if file == null: return "无法读取"
	if file.get_length() > MAX_BYTES: return "文件过大，无法读取"
	var parser = JSON.new()
	if parser.parse(file.get_as_text()) != OK: return "文件损坏，无法预览"
	var data = parser.data
	if not data is Dictionary or data.get("format", "") not in ["native-original-v1", "native-original-v2", "native-original-v3"]: return "格式不受支持"
	var raw = data.get("sram")
	if not raw is Array or raw.size() != 8192: return "数据不完整"
	for at in [0xd85, 0xd86, 0xd87, 0xd89, 0xd8a]:
		if not (raw[at] is int or raw[at] is float) or raw[at] < 0 or raw[at] > 255 or raw[at] != int(raw[at]): return "数据不完整"
	var first = int(raw[0xd89]) & 7
	var second = int(raw[0xd8a]) & 7
	if first >= RULERS.size() or ((int(raw[0xd8a]) & 128) != 0 and second >= RULERS.size()): return "玩家数据无效"
	var players: String = RULERS[first]
	if (int(raw[0xd8a]) & 128) != 0: players += " / " + RULERS[second]
	var phases = {"player_commands":"战略指令", "ai_turn":"电脑回合", "battle":"守城战", "expedition":"出征", "ending":"战局结束"}
	return "%d 年 %d 月 · %s · %s" % [int(raw[0xd85]) + 256 * int(raw[0xd86]), int(raw[0xd87]), players, phases.get(data.get("phase", "player_commands"), "战局记录")]

func show_slots(save_mode: bool) -> void:
	saving = save_mode
	title = "保存战局" if saving else "读取战局"
	explanation.text = "选择存档槽。保存到已有记录的位置会覆盖该记录。槽 1 兼容旧版存档。" if saving else "选择要读取的存档。当前未保存的进度将被替换。槽 1 兼容旧版存档。"
	for slot in range(3):
		entries[slot].text = "槽 %d\n%s" % [slot + 1, summary(paths[slot])]
		entries[slot].disabled = not saving and not FileAccess.file_exists(paths[slot])
	if not saving and entries[selected_slot].disabled:
		for slot in range(3):
			if not entries[slot].disabled:
				selected_slot = slot
				break
	select_slot(selected_slot)
	popup_centered(Vector2i(640, 350))
	if not entries[selected_slot].disabled: entries[selected_slot].grab_focus()
	else: get_cancel_button().grab_focus()

func select_slot(slot: int) -> void:
	selected_slot = slot
	entries[slot].button_pressed = true
	get_ok_button().disabled = entries[slot].disabled
	ok_button_text = ("覆盖槽 %d" if FileAccess.file_exists(paths[slot]) else "保存到槽 %d") % (slot + 1) if saving else "读取槽 %d" % (slot + 1)

func confirm_slot() -> void:
	if entries[selected_slot].disabled: return
	slot_chosen.emit(paths[selected_slot])
