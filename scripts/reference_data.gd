extends Control
## Developer inspection view. Displays C++ decoded original data; no game rules.
var native = ZhongyuanOriginalData.new()
var inventory: Dictionary
var details: Label
var officers_box: VBoxContainer
var selected_label: Label

func _ready() -> void:
	var theme_data = Theme.new()
	theme_data.default_font = load("res://assets/chinese.ttf")
	theme_data.default_font_size = 18
	theme_data.set_color("font_color", "Label", Color("20221d"))
	theme = theme_data
	var background = ColorRect.new()
	background.color = Color("f1e6c3")
	background.size = Vector2(1200,760)
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(background)
	text_at("原版数据核对", Vector2(36,24), 32)
	text_at("从指定中文版 ROM 直接读取 · C++ 数据与图块解码 · 此页是开发工具",Vector2(36,72),17)
	var path: String = ""
	var args = OS.get_cmdline_user_args()
	for i in range(args.size()-1):
		if args[i] == "--rom": path = args[i+1]
	if path.is_empty() and FileAccess.file_exists("res://reference/manifest.json"):
		var manifest = JSON.parse_string(FileAccess.get_file_as_string("res://reference/manifest.json"))
		path = manifest.get("local_path", "")
	inventory = native.load_rom(path)
	if inventory.has("error"):
		text_at("无法读取参考 ROM：" + str(inventory.error),Vector2(36,140),20)
		return
	text_at("30 个城池记录 / 241 个武将记录 / 16,384 个原始图块",Vector2(36,120),18)
	text_at("城名和武将名保留 ROM 字形。点击城池查看初始记录。",Vector2(36,153),15)
	for i in range(30):
		var b = Button.new()
		b.position = Vector2(36+(i%5)*130,200+(i/5)*70)
		b.size = Vector2(120,60)
		var style = StyleBoxFlat.new()
		style.bg_color = Color("fff0c3")
		style.border_color = Color("9a8053")
		style.set_border_width_all(1)
		b.add_theme_stylebox_override("normal",style)
		var hover = style.duplicate()
		hover.bg_color = Color("dfca87")
		b.add_theme_stylebox_override("hover",hover)
		b.add_theme_stylebox_override("pressed",hover)
		b.pressed.connect(select_city.bind(i))
		add_child(b)
		var glyph = name_texture(false,i)
		glyph.position = Vector2(12,12)
		b.add_child(glyph)
	selected_label = text_at("",Vector2(730,200),26)
	details = text_at("",Vector2(730,252),19)
	officers_box = VBoxContainer.new()
	officers_box.position = Vector2(730,430)
	officers_box.size = Vector2(440,270)
	officers_box.add_theme_constant_override("separation",10)
	add_child(officers_box)
	text_at("已核对新野、刘备、赵云；其余记录仍需逐项运行验证。",Vector2(36,665),16)
	text_at("未知字段保留原始字节。本工具不等于完整原版重制。",Vector2(36,703),16)
	select_city(13)
	if "--capture" in args:
		await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("res://reference-data-preview.png")
		get_tree().quit()

func text_at(value: String, at: Vector2, font_size: int) -> Label:
	var label = Label.new()
	label.text = value
	label.position = at
	label.add_theme_font_size_override("font_size",font_size)
	add_child(label)
	return label

func name_texture(is_officer: bool, index: int) -> TextureRect:
	var glyph = TextureRect.new()
	glyph.texture = ImageTexture.create_from_image(native.name_image(is_officer,index))
	glyph.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	glyph.custom_minimum_size = Vector2(96,32)
	glyph.size = Vector2(96,32)
	glyph.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	glyph.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return glyph

func select_city(index: int) -> void:
	var c: Dictionary = inventory.cities[index]
	selected_label.text = "%s  /  城池 %02d" % [c.name,index]
	details.text = "黄金  %d\n土地  %d       商业  %d\n人口  %d      统治  %d\nROM 地址  0x%04X\n状态字节  0x%02X（含义待补全）" % [c.gold,c.land,c.commerce,c.population,c.control,c.rom_file_offset,c.flags_raw]
	for child in officers_box.get_children():
		officers_box.remove_child(child)
		child.queue_free()
	for officer_id in c.officer_slots:
		if officer_id == null: continue
		var o: Dictionary = inventory.officers[int(officer_id)]
		var row = HBoxContainer.new()
		row.add_theme_constant_override("separation",10)
		row.add_child(name_texture(true,int(officer_id)))
		var info = Label.new()
		info.add_theme_font_size_override("font_size",14)
		info.text = "体%d 知%d 武%d 德%d\n步%d 骑%d 弓%d" % [o.stamina,o.intelligence,o.martial,o.virtue,o.infantry,o.cavalry,o.archers]
		row.add_child(info)
		officers_box.add_child(row)
