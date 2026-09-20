extends Control
const Game = preload("res://scripts/game.gd")
const GOLD = Color("d7bb79")
const INK = Color("e5e6d5")
const MUTED = Color("8e9f9a")
const COLORS = [Color("94998b"), Color("63bea6"), Color("d77d69"), Color("9da8ed"), Color("dbb670"), Color("7dc1dd"), Color("c698c9")]
var game = Game.new()
var city_views: Array = []
var selected: int = 13
var marching: bool = false
var font: Font
var month_label: Label
var info: Label
var stats: Label
var notice: Label
var logs: RichTextLabel
var city_buttons: Array[Button] = []
var develop_button: Button
var recruit_button: Button
var march_button: Button
var end_button: Button
var confirm_dialog: ConfirmationDialog
var message: String = "刘备军据守新野、荆州、衡阳。点击城池查看，出征时金框标出相邻城。"
var holdings: Label
var city_picker: OptionButton
var map_texture: Texture2D
var town_textures: Array[Texture2D] = []
const OWNER_ROM = [-1,4,2,0,1,3,5]
var history_dialog: AcceptDialog
var history_full: RichTextLabel

func _ready() -> void:
	city_views = game.cities
	select_home()
	message = game.history[0]
	map_texture = load("res://assets/original/world-roads.png")
	for i in range(6): town_textures.append(load("res://assets/original/town-%d.png" % i))
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	font = load("res://assets/chinese.ttf")
	var t = Theme.new()
	t.default_font = font
	t.default_font_size = 17
	t.set_color("font_color", "Label", INK)
	t.set_color("font_color", "Button", INK)
	t.set_color("font_disabled_color", "Button", Color("697871"))
	t.set_stylebox("normal", "Button", box(Color("1e302e"), Color("3b5149")))
	t.set_stylebox("hover", "Button", box(Color("2c4740"), GOLD))
	t.set_stylebox("pressed", "Button", box(Color("385348"), GOLD))
	t.set_stylebox("disabled", "Button", box(Color("172320"), Color("263731")))
	theme = t
	label_at("中 原", Vector2(36, 18), Vector2(200, 56), 38, GOLD)
	label_at("三 十 城 池  /  六 方 争 霸", Vector2(39, 74), Vector2(450, 30), 15, MUTED)
	month_label = label_at("", Vector2(490, 35), Vector2(390, 45), 23, INK)
	button_at("存档", Vector2(902, 33), Vector2(78, 40), save_game)
	button_at("读档", Vector2(992, 33), Vector2(78, 40), load_game)
	button_at("新局", Vector2(1082, 33), Vector2(78, 40), ask_reset)
	for owner in range(1, 7):
		var icon = TextureRect.new()
		icon.texture = town_textures[OWNER_ROM[owner]]
		icon.position = Vector2(400+(owner-1)*124,88)
		icon.size = Vector2(16,16)
		icon.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
		icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(icon)
		label_at(game.faction(owner), Vector2(421 + (owner-1)*124, 83), Vector2(102, 25), 14, INK)
	holdings = label_at("", Vector2(868, 178), Vector2(290, 26), 15, MUTED)
	city_picker = OptionButton.new()
	city_picker.position = Vector2(868, 214)
	city_picker.size = Vector2(290, 36)
	city_picker.get_popup().max_size = Vector2i(370, 480)
	for c in city_views: city_picker.add_item(c.name, int(c.id))
	city_picker.item_selected.connect(select_city)
	add_child(city_picker)
	for c in city_views:
		var point = city_point(int(c.id))
		var b = button_at("", point - Vector2(12, 12), Vector2(24, 24), select_city.bind(int(c.id)))
		b.add_theme_stylebox_override("normal", StyleBoxEmpty.new())
		b.add_theme_stylebox_override("hover", box(Color(1,1,1,0.12), GOLD))
		b.add_theme_stylebox_override("pressed", box(Color(1,1,1,0.2), GOLD))
		b.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
		city_buttons.append(b)
	info = label_at("", Vector2(868, 136), Vector2(290, 38), 23, GOLD)
	stats = label_at("", Vector2(868, 264), Vector2(290, 138), 17, INK)
	develop_button = button_at("开发  ·  60 金", Vector2(868, 411), Vector2(290, 43), do_action.bind("develop"))
	recruit_button = button_at("征兵  ·  70 金 / 50 粮", Vector2(868, 466), Vector2(290, 43), do_action.bind("recruit"))
	march_button = button_at("出征 / 调兵  ·  40 粮", Vector2(868, 521), Vector2(290, 43), begin_march)
	end_button = button_at("结束回合  →", Vector2(868, 611), Vector2(290, 54), end_turn)
	label_at("每月三道命令 · 占领全部30城获胜", Vector2(868, 681), Vector2(290, 30), 13, MUTED)
	notice = label_at("", Vector2(57, 625), Vector2(738, 46), 16, GOLD)
	notice.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	logs = RichTextLabel.new()
	logs.position = Vector2(57, 676)
	logs.size = Vector2(732, 36)
	logs.add_theme_color_override("default_color", MUTED)
	logs.add_theme_font_size_override("normal_font_size", 15)
	add_child(logs)
	label_at("原版坐标、道路与城池图块 · 内政和战斗规则仍在重制", Vector2(36, 723), Vector2(740, 25), 13, MUTED)
	history_dialog = AcceptDialog.new()
	history_dialog.title = "战报 · 最近 200 条"
	history_dialog.ok_button_text = "关闭"
	history_full = RichTextLabel.new()
	history_full.custom_minimum_size = Vector2(710, 380)
	history_dialog.add_child(history_full)
	add_child(history_dialog)
	button_at("查看完整战报", Vector2(868, 574), Vector2(290, 28), show_history)
	confirm_dialog = ConfirmationDialog.new()
	confirm_dialog.title = "重新开局"
	confirm_dialog.dialog_text = "重新开始会丢弃尚未保存的进度。"
	confirm_dialog.ok_button_text = "重新开始"
	confirm_dialog.cancel_button_text = "取消"
	confirm_dialog.confirmed.connect(func(): get_tree().change_scene_to_file("res://opening.tscn"))
	add_child(confirm_dialog)
	refresh()
	if "--capture" in OS.get_cmdline_user_args():
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("res://preview.png")
		get_tree().quit()

func box(background: Color, border: Color) -> StyleBoxFlat:
	var style = StyleBoxFlat.new()
	style.bg_color = background
	style.border_color = border
	style.set_border_width_all(1)
	style.content_margin_left = 12
	style.content_margin_right = 12
	return style

func label_at(value: String, pos: Vector2, extent: Vector2, size_px: int, color: Color) -> Label:
	var label = Label.new()
	label.text = value
	label.position = pos
	label.size = extent
	label.add_theme_font_size_override("font_size", size_px)
	label.add_theme_color_override("font_color", color)
	add_child(label)
	return label

func button_at(value: String, pos: Vector2, extent: Vector2, callback: Callable) -> Button:
	var button = Button.new()
	button.text = value
	button.position = pos
	button.size = extent
	button.pressed.connect(callback)
	add_child(button)
	return button

func city_point(index: int) -> Vector2:
	var pos: Array = city_views[index].pos
	return Vector2(42 + float(pos[0]) * 768 + 12, 126 + float(pos[1]) * 480 + 15)

func _draw() -> void:
	draw_rect(Rect2(0, 0, 1200, 760), Color("101b1b"))
	draw_line(Vector2(36, 113), Vector2(1164, 113), Color("465146"), 1)
	draw_rect(Rect2(36, 126, 786, 491), Color("1c2a27"))
	draw_rect(Rect2(843, 126, 321, 585), Color("172522"))
	if map_texture:
		draw_texture_rect_region(map_texture, Rect2(42,126,768,480), Rect2(0,0,256,160))
	for c in city_views:
		var center = city_point(int(c.id))
		if int(c.owner) > 0:
			draw_texture_rect(town_textures[OWNER_ROM[int(c.owner)]], Rect2(center-Vector2(12,12),Vector2(24,24)), false)
		if marching and game.reason("march", selected, int(c.id)).is_empty():
			draw_rect(Rect2(center-Vector2(13,13),Vector2(26,26)), Color("fff000"), false, 2)
	if not city_buttons.is_empty():
		var center = city_point(selected)
		draw_rect(Rect2(center-Vector2(14,14),Vector2(28,28)), Color("fff000"), false, 2)
	draw_line(Vector2(57, 670), Vector2(801, 670), Color("3a4b41"))

func refresh() -> void:
	city_views = game.cities
	month_label.text = "第 %02d 月    /    命令 %d / 3" % [game.month, game.orders]
	for i in range(city_buttons.size()):
		var c: Dictionary = city_views[i]
		city_buttons[i].text = ""
		city_buttons[i].tooltip_text = "%s · %s\n兵力 %d / 金 %d / 粮 %d" % [c.name, game.faction(int(c.owner)), c.troops, c.gold, c.grain]
		city_picker.set_item_text(i, "%02d  %s · %s" % [i+1, c.name, game.faction(int(c.owner))])
		city_buttons[i].add_theme_color_override("font_color", COLORS[int(c.owner)])
	var c: Dictionary = city_views[selected]
	info.text = "%s · %s" % [c.name, game.faction(int(c.owner))]
	var owned = 0
	for city in city_views:
		if int(city.owner) == game.player_owner: owned += 1
	holdings.text = "%s领地  %d / %d 城" % [game.faction(game.player_owner), owned, city_views.size()]
	city_picker.select(selected)
	var adjacent_names: PackedStringArray = []
	for neighbor in c.neighbors: adjacent_names.append(city_views[int(neighbor)].name)
	stats.text = "守将武力  %d     兵力  %d\n金钱  %d     粮草  %d\n发展  Lv.%d   /   月税 %d\n相邻：%s" % [c.ability, c.troops, c.gold, c.grain, c.development, 40 + int(c.development) * 20, "、".join(adjacent_names)]
	stats.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	stats.tooltip_text = "开局原版记录：土地 %d / 商业 %d / 人口 %d / 统治 %d\n驻军合计原版驻城武将兵种；粮草与发展等级为临时规则映射。" % [c.reference.land, c.reference.commerce, c.reference.population, c.reference.control]
	develop_button.disabled = not game.reason("develop", selected).is_empty()
	develop_button.tooltip_text = game.reason("develop", selected) if develop_button.disabled else "消耗一道命令；发展 +1，每月税收 +20、收粮 +25。"
	recruit_button.disabled = not game.reason("recruit", selected).is_empty()
	recruit_button.tooltip_text = game.reason("recruit", selected) if recruit_button.disabled else "消耗一道命令；增加 300 兵。"
	march_button.disabled = game.winner != 0 or game.orders <= 0 or int(c.owner) != game.player_owner or int(c.troops) < 200 or int(c.grain) < 40
	march_button.text = "取消出征选择" if marching else "出征 / 调兵  ·  40 粮"
	march_button.tooltip_text = "派出七成兵力。选择相邻敌城进攻，或己方城池调兵。"
	end_button.disabled = game.winner != 0
	notice.text = message
	if game.winner == 1: notice.text = "天下归一！%s占领全部30城。" % game.faction(game.player_owner)
	if game.winner == 2: notice.text = "己方城池全部失守。可以读档或重新开局。"
	logs.text = "\n".join(game.history)
	history_full.text = logs.text
	logs.scroll_to_line(0)
	queue_redraw()

func select_city(index: int) -> void:
	if marching:
		if index == selected:
			marching = false
			message = "已取消出征选择。"
		else:
			var problem: String = game.reason("march", selected, index)
			if problem.is_empty():
				message = game.act("march", selected, index)
				marching = false
			else:
				message = problem
	else:
		selected = index
		message = "已选择%s。" % city_views[index].name
	refresh()

func do_action(action: String) -> void:
	marching = false
	message = game.act(action, selected)
	refresh()

func begin_march() -> void:
	marching = not marching
	message = "点击道路相连的目标城池。敌城触发战斗，己方城池接收调兵。" if marching else "已取消出征选择。"
	refresh()

func end_turn() -> void:
	marching = false
	game.end_turn()
	message = "电脑行动结束，新月份已结算。查看下方战报了解局势。"
	refresh()

func save_game() -> void:
	var result: Error = game.save_game()
	message = "进度已保存。读档可恢复当前月份、城池和剩余命令。" if result == OK else "存档失败：%s" % error_string(result)
	refresh()

func load_game() -> void:
	var result: Error = game.load_game()
	if result == OK:
		city_views = game.cities
		select_home()
		marching = false
		message = "已恢复存档。"
	else:
		message = "没有可用存档，请先保存一次。" if result == ERR_FILE_NOT_FOUND else "存档无效，当前战局已保留。"
	refresh()

func ask_reset() -> void:
	confirm_dialog.popup_centered(Vector2i(430, 160))

func reset_game() -> void:
	game.reset()
	city_views = game.cities
	select_home()
	marching = false
	message = game.history[0]
	refresh()

func show_history() -> void:
	history_full.text = "\n\n".join(game.history)
	history_full.scroll_to_line(0)
	history_dialog.popup_centered(Vector2i(760, 440))

func select_home() -> void:
	selected = 13 if game.player_owner == 1 else 0
	if int(city_views[selected].owner) != game.player_owner:
		for c in city_views:
			if int(c.owner) == game.player_owner:
				selected = int(c.id)
				break
