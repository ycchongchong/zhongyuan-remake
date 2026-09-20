extends Control
## UI only: mutable original records, proposals and command effects live in C++.
const SAVE = "user://campaign-native-original.json"
const RULERS = ["袁绍","马腾","曹操","孙权","刘备","刘璋"]
const KINDS = ["土地","商业","人口"]
var native = ZhongyuanOriginalData.new()
var state: Dictionary = {}
var initialized = false
var selected = 13
var chosen: Array[int] = []
var moving = false
var clock_remainder = 0.0
var map_texture = preload("res://assets/original/world-roads.png")
var names = preload("res://assets/original/names.png")
var towns: Array[Texture2D] = []
var header: Label
var city_label: Label
var details: Label
var officer_details: Label
var status: Label
var city_list: OptionButton
var roster: Control
var develop_button: Button
var move_button: Button
var search_button: Button
var search_confirm: ConfirmationDialog
var search_report: ConfirmationDialog
var confirm: ConfirmationDialog
var kinds: PopupMenu
var ending_report: AcceptDialog
var ending_picture: TextureRect
var ending_text: Label
var ending_button: Button
var ending_seen = false
var notice: AcceptDialog
var cursor = Vector2i(136,88)
var officer_book: Window
var officer_list: ItemList
var end_button: Button
var phase_label: Label
var ai_paused = false
var recruit_button: Button
var recruit_dialog: ConfirmationDialog
var recruit_quantity: SpinBox
var army_dialog: AcceptDialog
var army_officers: OptionButton
var army_quantity: SpinBox
var army_status: Label
const ORDER_KEYS=["transport","gift","award_gold","award_weapons","award_people","buy","sell","study","heal","scout"]
const ORDER_TITLES=["运输物资","赠送宝物","赏赐武将黄金","赏赐武将武器","赏赐百姓","购买武器","出售宝物","学习","治疗","侦查地形"]
const ITEM_NAMES=["黄金","武器","宝石","手镯","指环"]
var orders_button:Button
var order_dialog:ConfirmationDialog
var order_kind:OptionButton
var order_target:OptionButton
var order_officer:OptionButton
var order_description:Label
var order_rows:Array=[]
var order_amounts:Array=[]
var scout_dialog:AcceptDialog
var scout_texture:TextureRect
var expedition_button:Button
var expedition_dialog:ConfirmationDialog
var expedition_target:OptionButton
var expedition_leader:OptionButton
var expedition_description:Label
var battle_details:AcceptDialog
var battle_texture:TextureRect
var battle_description:Label
var battle_attackers:ItemList
var battle_defenders:ItemList
var tactical_start:Button
var human_strategy_controls:VBoxContainer
var human_strategy_choices:OptionButton
var human_strategy_target:OptionButton
var human_strategy_prepare:Button
var human_strategy_confirm:Button
var human_strategy_cancel:Button
var human_strategy_finish:Button
var human_strategy_report:Label
const TACTICAL_STRATEGY_NAMES=["火计","陷阱计","水计","要击计","虚兵计","连环计","共杀计","笼络计"]
var computer_next:Button
var tactical_end:Button
var support_controls:HBoxContainer
var formation_label:Label
var formation_previous:Button
var formation_next:Button
var scout_target:OptionButton
var scout_button:Button
var scout_close:Button
var retreat_controls:HBoxContainer
var retreat_button:Button
var retreat_confirm:Button
var retreat_cancel:Button
var retreat_finish:Button
var withdrawal_next:Button
var time_limit_next:Button
var time_limit_identity:TextureRect
var time_limit_row:HBoxContainer
var attack_controls:HBoxContainer
var attack_arrows:Array[Button]=[]
var attack_confirm:Button
var attack_cancel:Button
var clash_step:Button
var clash_strategy_resume:Button
var clash_auto:Button
var clash_timer:Timer
var clash_start:Button
var clash_controls:VBoxContainer
var clash_headings:Array[Label]=[]
var clash_names:Array[TextureRect]=[]
var clash_orders:Array[Button]=[]
var surrender_controls:HBoxContainer
var surrender_identity:HBoxContainer
var surrender_name:TextureRect
var surrender_submit:Button
var surrender_yes:Button
var surrender_no:Button
var surrender_next:Button
var human_failure_controls:HBoxContainer
var human_failure_next:Button
var clash_result_close:Button
var clash_retreat_row:HBoxContainer
var clash_retreat_name:TextureRect
var clash_retreat_next:Button
var clash_defeat_row:HBoxContainer
var clash_defeat_name:TextureRect
var clash_defeat_next:Button
var duel_controls:HBoxContainer
var duel_commands:Array[Button]=[]
var duel_next:Button
var duel_yes:Button
var duel_no:Button
var battle_forces:HBoxContainer
var battle_overlay:Control
var deployment_start:Button
var deployment_confirm:Button
var deployment_arrows:Array[Button]=[]


func initialize(ruler: int, difficulty: int, restore_saved := false, second := -1) -> String:
	var manifest = JSON.parse_string(FileAccess.get_file_as_string("res://reference/manifest.json"))
	var path: String = manifest.get("local_path", "") if manifest is Dictionary else ""
	var args = OS.get_cmdline_user_args()
	for i in range(args.size()-1):
		if args[i] == "--rom": path = args[i+1]
	var imported: Dictionary = native.load_rom(path)
	if imported.has("error"): return "无法读取指定原版文件："+str(imported.error)
	var error: String = native.load_session(SAVE) if restore_saved else native.start_two_player_session(ruler,second,difficulty) if second>=0 else native.start_session(ruler,difficulty)
	if not error.is_empty(): return error
	state = native.session_snapshot()
	selected = int(state.rulers[int(state.player)].seat)
	initialized = true
	return ""

func _ready() -> void:
	OriginalSound.attach_scene(self)
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	var theme_data = Theme.new()
	theme_data.default_font = load("res://assets/chinese.ttf")
	theme_data.default_font_size = 18
	theme = theme_data
	var bg = ColorRect.new()
	bg.color = Color("101d1b")
	bg.size = Vector2(1200,760)
	bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(bg)
	# Drawing overlays on the backdrop child keeps the map behind all controls.
	bg.draw.connect(draw_map.bind(bg))
	for i in range(6): towns.append(load("res://assets/original/town-%d.png" % i))
	label_at("中原之霸者",Vector2(36,20),30)
	OriginalSound.add_mute_button(self, Vector2(215, 27))
	header = label_at("",Vector2(340,33),19)
	ending_report=AcceptDialog.new()
	ending_report.title="战局结束"
	ending_report.ok_button_text="查看终局地图"
	ending_report.add_button("返回标题",true,"title")
	ending_report.custom_action.connect(func(action:StringName):
		if action==&"title":return_to_title())
	add_child(ending_report)
	ending_report.get_label().hide()
	var ending_layout=VBoxContainer.new()
	ending_report.add_child(ending_layout)
	ending_picture=TextureRect.new()
	ending_picture.custom_minimum_size=Vector2(432,288)
	ending_picture.expand_mode=TextureRect.EXPAND_IGNORE_SIZE
	ending_picture.stretch_mode=TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	ending_picture.texture_filter=CanvasItem.TEXTURE_FILTER_NEAREST
	ending_layout.add_child(ending_picture)
	ending_picture.hide()
	ending_text=Label.new()
	ending_text.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
	ending_text.custom_minimum_size.x=540
	ending_layout.add_child(ending_text)
	ending_button=button_at("结局报告",Vector2(680,70),Vector2(125,40),show_ending_report)
	ending_button.hide()
	button_at("存档",Vector2(900,25),Vector2(78,40),save_game)
	button_at("读档",Vector2(990,25),Vector2(78,40),load_game)
	button_at("标题",Vector2(1080,25),Vector2(78,40),return_to_title)
	label_at("原版战局 · 城池与武将",Vector2(36,79),16)
	phase_label = label_at("",Vector2(450,79),16)
	city_label = label_at("",Vector2(844,125),25)
	city_list = OptionButton.new()
	city_list.position = Vector2(844,168)
	city_list.size = Vector2(320,36)
	city_list.item_selected.connect(select_city)
	add_child(city_list)
	details = label_at("",Vector2(844,218),17)
	details.size = Vector2(320,128)
	details.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	officer_details = label_at("",Vector2(844,355),17)
	develop_button = button_at("开发",Vector2(844,530),Vector2(154,42),show_development)
	search_button = button_at("搜索",Vector2(1010,530),Vector2(154,42),show_search)
	move_button = button_at("移动武将",Vector2(844,581),Vector2(154,42),start_move)
	recruit_button = button_at("征兵编成",Vector2(1010,581),Vector2(154,42),show_recruit)
	button_at("武将名册",Vector2(844,635),Vector2(154,38),show_officers)
	orders_button=button_at("物资与人事",Vector2(1010,635),Vector2(154,38),show_orders)
	expedition_button=button_at("出征",Vector2(608,71),Vector2(100,34),show_expedition)
	button_at("重制进度",Vector2(718,71),Vector2(100,34),show_remaining)
	end_button = button_at("结束本回合",Vector2(844,684),Vector2(320,38),end_turn)
	roster = Control.new()
	add_child(roster)
	status = label_at("选择武将后，可开发或沿相邻道路移动。",Vector2(36,733),14)
	confirm = ConfirmationDialog.new()
	confirm.title = "确认开发"
	confirm.ok_button_text = "执行"
	confirm.cancel_button_text = "取消"
	confirm.confirmed.connect(confirm_development)
	confirm.canceled.connect(native.cancel_development)
	add_child(confirm)
	search_confirm = ConfirmationDialog.new()
	search_confirm.title = "派遣搜索"
	search_confirm.ok_button_text = "派遣"
	search_confirm.cancel_button_text = "取消"
	search_confirm.confirmed.connect(dispatch_search)
	add_child(search_confirm)
	search_report = ConfirmationDialog.new()
	search_report.title = "搜索报告"
	search_report.confirmed.connect(finish_search.bind(true))
	search_report.canceled.connect(finish_search.bind(false))
	add_child(search_report)
	setup_expedition()
	setup_orders()
	recruit_dialog=ConfirmationDialog.new()
	recruit_dialog.title="征兵"
	recruit_dialog.ok_button_text="征募"
	recruit_dialog.cancel_button_text="取消"
	recruit_dialog.confirmed.connect(recruit)
	add_child(recruit_dialog)
	var recruit_content=VBoxContainer.new()
	recruit_content.add_theme_constant_override("separation",18)
	recruit_dialog.add_child(recruit_content)
	var recruit_description=Label.new()
	recruit_description.text="每百人二十金，消耗一枚命令书。\n确认后可分配给本城武将。"
	recruit_content.add_child(recruit_description)
	recruit_quantity=SpinBox.new()
	recruit_quantity.custom_minimum_size=Vector2(0,44)
	recruit_quantity.min_value=0
	recruit_quantity.max_value=255
	recruit_quantity.suffix="百人"
	recruit_content.add_child(recruit_quantity)
	army_dialog=AcceptDialog.new()
	army_dialog.title="兵马编成"
	army_dialog.ok_button_text="完成编成"
	army_dialog.confirmed.connect(finish_recruitment)
	army_dialog.canceled.connect(finish_recruitment)
	add_child(army_dialog)
	var army_content=VBoxContainer.new()
	army_content.add_theme_constant_override("separation",16)
	army_dialog.add_child(army_content)
	army_officers=OptionButton.new()
	army_officers.custom_minimum_size=Vector2(0,44)
	army_officers.item_selected.connect(select_army_officer)
	var roster_style=StyleBoxFlat.new()
	roster_style.bg_color=Color("f1e2b4")
	for style_name in ["normal","hover","pressed"]:army_officers.add_theme_stylebox_override(style_name,roster_style)
	army_officers.get_popup().add_theme_stylebox_override("panel",roster_style)
	for color_name in ["font_color","font_hover_color","font_pressed_color"]:army_officers.add_theme_color_override(color_name,Color("211b10"))
	army_officers.get_popup().add_theme_color_override("font_color",Color("211b10"))
	army_content.add_child(army_officers)
	var army_row=HBoxContainer.new()
	army_row.add_theme_constant_override("separation",16)
	army_content.add_child(army_row)
	army_quantity=SpinBox.new()
	army_quantity.custom_minimum_size=Vector2(0,44)
	army_quantity.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	army_quantity.min_value=0
	army_quantity.max_value=10
	army_quantity.suffix="百人"
	army_row.add_child(army_quantity)
	var apply_army=Button.new()
	apply_army.text="应用编成"
	apply_army.custom_minimum_size=Vector2(160,44)
	apply_army.pressed.connect(assign_army)
	army_row.add_child(apply_army)
	army_status=Label.new()
	army_content.add_child(army_status)
	kinds = PopupMenu.new()
	for i in range(3): kinds.add_item("开发"+KINDS[i],i)
	kinds.id_pressed.connect(prepare_development)
	add_child(kinds)
	notice = AcceptDialog.new()
	notice.title = "重制进度"
	add_child(notice)
	officer_book = Window.new()
	officer_book.title = "武将名册 · 241 人"
	officer_book.size = Vector2i(1080,640)
	officer_book.visible = false
	officer_book.close_requested.connect(officer_book.hide)
	add_child(officer_book)
	officer_list = ItemList.new()
	officer_list.theme = theme_data
	officer_list.position = Vector2(8,8)
	officer_list.size = Vector2(1064,624)
	officer_list.max_columns = 3
	officer_list.max_text_lines = 1
	officer_list.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	officer_list.fixed_column_width = 346
	officer_list.fixed_icon_size = Vector2i(96,32)
	officer_list.icon_mode = ItemList.ICON_MODE_LEFT
	officer_list.add_theme_font_size_override("font_size",14)
	officer_list.add_theme_color_override("font_color",Color("211b10"))
	officer_list.add_theme_color_override("font_selected_color",Color("211b10"))
	var paper=StyleBoxFlat.new()
	paper.bg_color=Color("f1e2b4")
	officer_list.add_theme_stylebox_override("panel",paper)
	var highlighted=paper.duplicate()
	highlighted.bg_color=Color("ffd978")
	officer_list.add_theme_stylebox_override("selected",highlighted)
	officer_list.add_theme_stylebox_override("selected_focus",highlighted)
	officer_book.add_child(officer_list)
	if not initialized:
		var error = initialize(4,0)
		if not error.is_empty():
			status.text = error
			return
	for c in state.cities: city_list.add_item(c.name,int(c.id))
	select_city(selected)
	resume_pending()

func label_at(value: String, at: Vector2, font_size: int) -> Label:
	var label = Label.new()
	label.text = value
	label.position = at
	label.add_theme_font_size_override("font_size",font_size)
	add_child(label)
	return label

func button_at(value: String, at: Vector2, size_: Vector2, action: Callable) -> Button:
	var button = Button.new()
	button.text = value
	button.position = at
	button.size = size_
	button.pressed.connect(action)
	add_child(button)
	return button

func draw_map(canvas: CanvasItem) -> void:
	if not initialized: return
	canvas.draw_texture_rect_region(map_texture,Rect2(42,126,768,480),Rect2(0,0,256,160))
	var source: Dictionary = state.cities[selected]
	for c in state.cities:
		var at = Vector2(float(c.map_position[0]),float(c.map_position[1])+1)*3+Vector2(42,126)
		var owner = int(c.faction_id)
		if owner < 6: canvas.draw_texture_rect(towns[owner],Rect2(at,Vector2(24,24)),false)
		if int(c.id)==selected or (moving and c.id in source.neighbors and owner==int(state.current_ruler)):
			canvas.draw_rect(Rect2(at-Vector2(3,3),Vector2(30,30)),Color("f2d368"),false,2)
	var cross = Vector2(cursor)*3+Vector2(42,126)
	canvas.draw_line(cross-Vector2(9,0),cross+Vector2(9,0),Color.WHITE,2)
	canvas.draw_line(cross-Vector2(0,9),cross+Vector2(0,9),Color.WHITE,2)

func select_city(id: int) -> void:
	if not initialized: return
	if moving and id!=selected:
		var error: String = native.move_officers(selected,id,chosen)
		if not error.is_empty(): status.text = error; return
		moving = false
		status.text = "武将已移动；本次消耗一枚命令书。"
	selected = id
	chosen.clear()
	state = native.session_snapshot()
	cursor = Vector2i(int(state.cities[id].map_position[0])+8,int(state.cities[id].map_position[1])+8)
	for officer in state.cities[id].officer_slots:
		if officer!=null:
			chosen.append(int(officer))
			break
	refresh()
	var report: Dictionary = native.visit_city(id)
	if report.has("error"): status.text=report.error
	elif not report.is_empty(): show_search_report(report)

func refresh() -> void:
	state = native.session_snapshot()
	OriginalSound.set_music(native.music_cue())
	var ruler = int(state.current_ruler)
	var c: Dictionary = state.cities[selected]
	header.text = "%d 年 %d 月 · %s · 命令书 %d" % [state.year,state.month,RULERS[ruler],state.rulers[ruler].books]
	phase_label.text = {"player_commands":"玩家指令", "ai_turn":"%s 正在行动…" % RULERS[ruler], "battle":"守城待续", "expedition":"出征待续", "ending":"战局结束"}.get(state.phase, "")
	end_button.disabled = state.phase!="player_commands" or state.pending_development!=null or state.pending_search!=null or state.pending_army!=null
	end_button.text = "进入下一回合" if int(state.rulers[ruler].books)==0 else "结束本回合"
	city_label.text = "%s · %s" % [c.name,RULERS[int(c.faction_id)] if int(c.faction_id)<6 else "无所属"]
	city_list.select(selected)
	details.text = "黄金  %d       后备兵  %d\n土地  %d       商业  %d\n人口  %d      统治  %d\n相邻：%s" % [c.gold,c.reserves,c.land,c.commerce,c.population,c.control,neighbor_names(c)]
	details.text+="\n武器 %d  宝石 %d  手镯 %d  指环 %d" % [c.inventory[1],c.inventory[2],c.inventory[3],c.inventory[4]]
	for child in roster.get_children(): child.queue_free()
	var slot = 0
	for officer in c.officer_slots:
		if officer==null: continue
		var id = int(officer)
		var b = Button.new()
		b.position = Vector2(42+(slot%6)*126,620+(slot/6)*49)
		b.size = Vector2(116,44)
		b.toggle_mode = true
		var normal = StyleBoxFlat.new()
		normal.bg_color = Color("f1e2b4")
		b.add_theme_stylebox_override("normal",normal)
		var active = normal.duplicate()
		active.bg_color = Color("ffe99e")
		active.border_color = Color("e0a12c")
		active.set_border_width_all(3)
		b.add_theme_stylebox_override("pressed",active)
		b.add_theme_stylebox_override("hover",active)
		b.button_pressed = id in chosen
		b.pressed.connect(toggle_officer.bind(id))
		roster.add_child(b)
		var atlas = AtlasTexture.new()
		atlas.atlas = names
		atlas.region = Rect2(0,(30+id)*16,48,16)
		var glyph = TextureRect.new()
		glyph.texture = atlas
		glyph.position = Vector2(10,6)
		glyph.size = Vector2(96,32)
		glyph.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		glyph.mouse_filter = Control.MOUSE_FILTER_IGNORE
		b.add_child(glyph)
		slot+=1
	if chosen.size()>0:
		var o: Dictionary = state.officers[chosen.back()]
		officer_details.text = "选中 %d 名武将\n体 %d    知 %d    武 %d    德 %d\n忠 %s\n步兵 %d    骑兵 %d    弓兵 %d" % [chosen.size(),o.stamina,o.intelligence,o.martial,o.virtue,str(int(o.loyalty)) if o.loyalty!=null else "--",o.infantry,o.cavalry,o.archers]
	else: officer_details.text = "点击下方原版姓名图块选择武将。"
	var allowed = state.phase=="player_commands" and state.pending_army==null and int(c.faction_id)==ruler and int(state.rulers[ruler].books)>0
	expedition_button.disabled=not (state.phase in ["expedition","battle"]) and (not allowed or chosen.is_empty() or state.pending_development!=null or state.pending_search!=null)
	expedition_button.text="查看战场" if state.phase in ["expedition","battle"] else "出征"
	orders_button.disabled=not allowed or state.pending_development!=null or state.pending_search!=null
	recruit_button.disabled=not allowed
	develop_button.disabled = not allowed or chosen.size()!=1
	move_button.disabled = not allowed or chosen.is_empty()
	search_button.disabled = not allowed or chosen.size()!=1 or (chosen.size()==1 and chosen[0]<6) or slot<2
	move_button.text = "取消移动" if moving else "移动武将"
	get_child(0).queue_redraw()
	ending_button.visible=state.phase=="ending"
	if state.phase=="ending":
		moving=false
		if not ending_seen:
			ending_seen=true
			battle_details.hide()
			show_ending_report()
	else:
		ending_seen=false
		ending_report.hide()

func show_ending_report() -> void:
	if state.phase!="ending":return
	var text=""
	ending_picture.hide()
	ending_picture.texture=null
	match state.ending.kind:
		"year_limit":text="公元 250 年，仍未统一天下。\n战局已达到年限。"
		"unification":text="%s 已统一全部 30 座城池。" % RULERS[int(state.ending.winner)]
		"players_eliminated":text="玩家势力均已灭亡。"
	if state.ending.kind=="unification" and state.get("unification") is Dictionary:
		var summary:Dictionary=state.unification
		if summary.has("score") and summary.has("variant"):
			text+="\n统治度：%d　驻城武将：%d" % [int(summary.score),int(summary.residents)]
			var original=Image.load_from_file("res://assets/original/endings/%d.png" % int(summary.variant))
			if original!=null:
				ending_picture.texture=ImageTexture.create_from_image(original)
				ending_picture.show()
	ending_report.dialog_text="%d 年 %d 月\n\n%s\n\n可查看终局地图、保存战局或返回标题。" % [state.year,state.month,text]
	ending_text.text=ending_report.dialog_text
	ending_report.popup_centered(Vector2i(580,560 if ending_picture.visible else 260))

func neighbor_names(c: Dictionary) -> String:
	var items = PackedStringArray()
	for id in c.neighbors: items.append(state.cities[int(id)].name)
	return "、".join(items)

func toggle_officer(id: int) -> void:
	if moving: return
	if id in chosen: chosen.erase(id)
	else: chosen.append(id)
	refresh()

func start_move() -> void:
	moving = not moving
	status.text = "选择相邻的己方城池；出发城须至少留一名武将。" if moving else "已取消移动。"
	refresh()

func show_development() -> void:
	if moving: moving=false;refresh()
	kinds.popup_centered(Vector2i(260,140))

func prepare_development(kind: int) -> void:
	if chosen.size()!=1: return
	var offer: Dictionary = native.prepare_development(selected,chosen[0],kind)
	if offer.has("error"): status.text=offer.error;return
	confirm.dialog_text = "开发%s\n费用：%d 金\n消耗一枚命令书。" % [KINDS[kind],offer.cost]
	confirm.popup_centered(Vector2i(440,190))

func confirm_development() -> void:
	var error: String = native.confirm_development()
	if not error.is_empty():
		status.text=error
		native.cancel_development()
	else: status.text="开发完成，城池数值已更新。"
	refresh()

func show_search() -> void:
	if moving: moving=false;refresh()
	search_confirm.dialog_text="派遣选中武将搜索，消耗一枚命令书。\n武将会暂时离开城池。\n返回月份后，进入派遣城查看搜索报告。"
	search_confirm.popup_centered(Vector2i(610,210))

func dispatch_search() -> void:
	if chosen.size()!=1: return
	var error: String = native.search(selected,chosen[0])
	if not error.is_empty(): status.text=error;return
	chosen.clear()
	refresh()
	status.text="武将已出发搜索；派遣记录随原版战局保存。"

func show_search_report(report: Dictionary) -> void:
	var kind=int(report.kind)
	var text="武将 #%03d 返回%s。\n" % [report.officer,state.cities[int(report.city)].name]
	if kind<3: text+="带回黄金 %d。" % report.value
	elif kind<6: text+="带回物品 %d 件。" % report.value
	elif kind==6: text+="发现武将 #%03d，愿以 %d 金加入。" % [report.candidate,report.value]
	elif kind==7: text+="发现武将 #%03d，是否劝说加入？" % report.candidate
	elif kind==8: text+="武将 #%03d 愿意加入。" % report.candidate
	else: text+="本次未发现物资或武将。"
	search_report.dialog_text=text
	search_report.ok_button_text="招募" if kind==6 else "劝说" if kind==7 else "确定"
	search_report.cancel_button_text="放弃"
	search_report.get_cancel_button().visible=kind in [6,7]
	if not search_report.visible: search_report.popup_centered(Vector2i(610,210))
	refresh()

func finish_search(accept: bool) -> void:
	var result: Dictionary = native.finish_search(accept)
	if result.has("error"): status.text=result.error;return
	status.text="搜索报告已处理。"
	if result.has("joined"):
		status.text="武将已加入本城。" if result.joined else "武将未加入；城池满员、黄金不足或劝说失败均无法招募。"
	refresh()

func save_game() -> void:
	var error: String = native.save_session(SAVE)
	status.text = "原版战局已保存。" if error.is_empty() else error

func load_game() -> void:
	var error: String = native.load_session(SAVE)
	if not error.is_empty(): status.text=error;return
	moving=false
	ai_paused=false
	ending_seen=false
	state=native.session_snapshot()
	if state.phase=="player_commands": select_current_seat()
	elif state.phase=="battle":select_city(int(state.battle.target))
	else:select_city(selected)
	resume_pending()
	status.text="已读取原版战局。"

func resume_pending() -> void:
	if state.pending_army!=null:
		show_army()
		return
	if state.get("pending_search")!=null:
		show_search_report(state.pending_search)
		return
	var pending = state.pending_development
	if pending==null: return
	selected=int(pending.city)
	cursor=Vector2i(int(state.cities[selected].map_position[0])+8,int(state.cities[selected].map_position[1])+8)
	chosen.assign([int(pending.officer)])
	refresh()
	confirm.dialog_text="开发%s\n费用：%d 金\n消耗一枚命令书。" % [KINDS[int(pending.kind)],pending.cost]
	confirm.popup_centered(Vector2i(440,190))

func return_to_title() -> void:
	get_tree().change_scene_to_file("res://opening.tscn")

func setup_expedition() -> void:
	expedition_dialog=ConfirmationDialog.new()
	expedition_dialog.title="出征"
	expedition_dialog.ok_button_text="确认出征"
	expedition_dialog.cancel_button_text="取消"
	expedition_dialog.confirmed.connect(dispatch_expedition)
	add_child(expedition_dialog)
	var content=VBoxContainer.new()
	content.add_theme_constant_override("separation",16)
	expedition_dialog.add_child(content)
	expedition_target=OptionButton.new()
	expedition_target.custom_minimum_size=Vector2(600,40)
	expedition_target.item_selected.connect(update_expedition_quote)
	content.add_child(expedition_target)
	expedition_leader=OptionButton.new()
	expedition_leader.custom_minimum_size=Vector2(600,40)
	expedition_leader.item_selected.connect(update_expedition_quote)
	var paper=StyleBoxFlat.new()
	paper.bg_color=Color("f1e2b4")
	for key in ["normal","hover","pressed"]:expedition_leader.add_theme_stylebox_override(key,paper)
	expedition_leader.get_popup().add_theme_stylebox_override("panel",paper)
	content.add_child(expedition_leader)
	expedition_description=Label.new()
	expedition_description.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
	expedition_description.custom_minimum_size=Vector2(600,150)
	content.add_child(expedition_description)
	battle_details=AcceptDialog.new()
	battle_details.ok_button_text="返回"
	battle_details.custom_action.connect(deployment_action)
	battle_details.window_input.connect(deployment_input)
	tactical_start=battle_details.add_button("进入战术移动",false,"tactics")
	tactical_end=battle_details.add_button("结束本方行动",false,"end_tactical_turn")
	deployment_start=battle_details.add_button("开始布阵",false,"prepare")
	deployment_confirm=battle_details.add_button("确认位置",false,"confirm")
	for item in [["→","0"],["↓","2"],["↑","3"],["←","1"]]:
		deployment_arrows.push_front(battle_details.add_button(item[0],false,item[1]))
	battle_details.add_button("存档",false,"save")
	add_child(battle_details)
	var battle_content=VBoxContainer.new()
	battle_content.add_theme_constant_override("separation",12)
	battle_details.add_child(battle_content)
	battle_texture=TextureRect.new()
	battle_texture.custom_minimum_size=Vector2(640,320)
	battle_texture.expand_mode=TextureRect.EXPAND_IGNORE_SIZE
	battle_texture.stretch_mode=TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	battle_texture.texture_filter=CanvasItem.TEXTURE_FILTER_NEAREST
	battle_content.add_child(battle_texture)
	battle_overlay=preload("res://scripts/deployment_map.gd").new()
	battle_overlay.mouse_filter=Control.MOUSE_FILTER_IGNORE
	battle_texture.add_child(battle_overlay)
	battle_overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	support_controls=HBoxContainer.new()
	support_controls.add_theme_constant_override("separation",8)
	battle_content.add_child(support_controls)
	formation_previous=Button.new()
	formation_previous.text="阵容 ←"
	formation_previous.pressed.connect(deployment_action.bind("formation_previous"))
	support_controls.add_child(formation_previous)
	formation_label=Label.new()
	support_controls.add_child(formation_label)
	formation_next=Button.new()
	formation_next.text="→"
	formation_next.pressed.connect(deployment_action.bind("formation_next"))
	support_controls.add_child(formation_next)
	scout_target=OptionButton.new()
	scout_target.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	support_controls.add_child(scout_target)
	scout_button=Button.new()
	scout_button.text="侦察（1机动力）"
	scout_button.pressed.connect(deployment_action.bind("scout"))
	support_controls.add_child(scout_button)
	scout_close=Button.new()
	scout_close.text="关闭侦察报告"
	scout_close.pressed.connect(deployment_action.bind("close_scout"))
	support_controls.add_child(scout_close)
	attack_controls=HBoxContainer.new()
	attack_controls.add_theme_constant_override("separation",8)
	battle_content.add_child(attack_controls)
	for direction in [["攻击 →",0],["攻击 ←",1],["攻击 ↓",2],["攻击 ↑",3]]:
		var button=Button.new()
		button.text=direction[0]
		button.size_flags_horizontal=Control.SIZE_EXPAND_FILL
		button.pressed.connect(deployment_action.bind("attack_"+str(direction[1])))
		attack_controls.add_child(button)
		attack_arrows.append(button)
	retreat_button=Button.new()
	retreat_button.text="撤退"
	retreat_button.pressed.connect(deployment_action.bind("retreat"))
	attack_controls.add_child(retreat_button)
	human_strategy_controls=VBoxContainer.new()
	battle_content.add_child(human_strategy_controls)
	human_strategy_report=Label.new()
	human_strategy_report.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
	human_strategy_controls.add_child(human_strategy_report)
	var strategy_row=HBoxContainer.new()
	human_strategy_controls.add_child(strategy_row)
	human_strategy_choices=OptionButton.new()
	strategy_row.add_child(human_strategy_choices)
	human_strategy_target=OptionButton.new()
	strategy_row.add_child(human_strategy_target)
	for action in ["strategy_prepare","strategy_confirm","strategy_cancel","strategy_finish"]:
		var button=Button.new()
		button.pressed.connect(deployment_action.bind(action))
		strategy_row.add_child(button)
		match action:
			"strategy_prepare":human_strategy_prepare=button;button.text="施计"
			"strategy_confirm":human_strategy_confirm=button
			"strategy_cancel":human_strategy_cancel=button;button.text="取消"
			"strategy_finish":human_strategy_finish=button;button.text="确认计策结果"
	retreat_controls=HBoxContainer.new()
	retreat_controls.add_theme_constant_override("separation",8)
	battle_content.add_child(retreat_controls)
	retreat_confirm=Button.new()
	retreat_confirm.text="确认撤退（1机动力）"
	retreat_confirm.pressed.connect(deployment_action.bind("retreat_confirm"))
	retreat_controls.add_child(retreat_confirm)
	retreat_cancel=Button.new()
	retreat_cancel.text="取消"
	retreat_cancel.pressed.connect(deployment_action.bind("retreat_cancel"))
	retreat_controls.add_child(retreat_cancel)
	retreat_finish=Button.new()
	retreat_finish.text="确认结果"
	retreat_finish.pressed.connect(deployment_action.bind("retreat_finish"))
	retreat_controls.add_child(retreat_finish)
	withdrawal_next=Button.new()
	withdrawal_next.pressed.connect(deployment_action.bind("withdrawal_result"))
	battle_content.add_child(withdrawal_next)
	time_limit_row=HBoxContainer.new()
	time_limit_row.alignment=BoxContainer.ALIGNMENT_CENTER
	var time_name_panel=PanelContainer.new()
	var time_name_style=StyleBoxFlat.new()
	time_name_style.bg_color=Color("efe3b7")
	time_name_panel.add_theme_stylebox_override("panel",time_name_style)
	time_limit_row.add_child(time_name_panel)
	time_limit_identity=TextureRect.new()
	time_limit_identity.custom_minimum_size=Vector2(96,32)
	time_limit_identity.texture_filter=CanvasItem.TEXTURE_FILTER_NEAREST
	time_limit_identity.stretch_mode=TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	time_name_panel.add_child(time_limit_identity)
	battle_content.add_child(time_limit_row)
	time_limit_next=Button.new()
	time_limit_next.pressed.connect(deployment_action.bind("time_limit_result"))
	battle_content.add_child(time_limit_next)
	attack_confirm=Button.new()
	attack_confirm.text="完成交战准备"
	attack_confirm.pressed.connect(deployment_action.bind("attack_confirm"))
	attack_controls.add_child(attack_confirm)
	attack_cancel=Button.new()
	attack_cancel.text="取消攻击（不退机动力）"
	attack_cancel.pressed.connect(deployment_action.bind("attack_cancel"))
	attack_controls.add_child(attack_cancel)
	clash_start=Button.new()
	clash_start.text="进入交战军令"
	clash_start.pressed.connect(deployment_action.bind("begin_clash"))
	attack_controls.add_child(clash_start)
	clash_step=Button.new()
	clash_step.text="推进交锋"
	clash_step.pressed.connect(deployment_action.bind("clash_step"))
	attack_controls.add_child(clash_step)
	clash_strategy_resume=Button.new()
	clash_strategy_resume.text="继续电脑军令"
	clash_strategy_resume.pressed.connect(deployment_action.bind("clash_strategy_resume"))
	attack_controls.add_child(clash_strategy_resume)
	clash_auto=Button.new()
	clash_auto.text="自动推进"
	clash_auto.pressed.connect(func():
		if clash_timer.is_stopped():clash_timer.start()
		else:clash_timer.stop()
		clash_auto.text="暂停交锋" if not clash_timer.is_stopped() else "自动推进")
	attack_controls.add_child(clash_auto)
	clash_timer=Timer.new()
	clash_timer.wait_time=0.12
	clash_timer.timeout.connect(deployment_action.bind("clash_step"))
	add_child(clash_timer)
	battle_details.visibility_changed.connect(func():
		if not battle_details.visible:
			clash_timer.stop()
			clash_auto.text="自动推进")
	clash_controls=VBoxContainer.new()
	clash_controls.add_theme_constant_override("separation",8)
	battle_content.add_child(clash_controls)
	for side in range(2):
		var row=HBoxContainer.new()
		row.add_theme_constant_override("separation",8)
		clash_controls.add_child(row)
		var heading=Label.new()
		row.add_child(heading)
		clash_headings.append(heading)
		var name_view=TextureRect.new()
		name_view.custom_minimum_size=Vector2(72,24)
		name_view.expand_mode=TextureRect.EXPAND_IGNORE_SIZE
		name_view.stretch_mode=TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		var name_paper=PanelContainer.new()
		name_paper.add_theme_stylebox_override("panel",paper)
		name_paper.add_child(name_view)
		row.add_child(name_paper)
		clash_names.append(name_view)
		for kind in range(4):
			var button=Button.new()
			button.size_flags_horizontal=Control.SIZE_EXPAND_FILL
			button.pressed.connect(deployment_action.bind("clash_%d_%d" % [side,kind]))
			row.add_child(button)
			clash_orders.append(button)
	clash_retreat_row=HBoxContainer.new()
	clash_retreat_row.add_theme_constant_override("separation",8)
	battle_content.add_child(clash_retreat_row)
	var retreat_title=Label.new()
	retreat_title.text="撤离武将"
	clash_retreat_row.add_child(retreat_title)
	clash_retreat_name=TextureRect.new()
	clash_retreat_name.custom_minimum_size=Vector2(72,24)
	clash_retreat_name.expand_mode=TextureRect.EXPAND_IGNORE_SIZE
	clash_retreat_name.stretch_mode=TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	var retreat_paper=PanelContainer.new()
	retreat_paper.add_theme_stylebox_override("panel",paper)
	retreat_paper.add_child(clash_retreat_name)
	clash_retreat_row.add_child(retreat_paper)
	clash_retreat_next=Button.new()
	clash_retreat_next.text="继续"
	clash_retreat_next.pressed.connect(deployment_action.bind("clash_retreat_next"))
	clash_retreat_row.add_child(clash_retreat_next)
	clash_defeat_row=HBoxContainer.new()
	clash_defeat_row.add_theme_constant_override("separation",8)
	battle_content.add_child(clash_defeat_row)
	var defeat_title=Label.new()
	defeat_title.text="败北武将"
	clash_defeat_row.add_child(defeat_title)
	clash_defeat_name=TextureRect.new()
	clash_defeat_name.custom_minimum_size=Vector2(72,24)
	clash_defeat_name.expand_mode=TextureRect.EXPAND_IGNORE_SIZE
	clash_defeat_name.stretch_mode=TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	var defeat_paper=PanelContainer.new()
	defeat_paper.add_theme_stylebox_override("panel",paper)
	defeat_paper.add_child(clash_defeat_name)
	clash_defeat_row.add_child(defeat_paper)
	clash_defeat_next=Button.new()
	clash_defeat_next.text="继续"
	clash_defeat_next.pressed.connect(deployment_action.bind("clash_defeat_next"))
	clash_defeat_row.add_child(clash_defeat_next)
	duel_controls=HBoxContainer.new()
	duel_controls.add_theme_constant_override("separation",8)
	battle_content.add_child(duel_controls)
	for command in range(5):
		var button=Button.new()
		button.text=["牵制","攻击","舍命一击","后退","投降"][command]
		button.pressed.connect(deployment_action.bind("duel_command_%d" % command))
		duel_controls.add_child(button)
		duel_commands.append(button)
	for action in ["duel_next","duel_yes","duel_no"]:
		var button=Button.new()
		button.text="确认投降" if action=="duel_yes" else "取消" if action=="duel_no" else "继续单挑"
		button.pressed.connect(deployment_action.bind(action))
		duel_controls.add_child(button)
		match action:
			"duel_next":duel_next=button
			"duel_yes":duel_yes=button
			"duel_no":duel_no=button
	surrender_controls=HBoxContainer.new()
	surrender_controls.add_theme_constant_override("separation",8)
	battle_content.add_child(surrender_controls)
	surrender_identity=HBoxContainer.new()
	surrender_identity.add_theme_constant_override("separation",8)
	surrender_controls.add_child(surrender_identity)
	var surrender_label=Label.new()
	surrender_label.text="投降武将"
	surrender_identity.add_child(surrender_label)
	var identity_paper=PanelContainer.new()
	identity_paper.add_theme_stylebox_override("panel",paper)
	surrender_identity.add_child(identity_paper)
	surrender_name=TextureRect.new()
	surrender_name.custom_minimum_size=Vector2(72,24)
	surrender_name.expand_mode=TextureRect.EXPAND_IGNORE_SIZE
	surrender_name.stretch_mode=TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	identity_paper.add_child(surrender_name)
	for item in [["提交投降军令","surrender_submit"],["确认投降","surrender_yes"],["取消","surrender_no"],["继续","surrender_next"],["返回战术行动","result_close"]]:
		var button=Button.new()
		button.text=item[0]
		button.pressed.connect(deployment_action.bind(item[1]))
		surrender_controls.add_child(button)
		match item[1]:
			"surrender_submit":surrender_submit=button
			"surrender_yes":surrender_yes=button
			"surrender_no":surrender_no=button
			"surrender_next":surrender_next=button
			"result_close":clash_result_close=button
	human_failure_controls=HBoxContainer.new()
	human_failure_controls.add_theme_constant_override("separation",8)
	battle_content.add_child(human_failure_controls)
	human_failure_next=Button.new()
	human_failure_next.pressed.connect(deployment_action.bind("human_failure_next"))
	human_failure_controls.add_child(human_failure_next)
	var failure_title=Button.new()
	failure_title.text="返回标题"
	failure_title.pressed.connect(return_to_title)
	human_failure_controls.add_child(failure_title)
	computer_next=Button.new()
	computer_next.text="推进电脑战术行动"
	computer_next.pressed.connect(deployment_action.bind("computer_tactics"))
	battle_content.add_child(computer_next)
	var forces=HBoxContainer.new()
	battle_forces=forces
	forces.add_theme_constant_override("separation",16)
	battle_content.add_child(forces)
	for side in range(2):
		var column=VBoxContainer.new()
		column.size_flags_horizontal=Control.SIZE_EXPAND_FILL
		forces.add_child(column)
		var heading=Label.new()
		heading.text="进攻部队（蓝）" if side==0 else "守城部队（红）"
		column.add_child(heading)
		var list=ItemList.new()
		list.custom_minimum_size=Vector2(300,105)
		list.max_columns=2
		list.fixed_column_width=150
		list.add_theme_stylebox_override("panel",paper)
		list.add_theme_color_override("font_color",Color("201e18"))
		list.add_theme_color_override("font_selected_color",Color("201e18"))
		column.add_child(list)
		if side==0:
			battle_attackers=list
		else:battle_defenders=list
		list.item_selected.connect(select_tactical_unit.bind(side))
	battle_description=Label.new()
	battle_description.custom_minimum_size=Vector2(640,95)
	battle_description.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
	battle_content.add_child(battle_description)

func show_expedition() -> void:
	if state.phase in ["expedition","battle"]:
		show_battle_details()
		return
	if expedition_button.disabled:return
	expedition_target.clear()
	for id in state.cities[selected].neighbors:
		if int(state.cities[int(id)].faction_id)!=int(state.current_ruler):expedition_target.add_item(state.cities[int(id)].name,int(id))
	expedition_leader.clear()
	for id in chosen:
		var icon=AtlasTexture.new()
		icon.atlas=names
		icon.region=Rect2(0,(30+int(id))*16,48,16)
		expedition_leader.add_icon_item(icon,"",int(id))
	update_expedition_quote()
	expedition_dialog.popup_centered(Vector2i(660,370))

func update_expedition_quote(_index: int = 0) -> void:
	if expedition_target.item_count==0 or expedition_leader.item_count==0:
		expedition_description.text="请先选择出征武将，并选择相邻的其他势力城池。"
		expedition_dialog.get_ok_button().disabled=true
		return
	var quote:Dictionary=native.expedition_quote(selected,expedition_target.get_selected_id(),chosen,expedition_leader.get_selected_id())
	expedition_dialog.get_ok_button().disabled=quote.has("error")
	if quote.has("error"):
		expedition_description.text=quote.error
		return
	expedition_description.text="目标：%s\n出征 %d 名武将，支付 %d 金，消耗一枚命令书。\n上方选择主将；出发城须留将，出征扣减统治度。\n\n确认后进入布阵、战术行动与交锋，可保存进度。" % [state.cities[int(quote.target)].name,chosen.size(),int(quote.cost)]

func dispatch_expedition() -> void:
	var result:Dictionary=native.dispatch_expedition(selected,expedition_target.get_selected_id(),chosen,expedition_leader.get_selected_id())
	if result.has("error"):
		status.text=result.error
		return
	chosen.clear()
	moving=false
	refresh()
	status.text="出征部队已集结，可开始布阵并保存进度。"
	show_battle_details()

func show_battle_details() -> void:
	if state.battle==null:return
	var battle:Dictionary=state.battle
	battle_details.title=state.cities[int(battle.source)].name+" → "+state.cities[int(battle.target)].name
	var board:Image=native.tactical_image() if battle.has("deployment") else native.battlefield_image(int(battle.target))
	battle_texture.texture=ImageTexture.create_from_image(board)
	battle_overlay.update_battle(state)
	var deploying:bool=battle.has("deployment")
	var active:bool=deploying and int(battle.deployment.current)>=0
	var tactical:bool=battle.has("tactics")
	var acting_side:int=0 if not tactical or int(battle.tactics.get("side",128))==128 else 1
	var tactical_ready:bool=tactical and not battle.tactics.has("turn_boundary") and not battle.tactics.has("human_strategy")
	var pending_retreat:bool=tactical and battle.tactics.has("retreat")
	computer_next.visible=tactical and battle.tactics.get("turn_boundary","")=="computer" and not pending_retreat and not battle.tactics.has("attack") and not battle.tactics.has("scout")
	var computer_planning:bool=tactical and battle.tactics.get("computer",{}).get("kind","")=="plan"
	var motion:Dictionary=battle.tactics.get("computer_motion",{}) if tactical else {}
	var can_flank:bool=motion.get("kind","")=="pending" and motion.get("branch","") in ["fort_flank","cached_target"]
	var can_retry:bool=motion.get("kind","")=="retry_strategy"
	var can_scan:bool=motion.get("kind","")=="scan"
	var can_nearby:bool=motion.get("kind","")=="pending" and motion.get("branch","")=="role" and (int(motion.get("role",0))&1)!=0 and int(motion.get("role",0))!=15
	computer_next.disabled=computer_next.visible and computer_planning and ((battle.tactics.has("computer_strategy") and battle.tactics.computer_strategy.kind not in ["strategy","no_strategy","role_reassignment"]) or (battle.tactics.has("computer_motion") and not can_flank and not can_retry and not can_scan and not can_nearby) or (int(battle.tactics.computer.get("role_slot",-1))>=0 and not native.can_continue_computer_role()))
	computer_next.text="后续行动待接入" if computer_next.disabled else ("评估电脑计策与目标" if battle.tactics.has("computer_plan") else "选择部队并检查城堡补位") if computer_planning else "推进电脑战术行动"
	if computer_planning and not computer_next.disabled and battle.tactics.get("computer_strategy",{}).get("kind","")=="no_strategy":computer_next.text="继续电脑移动与攻击"
	if computer_planning and battle.tactics.has("computer_strategy") and battle.tactics.computer_strategy.kind=="strategy":computer_next.text="确认计策结果" if battle.tactics.has("strategy_result") else "执行电脑计策"
	if computer_planning and battle.tactics.get("computer_strategy",{}).get("kind","")=="role_reassignment":computer_next.text="应对敌军占堡"
	if computer_planning and int(battle.tactics.computer.get("role_slot",-1))>=0 and not computer_next.disabled:computer_next.text="继续角色调整后的行动"
	if computer_planning and not computer_next.disabled:
		if can_flank:computer_next.text="继续电脑绕行"
		elif can_retry:computer_next.text="检查补充计策"
		elif can_scan:computer_next.text="检查下一部队"
		elif can_nearby:computer_next.text="搜索附近敌军"
		elif battle.tactics.has("computer_reuse"):computer_next.text="继续同一部队行动"
	if computer_next.visible and state.phase=="battle":
		var step:String=battle.tactics.get("attacker_ai",{}).get("stage","assess")
		computer_next.disabled=false
		var labels:Dictionary={"assess":"评估进攻军行动","select":"选择进攻部队","strategy_check":"评估进攻军计策","strategy_execute":"执行进攻军计策","strategy_result":"确认进攻军计策结果","motion":"执行进攻军移动与攻击","scan":"检查下一支进攻部队","finished":"推进电脑进攻回合","held_retreat":"执行电脑进攻军撤退"}
		computer_next.text=labels.get(step,"推进电脑进攻")
	var withdrawal_result:bool=tactical and battle.tactics.get("turn_boundary","")=="battle_result" and int(battle.tactics.get("turn_reason",0)) in [2,3,20,36,131,148]
	withdrawal_next.visible=withdrawal_result
	var time_limited:bool=tactical and battle.tactics.get("turn_boundary","")=="time_limit"
	time_limit_next.visible=time_limited
	var time_notice:Dictionary=battle.tactics.get("time_limit",{}) if tactical else {}
	var time_stage:int=int(time_notice.get("stage",0))
	time_limit_next.text=["查看战斗限时报告","继续限时报告","确认撤军，开始战后安置"][clampi(time_stage,0,2)]
	time_limit_identity.visible=time_limited and time_notice.has("speaker")
	time_limit_row.visible=time_limit_identity.visible
	if time_limit_identity.visible:
		var identity=AtlasTexture.new()
		identity.atlas=names
		identity.region=Rect2(0,(30+int(time_notice.speaker))*16,48,16)
		time_limit_identity.texture=identity
	var result_reason:int=(int(battle.tactics.get("turn_reason",0))&127) if tactical else 0
	var ruler_result:bool=tactical and int(battle.tactics.get("turn_reason",0)) in [131,148]
	var settlement_stage:int=int(battle.tactics.get("settlement",{}).get("stage",0 if result_reason==20 else 14 if result_reason==36 else 24 if result_reason==3 else 9)) if withdrawal_result else 9
	withdrawal_next.text={0:"继续：确认进攻主将离场",1:"继续：安置守方收押俘虏",2:"继续：收回进攻方收押俘虏",3:"继续：剩余进攻军返回来源城",4:"确认战果，返回战略地图",9:"继续：进攻军入城",10:"继续：安置守方收押俘虏",11:"继续：安置进攻方收押俘虏",12:"继续：结算城池",13:"确认战果，返回战略地图",14:"继续：进攻军返回来源城",15:"继续：守方俘虏入城",16:"继续：进攻方俘虏返回来源城",17:"确认战果，返回战略地图",24:"继续：处理守城名册",25:"继续：进攻军入城",26:"继续：安置进攻方俘虏",27:"继续：收回守方俘虏",28:"继续：结算城池",29:"确认战果，返回战略地图"}.get(settlement_stage,"继续战后结算")
	if ruler_result and settlement_stage in [4,29]:
		var next_city:int=int(battle.tactics.get("annexation",{}).get("next_city",0))
		withdrawal_next.text="确认接收结果，返回战略地图" if next_city==30 else "继续：核对%s归属" % state.cities[next_city].name
	retreat_controls.visible=pending_retreat
	retreat_confirm.visible=pending_retreat and battle.tactics.retreat.stage=="confirm"
	retreat_cancel.visible=retreat_confirm.visible
	retreat_finish.visible=pending_retreat and battle.tactics.retreat.stage=="result"
	retreat_button.disabled=not tactical or int(battle.tactics.points)==0
	var pending_attack:bool=tactical and battle.tactics.has("attack")
	var clash_stage:String=battle.tactics.attack.stage if pending_attack else ""
	var in_duel:bool=clash_stage.begins_with("duel_")
	var duel_entry:bool=clash_stage=="clash_boundary" and battle.tactics.attack.get("boundary","")=="duel"
	var in_clash:bool=in_duel or clash_stage in ["clash_orders","clash_running","clash_boundary","clash_retreat","clash_defeat","surrender_confirm","surrender_notice","surrender_accepted"]
	duel_controls.visible=in_duel or duel_entry
	var duel_side:int=int(battle.tactics.attack.duel.side) if in_duel else 0
	var duel_human:bool=in_duel and int(battle.tactics.attack.clash.players[1 if duel_side else 0])!=0
	for command in range(5):
		duel_commands[command].visible=clash_stage=="duel_orders"
		duel_commands[command].disabled=not duel_human
		if in_duel and command==4:duel_commands[command].visible=clash_stage=="duel_orders" and int(battle.tactics.attack.clash.second if duel_side else battle.tactics.attack.clash.first)>=6
	duel_next.visible=duel_entry or (in_duel and clash_stage!="duel_surrender_confirm" and (clash_stage!="duel_orders" or not duel_human))
	duel_next.text="进入单挑" if duel_entry else "电脑出招" if clash_stage=="duel_orders" else "继续"
	duel_yes.visible=clash_stage=="duel_surrender_confirm"
	duel_no.visible=duel_yes.visible
	clash_step.visible=clash_stage in ["clash_orders","clash_running"]
	clash_strategy_resume.visible=clash_stage=="clash_boundary" and battle.tactics.attack.get("boundary","")=="ai_scratch"
	clash_auto.visible=clash_step.visible
	if not clash_step.visible:
		clash_timer.stop()
		clash_auto.text="自动推进"
	var retreat_entry:bool=clash_stage=="clash_boundary" and battle.tactics.attack.get("boundary","")=="retreat"
	clash_retreat_row.visible=retreat_entry or (pending_attack and battle.tactics.attack.has("retreat"))
	clash_retreat_next.visible=retreat_entry or clash_stage=="clash_retreat"
	if clash_retreat_row.visible:
		var retiring:Dictionary=battle.tactics.attack
		var who:int=int(retiring.clash.first if int(retiring.clash.runtime.active)==0 else retiring.clash.second)
		if retiring.has("retreat"):who=int(retiring.retreat.officer)
		var retreat_identity=AtlasTexture.new()
		retreat_identity.atlas=names
		retreat_identity.region=Rect2(0,(30+who)*16,48,16)
		clash_retreat_name.texture=retreat_identity
	var defeat_entry:bool=clash_stage=="clash_boundary" and battle.tactics.attack.get("boundary","")=="general_defeat"
	clash_defeat_row.visible=defeat_entry or (pending_attack and battle.tactics.attack.has("defeat"))
	clash_defeat_next.visible=defeat_entry or clash_stage=="clash_defeat"
	if clash_defeat_row.visible:
		var defeated:Dictionary=battle.tactics.attack
		var who:int=int(defeated.clash.first if (int(defeated.clash.runtime.active)&128)!=0 else defeated.clash.second)
		if defeated.has("defeat"):who=int(defeated.defeat.officer)
		var defeat_identity=AtlasTexture.new()
		defeat_identity.atlas=names
		defeat_identity.region=Rect2(0,(30+who)*16,48,16)
		clash_defeat_name.texture=defeat_identity
	surrender_controls.visible=in_clash or clash_stage=="clash_result"
	surrender_identity.visible=pending_attack and battle.tactics.attack.has("surrender") and clash_stage in ["surrender_confirm","surrender_notice","surrender_accepted","clash_result"]
	if surrender_identity.visible:
		var identity=AtlasTexture.new()
		identity.atlas=names
		identity.region=Rect2(0,(30+int(battle.tactics.attack.surrender.officer))*16,48,16)
		surrender_name.texture=identity
	surrender_submit.visible=clash_stage=="clash_orders"
	surrender_yes.visible=clash_stage=="surrender_confirm"
	surrender_no.visible=surrender_yes.visible
	surrender_next.visible=clash_stage in ["surrender_notice","surrender_accepted"]
	clash_result_close.visible=clash_stage=="clash_result"
	if clash_result_close.visible:
		clash_result_close.disabled=not battle.tactics.attack.result.can_continue and not battle.get("army_defeat_result_available",false) and not battle.get("commander_defeat_result_available",false) and not battle.get("ruler_defeat_result_available",false) and not battle.get("human_failure_available",false)
		clash_result_close.text="查看君主失败报告" if battle.get("human_failure_available",false) else "确认君主败北战果" if battle.get("ruler_defeat_result_available",false) else "确认主将离场战果" if battle.get("commander_defeat_result_available",false) else "确认全军战果" if battle.get("army_defeat_result_available",false) else "返回战术行动"
	clash_controls.visible=in_clash
	clash_start.visible=pending_attack and battle.tactics.attack.stage=="clash_ready"
	var show_clash_board:bool=in_clash or (clash_stage=="clash_result" and not battle.tactics.attack.result.get("map_restored",true))
	battle_overlay.visible=not show_clash_board
	battle_forces.visible=not show_clash_board
	if show_clash_board:battle_texture.texture=ImageTexture.create_from_image(native.clash_image())
	if in_clash:
		var clash:Dictionary=battle.tactics.attack.clash
		surrender_submit.disabled=true
		for side in range(2):
			var player:int=int(clash.players[side])
			clash_headings[side].text=("左军" if side==0 else "右军")+" · "+("电脑" if player==0 else "玩家%d" % player)
			var icon=AtlasTexture.new()
			icon.atlas=names
			icon.region=Rect2(0,(30+int(clash.first if side==0 else clash.second))*16,48,16)
			clash_names[side].texture=icon
			for kind in range(4):
				var index:int=side*4+kind
				clash_orders[index].text=["步兵","骑兵","弓兵","武将"][kind]+" · "+["前进","后退","待机","投降"][int(clash.runtime.orders[index])]
				clash_orders[index].disabled=player==0 or not (clash_stage=="clash_orders" or (clash_stage=="clash_running" and int(clash.runtime.phase)==4 and int(clash.runtime.counter)==1))
			if player!=0 and int(clash.runtime.orders[side*4+3])==3:surrender_submit.disabled=false
	retreat_button.visible=not pending_attack and (not tactical or tactical_ready)
	var pending_scout:bool=tactical and battle.tactics.has("scout")
	refresh_player_strategy(battle,tactical)
	tactical_end.visible=tactical_ready and not pending_attack and not pending_scout and not pending_retreat
	tactical_end.text="下一方行动" if tactical and int(battle.tactics.points)==0 else "结束本方行动"
	battle_texture.custom_minimum_size.y=288 if tactical and not pending_attack and not pending_scout and not pending_retreat else 320
	support_controls.visible=tactical_ready and not pending_attack and not pending_retreat
	for control in [formation_previous,formation_next,formation_label,scout_target,scout_button]:control.visible=not pending_scout
	scout_close.visible=pending_scout
	var previous_enemy:int=scout_target.get_selected_id() if scout_target.item_count>0 else -1
	scout_target.clear()
	if tactical_ready and not pending_retreat and not pending_attack:
		var formation:int=int(state.sram[0x438+int(state.sram[0xdc2+int(battle.tactics.selected)*3 if acting_side==0 else 0xdaa+int(battle.tactics.selected)*2])*8+4])&3
		formation_label.text="%d / 4 · 免费" % (formation+1)
		formation_previous.disabled=formation==0 or int(battle.tactics.points)==0
		formation_next.disabled=formation==3 or int(battle.tactics.points)==0
		scout_button.disabled=int(battle.tactics.points)==0
		for slot in range(12 if acting_side==0 else 11):
			if int(state.sram[0xdaa+slot*2 if acting_side==0 else 0xdc2+slot*3])<241:scout_target.add_item(("守城" if acting_side==0 else "进攻")+"第 %d 队" % (slot+1),slot)
		if scout_target.get_item_index(previous_enemy)>=0:scout_target.select(scout_target.get_item_index(previous_enemy))
		if scout_target.item_count==0:scout_button.disabled=true
	attack_controls.visible=(tactical_ready or pending_attack) and not pending_scout and not pending_retreat
	for button in attack_arrows:
		button.visible=not pending_attack
		button.disabled=not tactical or int(battle.tactics.points)<3
	attack_confirm.visible=pending_attack and battle.tactics.attack.stage=="confirm"
	attack_cancel.visible=attack_confirm.visible
	var handover:bool=deploying and battle.deployment.get("handover",false)
	tactical_start.visible=deploying and not active and not handover and not tactical and state.phase in ["expedition","battle"]
	tactical_start.text="进入电脑先攻" if state.phase=="battle" else "进入战术移动"
	deployment_start.visible=state.phase in ["expedition","battle"] and (not deploying or handover)
	deployment_start.text="进攻方接手布阵" if handover else "开始布阵"
	deployment_confirm.visible=deploying and not tactical
	deployment_confirm.disabled=not active
	for arrow in deployment_arrows:
		arrow.visible=deploying and not in_clash
		arrow.disabled=(not tactical_ready or pending_attack or pending_scout or pending_retreat or int(battle.tactics.points)==0) if tactical else not active
	battle_attackers.clear()
	battle_defenders.clear()
	for side in range(2):
		var list:ItemList=battle_attackers if side==0 else battle_defenders
		var count:int=11 if side==0 else 12
		for entry in range(count):
			var offset:int=0xdaa+24+entry*3 if side==0 else 0xdaa+entry*2
			var id:int=int(state.sram[offset])
			if id==255:continue
			var officer:Dictionary=state.officers[id]
			var icon=AtlasTexture.new()
			icon.atlas=names
			icon.region=Rect2(0,(30+id)*16,48,16)
			var troops:int=(int(officer.raw[6])&15)*100
			var commander:bool=side==0 and (int(state.sram[offset+2])&128)!=0
			var item:int=list.add_item(str(entry+1)+"·"+("主" if commander else "")+str(troops)+"人",icon,tactical_ready and not pending_attack and not pending_scout and not pending_retreat and side==acting_side)
			list.set_item_metadata(item,entry)
			if tactical and side==acting_side and entry==int(battle.tactics.selected):list.select(item)
		if list.item_count==0:list.add_item("全部部队已离开" if tactical else "尚未集结",null,false)
	if tactical:
		battle_description.text="%s行动 · 第 %d 回合 · 剩余机动力 %d\n点击本方名单切换部队，用方向按钮或 WASD 移动；攻击消耗 3 点机动力。\n结束行动后交给对方，最多保留 10 点机动力给本方下一轮。施计可选择计策及敌军目标，确认后扣除机动力。" % ["进攻方" if acting_side==0 else "守城方",int(battle.tactics.get("round",0))+(0 if state.phase=="battle" else 1),int(battle.tactics.points)]
		if battle.tactics.has("turn_boundary") and not pending_retreat:
			battle_description.text="轮到电脑战术行动。\n可推进战力评估与撤军决定；电脑移动、攻击和计策仍在重制中。" if battle.tactics.turn_boundary=="computer" else "战斗期限已到，进攻军须撤回，守方保有城池。\n查看限时报告后，依次安排进攻军返回来源城、安置双方俘虏；每一步均可存档。" if time_limited else "战斗已进入最终结果阶段，请保存进度。"
			if battle.tactics.get("turn_boundary","")=="computer" and battle.tactics.has("computer"):
				if battle.tactics.computer.kind=="plan":
					battle_description.text="电脑已完成撤军评估，可继续选择部队并检查城堡补位。"
					if battle.tactics.has("computer_plan"):battle_description.text="电脑已选择部队，可继续评估计策及目标。"
					if computer_next.disabled:battle_description.text="所选部队需要后续寻路、攻击或计策。\n后续决策尚未接入，当前计划已保留，可保存进度。"
					if int(battle.tactics.computer.get("role_slot",-1))>=0:
						battle_description.text="守军已调整角色，接下来沿用上一指令的方向尝试移动第 1 队。" if not computer_next.disabled else "守军已调整角色；上一指令的方向尚未确定或行动受到状态限制，当前进度可保存。"
						if not computer_next.disabled and int(state.sram[0xdaa])==255:battle_description.text="守军第 1 队已离场。确认后，电脑将继续检查其余部队的行动。"
					if battle.tactics.has("computer_strategy"):
						var decision:Dictionary=battle.tactics.computer_strategy
						battle_description.text=("电脑已选定对进攻方第 %d 队施计。\n执行后按原版扣除机动力并结算；目前尚未扣除。" % [int(decision.target_slot)+1]) if decision.kind=="strategy" else "本次未选择计策，可继续处理移动与攻击。" if decision.kind=="no_strategy" else "城堡已被敌军占领，电脑将检查计策、攻击和转向移动。"
					if battle.tactics.has("computer_motion"):
						battle_description.text="电脑正在调整城堡周围的站位，可继续绕行。" if can_flank else "当前站位无法继续绕行，电脑将按原版补充检查一次计策。" if can_retry else "当前部队没有可执行命令，继续检查下一部队；完成整圈检查后结束本方行动。" if can_scan else "电脑将检查附近敌军，并决定攻击、接近或返回任务目标。" if can_nearby else "该部队的后续行动仍在还原中，当前决定已保存。"
					if battle.tactics.has("strategy_result"):
						var report:Dictionary=battle.tactics.strategy_result
						battle_description.text=("计策生效。" if report.success else "计策未奏效。")+"\n消耗 %d 点机动力，剩余 %d。结果已保存，确认后继续电脑行动。" % [int(report.cost),int(report.points_after)]
						if int(report.converted_slot)>=0:battle_description.text+="\n目标部队已转入守军第 %d 队。" % [int(report.converted_slot)+1]
						if int(report.troops_lost)>0:battle_description.text+="\n敌军损失 %d 人。" % [int(report.troops_lost)*100]
						if int(report.hp_lost)>0:battle_description.text+="\n目标体力减少 %d。" % [int(report.hp_lost)]
				elif battle.tactics.computer.kind=="acted":
					battle_description.text="本次行动已完成，可继续电脑行动。\n剩余机动力 %d。" % [int(battle.tactics.points)]
				elif battle.tactics.computer.kind=="moved":
					var move:Dictionary=battle.tactics.computer_plan
					battle_description.text="电脑守军第 %d 队已补入空置城堡。\n消耗 %d 点机动力，剩余 %d。可继续电脑行动或保存进度。" % [int(move.slot)+1,int(move.points_before)-int(move.points_after),int(move.points_after)]
			if time_limited and time_notice.has("speaker"):
				battle_description.text+="\n%s的武将报告：战斗期限已到。" % RULERS[int(time_notice.faction)]
			if withdrawal_result:
				var settlement:Dictionary=battle.tactics.get("settlement",{})
				var arrived:int=0
				var dispersed:int=0
				for outcome in settlement.get("outcomes",[]):
					if outcome.returned:arrived+=1
					else:dispersed+=1
				battle_description.text="守军撤离后的战后结算 · 已安置 %d 人，离散 %d 人。\n进攻军按忠诚度依次入城，再处理双方俘虏和城池物资；每一步均可存档。" % [arrived,dispersed]
				if result_reason==20:
					battle_description.text="进攻主将已离场，守方保有城池。\n战后已安置 %d 人，离散 %d 人。先处理双方俘虏，再让剩余进攻军返回来源城；每一步均可存档。" % [arrived,dispersed]
					if settlement_stage==4:battle_description.text+="\n结算完成，确认后返回战略地图。"
				elif result_reason==36:
					battle_description.text="进攻军已撤退，守方保有城池。\n战后已安置 %d 人，离散 %d 人。双方俘虏分别进入守城和出征来源城；每一步均可存档。" % [arrived,dispersed]
					if settlement_stage==17:battle_description.text+="\n安置完成，确认后返回战略地图。"
				elif result_reason==3:
					battle_description.text="守军全灭后的战后结算 · 已安置 %d 人，离散 %d 人。\n先处理守城名册，再依次安置进攻军和双方俘虏，最后结算城池。每一步均可存档。" % [arrived,dispersed]
				if settlement_stage in [13,29]:
					var city:Dictionary=state.cities[int(battle.target)]
					battle_description.text="%s已归进攻方所有。\n入城安置 %d 人，离散 %d 人。金 %d，土地 %d，商业 %d，人口 %d。\n确认后回到战略地图，%s。" % [city.name,arrived,dispersed,int(city.gold),int(city.land),int(city.commerce),int(city.population),"轮到下一位君主" if state.phase=="battle" else "继续当前势力的指令"]
				if ruler_result and settlement_stage in [4,29]:
					var annexation:Dictionary=battle.tactics.get("annexation",{})
					var inspected:int=int(annexation.get("next_city",0))
					battle_description.text="战场军团与俘虏已安置，继续接收败方君主的其他领地。\n已核对 %d / 30 城，接收 %d 城。\n驻将忠诚与城池资源按原版规则调整；战场目标城不会重复扣减。每一步均可存档。" % [inspected,annexation.get("outcomes",[]).size()]
					if inspected==30:battle_description.text+="\n败方领地已全部处理，确认后返回战略地图。"
		elif pending_retreat:
			var retreat:Dictionary=battle.tactics.retreat
			if retreat.get("defending",false):
				if retreat.stage=="confirm":
					battle_description.text="当前守军撤往本势力驻将最少的其他城池，人数相同时选择原版编号较小的城池。\n成功消耗 1 点机动力；无城可退或名册已满时不扣点。\n取消后继续当前部队行动。"
				else:
					var outcome:Dictionary=retreat.outcomes[0]
					battle_description.text=("守军已撤往%s，消耗 1 点机动力，剩余 %d。" % [state.cities[int(outcome.city)].name,int(battle.tactics.points)]) if outcome.returned else "没有可容纳这支守军的本方城池，部队留在战场，机动力未扣除。"
					if retreat.get("computer",false):
						battle_description.text="电脑守军已决定撤退。\n"+(("部队撤往%s，消耗 1 点机动力，剩余 %d。" % [state.cities[int(outcome.city)].name,int(battle.tactics.points)]) if outcome.returned else "其他城池名册已满，部队按原版规则离散；已消耗 1 点机动力。")
					battle_description.text+="\n"+("确认后进入战后军团、俘虏与城池结算。" if retreat.ended else "确认结果后继续守方战术行动。")
			elif retreat.stage=="confirm":
				battle_description.text=("主将撤退将命令全部进攻部队返回来源城。" if retreat.commander else "当前部队撤回来源城，其他部队继续作战。")+"\n确认消耗 1 点机动力；此时取消不扣点。\n主将撤退时，原城已满的部队将按原版规则离散。"
			else:
				var returned:int=0
				for outcome in retreat.outcomes:
					if outcome.returned:returned+=1
				battle_description.text="撤退已消耗 1 点机动力，剩余 %d。\n成功返回 %d 队。" % [int(battle.tactics.points),returned]
				if returned<retreat.outcomes.size():battle_description.text+=("其余部队因原城已满而离散。" if retreat.commander else "原城已满，该队留在战场。")
				battle_description.text+="\n"+("全部进攻部队已离开；确认结果后安置剩余俘虏，再返回战略地图。" if retreat.ended else "确认结果后继续战术行动。")
		elif pending_scout:
			var report:Dictionary=battle.tactics.scout
			var raw:Array=state.officers[int(report.officer)].raw
			battle_description.text="侦察敌方第 %d 队 · 已消耗 1 点机动力 · 剩余 %d\n体力 %d　智力 %d　武力 %d　兵力 %d　阵容 %d / 4\n关闭报告后继续行动；报告可保存，重新打开不会再次扣点。" % [int(report.target_slot)+1,int(battle.tactics.points),int(raw[1]),int(raw[2]),int(raw[3]),(int(raw[6])&15)*100,(int(raw[4])&3)+1]
		elif pending_attack:
			var attack:Dictionary=battle.tactics.attack
			battle_description.text="本方第 %d 队 → 敌方第 %d 队 · 剩余机动力 %d\n" % [int(attack.slot)+1,int(attack.target_slot)+1,int(battle.tactics.points)]
			if attack.stage=="confirm":battle_description.text+="攻击已扣除 3 点机动力；取消不返还。可完成交战准备或保存进度。\n准备完成后可进入交战军令，查看并配置双方兵队，再推进交锋。"
			elif in_clash:
				var units:Array=attack.clash.runtime.units
				battle_description.text="交战军令 · 左军体力 %d / 右军体力 %d\n点击本方兵种按钮循环军令：前进、后退、待机；普通武将另可选择投降。\n可逐步或自动推进交锋；选中投降后可提交军令。当前进度可保存恢复。" % [int(units[1]),int(units[34])]
				if in_duel:
					var duel:Dictionary=attack.duel
					battle_description.text="单挑 · 左军体力 %d / 右军体力 %d\n" % [0 if int(units[1])==255 else int(units[1]),0 if int(units[34])==255 else int(units[34])]
					match clash_stage:
						"duel_orders":battle_description.text+="%s选择招式。\n牵制、攻击、舍命一击、后退；普通武将可投降。" % ("左军" if duel_side==0 else "右军")
						"duel_exchange":
							var exchange:Dictionary=duel.exchange
							battle_description.text+=("攻击未命中。" if exchange.outcome=="miss" else ("遭到反击，" if exchange.outcome=="counter" else "攻击命中，")+("左军" if int(exchange.target_side)==0 else "右军")+"损失 %d 点体力。" % int(exchange.last_damage))+"\n继续查看下一步；当前进度可保存。"
						"duel_defeat":battle_description.text+="武将体力耗尽，单挑结束。\n继续登记败北与双方兵力。"
						"duel_retreat":battle_description.text+="武将退出单挑。\n继续后返回交锋，将该武将军令改为后退；保留现有体力。"
						"duel_surrender_confirm":battle_description.text+="是否确认投降？取消后仍由当前武将选择招式。"
						"duel_surrender_notice":battle_description.text+="武将请求投降。继续查看对方答复。"
						"duel_surrender_accepted":battle_description.text+="对方接受投降。继续登记俘虏和双方兵力。"
				elif clash_stage=="clash_running":
					var current:int=int(attack.clash.runtime.active)
					battle_description.text="交锋进行中 · %s第 %d 兵队\n左军主将体力 %d / 右军主将体力 %d\n可暂停、保存或逐步推进；军令按钮在行动间隙开放。进入对话时会暂停；尚未还原的阶段可保留现场并保存。" % ["右军" if current>=128 else "左军",(current&15)+1,int(units[1]),int(units[34])]
				elif clash_stage=="clash_boundary":
					var reasons:Dictionary={"duel":"进入武将单挑","retreat":"进入交锋撤离","general_defeat":"主将体力耗尽","ai_scratch":"电脑军令等待继续"}
					battle_description.text=("武将已到达交锋撤离边界。\n点击继续，查看追击与撤离结果；不会再次扣减战术机动力。" if attack.boundary=="retreat" else "交锋已暂停 · "+str(reasons.get(attack.boundary,"后续流程待接入"))+"\n这一流程尚未完整接入，兵队、随机状态和战场均已保留，可保存进度。")
					if attack.boundary=="general_defeat":battle_description.text="武将体力耗尽，本次交锋结束。\n点击继续确认败北记录，再结算双方兵力；当前现场可保存。"
					if attack.boundary=="duel":battle_description.text="双方武将相遇，即将单挑。\n点击进入单挑选择招式；当前进度可保存。"
					if attack.boundary=="ai_scratch":battle_description.text="电脑正在决定军令。\n点击“继续电脑军令”恢复交锋，当前进度可保存。"
				elif clash_stage=="clash_defeat":battle_description.text="武将败北已登记。\n继续写回双方兵力，处理部队离场；不会再次扣减战术机动力。"
				elif clash_stage=="clash_retreat":
					var retreat:Dictionary=attack.retreat
					match int(attack.clash.runtime.counter):
						2:battle_description.text="撤离时受到追击，损失 %d 点体力。\n继续查看撤离结果；当前进度可保存。" % int(retreat.injury.damage)
						3:battle_description.text="撤离武将已失去战斗能力。\n继续确认此次交锋损失。"
						4:battle_description.text=("成功避开追击。" if int(retreat.injury.counter)==4 else "追击已结束。")+"\n继续结算撤离中的散兵损失。"
						6:battle_description.text="撤离中有 %d 支兵队离散。\n继续写回双方兵力，返回战术地图。" % int(retreat.losses)
						7:battle_description.text="主将战败记录已登记。\n继续处理部队离场；君主及整场战果尚待后续流程。"
				elif clash_stage=="surrender_confirm":battle_description.text="是否确认本方武将投降？\n确认后进入原版投降对话；此时取消会返回军令配置。\n双方兵力与俘虏名册尚未改变。"
				elif clash_stage=="surrender_notice":battle_description.text=("对方" if int(attack.clash.players[int(attack.surrender.side)])==0 else "本方")+"武将请求投降。\n继续查看答复；当前对话进度可保存。"
				elif clash_stage=="surrender_accepted":battle_description.text=("本方" if int(attack.clash.players[int(attack.surrender.side)])==0 else "对方")+"接受投降。\n继续后将按原版规则登记俘虏、调整忠诚度并移除战场部队。"
			elif clash_stage=="clash_result":
				if attack.result.kind=="duel":
					battle_description.text=("单挑投降已结算，武将已登记为俘虏。" if attack.result.outcome=="surrender" else "单挑败北已结算，部队已离场。")+"\n双方兵力已写回。\n"
					battle_description.text+=("可返回战术行动。" if attack.result.can_continue else "最终战果处置尚未接入，请保存进度。")
				elif attack.result.kind=="defeat":
					battle_description.text="武将败北已结算，部队已离场，双方兵力已写回。\n"
					battle_description.text+=("战术地图已恢复。" if attack.result.map_restored else "已进入原版君主战败阶段，现场已保留。")+"\n"+("可返回战术行动。" if attack.result.can_continue else "最终战果处置尚未接入，请保存进度。")
				elif attack.result.kind=="retreat":
					battle_description.text="交锋撤离已结算，双方兵力已写回。\n"
					if attack.result.defeated:battle_description.text+="撤离武将已失去战斗能力并离场。\n"
					battle_description.text+=("战术地图已恢复。" if attack.result.map_restored else "已进入原版君主战败阶段，现场已保留。")+"\n"+("可返回战术行动。" if attack.result.can_continue else "最终战果处置尚未接入，请保存进度。")
				else:
					battle_description.text="投降已结算，武将已进入战斗俘虏名册，战术地图已恢复。\n"+("可返回战术行动。" if attack.result.can_continue else "已触及主将离场或全军结果；最终战果处置尚未接入，请保存进度。")+"\n俘虏最终处置尚未接入，记录会随战局保留。"
			else:battle_description.text+="交战准备已完成，可进入交战军令查看双方部队与战场，或保存进度。\n可配置军令并推进交锋，随后处理伤亡、俘虏和战果。"
		if battle.get("army_defeat_result_available",false):
			battle_description.text="守军已全部离场，交锋伤亡已经结算。\n确认全军战果后，继续处理入城部队、俘虏和城池物资。"
		if battle.get("commander_defeat_result_available",false):
			battle_description.text="进攻主将已离场，交锋伤亡已经结算。\n确认战果后，先处理双方俘虏，再安排剩余进攻军撤回。"
		if battle.get("human_failure_available",false):
			battle_description.text="君主战败，查看失败报告后确认战局是否继续。"
		if battle.get("ruler_defeat_result_available",false):
			battle_description.text="电脑君主已败北，交锋伤亡已经结算。\n先处理战场军团、俘虏与城池，再逐城接收其余领地。"
		if battle.tactics.has("captives"):
			var captive_count:int=0
			for id in battle.tactics.captives:
				if int(id)!=255:captive_count+=1
			if captive_count>0:battle_description.text+="\n战斗俘虏：%d 名（等待最终处置）。" % captive_count
	elif deploying:
		var defending:bool=battle.deployment.get("side","defender" if state.phase=="battle" else "attacker")=="defender"
		var side_name:String="守城" if defending else "出征"
		battle_description.text=("部署%s第 %d 队：方向键/WASD 移动，Enter 确认。\n浅色网格是本方部署区域，黄框是当前部队。" % [side_name,int(battle.deployment.current)+1]) if active else "双方部队已完成布阵，位置可保存和恢复。"
		if battle.deployment.get("two_players",false) and (active or handover):
			var owner:int=(int(state.sram[0xde3])&15) if defending else (int(state.sram[0xde3])>>4)
			var player_number:int=1 if owner==(int(state.sram[0xd89])&7) else 2
			if handover:battle_description.text="守城方部署完毕，请交给玩家%d（%s）。\n点击“进攻方接手布阵”或按 Enter 后开始。" % [player_number,RULERS[owner]]
			else:battle_description.text="玩家%d（%s） · " % [player_number,RULERS[owner]]+battle_description.text
		battle_description.text+="\n"+("电脑进攻方先行动，点击“进入电脑先攻”可推进选队、施计、移动与交战。" if state.phase=="battle" else "双方布阵完成后可移动及发起攻击；实时交战与战后结算尚未接入。")
	elif state.phase=="expedition":
		battle_description.text="已出征 %d 名武将，支付 %d 金。\n可进入布阵，按原版区域逐将部署，并保存进度。\n实时交战与战后结算尚未接入。" % [battle.officers.size(),int(battle.cost)]
	else:
		battle_description.text="敌军来袭，请部署守城武将。\n守军全部确认后，电脑自动部署进攻部队。可保存布阵进度。\n实时交战和战后结算尚未接入。"
	human_failure_controls.visible=tactical and battle.tactics.has("human_failure")
	if human_failure_controls.visible:
		var failure:Dictionary=battle.tactics.human_failure
		var failure_picture=AtlasTexture.new()
		failure_picture.atlas=load("res://assets/original/failure-reference.png")
		failure_picture.region=Rect2(56,40,144,96)
		battle_texture.texture=failure_picture
		battle_texture.custom_minimum_size.y=288
		battle_overlay.hide()
		battle_forces.hide()
		surrender_controls.hide()
		clash_result_close.hide()
		clash_defeat_row.hide()
		clash_retreat_row.hide()
		human_failure_next.disabled=not failure.can_continue
		human_failure_next.text="确认失败，继续战后结算" if failure.can_continue else "战局结束"
		battle_description.text="%d 年 %d 月　%s 大人\n%s 击败了你\n请重新来过" % [state.year,state.month,RULERS[int(failure.loser)],RULERS[int(failure.winner)]]
		battle_description.text+="\n另一位玩家仍在征战，确认后继续处理战果。" if failure.can_continue else "\n所有玩家均已败北。可保存报告或返回标题开始新游戏。"
	if tactical and state.phase=="battle" and battle.tactics.has("turn_boundary") and not pending_attack and not pending_retreat:
		if battle.tactics.turn_boundary=="computer":
			var advance_state:Dictionary=battle.tactics.get("attacker_ai",{})
			battle_description.text="电脑进攻方行动 · 第 %d 回合 · 机动力 %d\n%s；可随时保存进度。" % [int(battle.tactics.get("round",1)),int(battle.tactics.points),computer_next.text]
			if advance_state.get("stage","")=="strategy_result":
				var effect:Dictionary=advance_state.result
				battle_description.text+="\n施计%s，消耗 %d 点机动力。" % ["成功" if effect.success else "失败",int(effect.points_before)-int(effect.points_after)]
		elif not withdrawal_result and not time_limited:battle_description.text="电脑来袭的战场已进入战果边界，当前现场可保存。"
	battle_details.popup_centered(Vector2i(700,650))

func deployment_action(action:StringName) -> void:
	if action!="clash_step":
		clash_timer.stop()
		clash_auto.text="自动推进"
	var error:String=""
	if action=="strategy_prepare":error=native.prepare_tactical_strategy(int(state.battle.tactics.selected),human_strategy_target.get_selected_id(),human_strategy_choices.get_selected_id())
	elif action=="strategy_confirm":error=native.confirm_tactical_strategy()
	elif action=="strategy_cancel":error=native.cancel_tactical_strategy()
	elif action=="strategy_finish":error=native.finish_tactical_strategy()
	elif action=="computer_tactics":
		if state.phase=="battle":error=native.resume_computer_attacker_retreat() if state.battle.tactics.get("attacker_ai",{}).get("stage","")=="held_retreat" else native.advance_computer_attack()
		elif state.battle.tactics.get("computer",{}).get("kind","")=="plan":
			if int(state.battle.tactics.computer.get("role_slot",-1))>=0:
				error=native.continue_computer_role()
				if not error.is_empty():error=native.continue_empty_computer_role()
				if not error.is_empty():error=native.resume_computer_role_after_handover()
			elif state.battle.tactics.has("strategy_result"):error=native.finish_computer_strategy()
			elif state.battle.tactics.get("computer_motion",{}).get("kind","")=="retry_strategy":error=native.retry_computer_strategy()
			elif state.battle.tactics.get("computer_motion",{}).get("kind","")=="scan":error=native.continue_computer_scan()
			elif state.battle.tactics.get("computer_motion",{}).get("branch","")=="role":error=native.continue_computer_nearby()
			elif state.battle.tactics.has("computer_motion"):error=native.continue_computer_flank()
			elif state.battle.tactics.get("computer_strategy",{}).get("kind","")=="role_reassignment":error=native.resume_computer_occupied_fort()
			elif state.battle.tactics.has("computer_strategy"):error=native.continue_computer_motion() if state.battle.tactics.computer_strategy.kind=="no_strategy" else native.execute_computer_strategy()
			else:error=native.evaluate_computer_strategy() if state.battle.tactics.has("computer_plan") else native.resume_computer_unit() if state.battle.tactics.has("computer_reuse") else native.plan_computer_tactics()
		else:error=native.advance_computer_tactics()
		if not error.is_empty() and state.phase!="battle":
			var exhausted_error:String=native.finish_exhausted_computer_attack()
			if exhausted_error.is_empty():error=""
	elif action=="end_tactical_turn":error=native.end_tactical_turn()
	elif action=="begin_clash":error=native.begin_clash()
	elif action=="clash_step":error=native.advance_clash()
	elif action=="clash_strategy_resume":
		error=native.resume_clash_strategy()
		if not error.is_empty():error=native.recover_clash_strategy()
	elif action=="clash_retreat_next":error=native.advance_clash_retreat()
	elif action=="clash_defeat_next":error=native.advance_clash_defeat()
	elif action.begins_with("duel_command_"):error=native.choose_duel_command(int(action.trim_prefix("duel_command_")))
	elif action=="duel_next":error=native.begin_duel() if state.battle.tactics.attack.stage=="clash_boundary" else native.advance_duel()
	elif action=="duel_yes":error=native.answer_duel_surrender(true)
	elif action=="duel_no":error=native.answer_duel_surrender(false)
	elif action=="surrender_submit":error=native.request_clash_surrender()
	elif action=="surrender_yes" or action=="surrender_no":error=native.answer_clash_surrender(action=="surrender_yes")
	elif action=="surrender_next":error=native.advance_clash_surrender()
	elif action=="human_failure_next":error=native.finish_human_failure()
	elif action=="result_close":error=native.begin_human_failure() if state.battle.get("human_failure_available",false) else native.begin_ruler_defeat_result() if state.battle.get("ruler_defeat_result_available",false) else native.begin_commander_defeat_result() if state.battle.get("commander_defeat_result_available",false) else native.begin_defender_defeat_result() if state.battle.get("army_defeat_result_available",false) else native.finish_clash_result()
	elif str(action).begins_with("clash_"):
		var parts:PackedStringArray=str(action).split("_")
		error=native.cycle_clash_order(int(parts[1]),int(parts[2]))
	elif action=="retreat":error=native.begin_tactical_retreat(int(state.battle.tactics.selected))
	elif action=="retreat_cancel":error=native.cancel_tactical_retreat()
	elif action=="retreat_confirm":error=native.confirm_tactical_retreat()
	elif action=="retreat_finish":error=native.finish_tactical_retreat()
	elif action=="time_limit_result":error=native.advance_time_limit_result()
	elif action=="withdrawal_result":
		if int(state.battle.tactics.get("settlement",{}).get("stage",0)) in [4,13,17,29]:
			error=native.advance_ruler_annexation() if int(state.battle.tactics.get("turn_reason",0)) in [131,148] and int(state.battle.tactics.get("annexation",{}).get("next_city",0))<30 else native.finish_withdrawal_result()
		else:error=native.advance_withdrawal_result()
	elif action=="formation_previous" or action=="formation_next":error=native.adjust_tactical_formation(int(state.battle.tactics.selected),1 if action=="formation_previous" else 0)
	elif action=="scout":error=native.scout_tactical(int(state.battle.tactics.selected),scout_target.get_selected_id())
	elif action=="close_scout":error=native.close_tactical_scout()
	elif action=="attack_confirm":error=native.confirm_tactical_attack()
	elif action=="attack_cancel":error=native.cancel_tactical_attack()
	elif str(action).begins_with("attack_"):error=native.attack_tactical(int(state.battle.tactics.selected),int(str(action).trim_prefix("attack_")))
	elif action=="tactics":error=native.begin_tactics()
	elif action=="prepare":error=native.prepare_deployment()
	elif action=="confirm":error=native.confirm_deployment()
	elif action=="save":
		error=native.save_session(SAVE)
		battle_description.text+="\n"+("战场进度已保存。" if error.is_empty() else error)
		return
	elif state.battle.has("tactics"):error=native.move_tactical(int(state.battle.tactics.selected),int(str(action)))
	else:error=native.move_deployment(int(str(action)))
	refresh()
	if state.battle==null:
		battle_details.hide()
		status.text=("战后结算完成，继续战略行动。" if action=="withdrawal_result" else "进攻部队已撤回，继续战略行动。") if error.is_empty() else error
		return
	show_battle_details()
	if not error.is_empty():
		clash_timer.stop()
		clash_auto.text="自动推进"
		battle_description.text+="\n"+error

func select_tactical_unit(index:int,side:int=0) -> void:
	var list:ItemList=battle_attackers if side==0 else battle_defenders
	var error:String=native.select_tactical_unit(int(list.get_item_metadata(index)))
	refresh()
	if state.battle==null:
		battle_details.hide()
		status.text="进攻部队已撤回，继续战略行动。" if error.is_empty() else error
		return
	show_battle_details()
	if not error.is_empty():battle_description.text+="\n"+error

func deployment_input(event:InputEvent) -> void:
	if not event is InputEventKey or not event.pressed or event.echo:return
	if state.battle==null or not state.battle.has("deployment"):return
	if state.battle.has("tactics") and state.battle.tactics.has("attack") and state.battle.tactics.attack.stage!="confirm":return
	var action:String=""
	match event.keycode:
		KEY_RIGHT,KEY_D:action="0"
		KEY_LEFT,KEY_A:action="1"
		KEY_DOWN,KEY_S:action="2"
		KEY_UP,KEY_W:action="3"
		KEY_ENTER,KEY_KP_ENTER:
			if not state.battle.has("tactics"):action="prepare" if state.battle.deployment.get("handover",false) else "confirm"
	if not action.is_empty():
		battle_details.set_input_as_handled()
		deployment_action(action)

func setup_orders() -> void:
	order_dialog=ConfirmationDialog.new()
	order_dialog.title="物资与人事"
	order_dialog.ok_button_text="执行指令"
	order_dialog.cancel_button_text="取消"
	order_dialog.confirmed.connect(execute_order)
	add_child(order_dialog)
	scout_dialog=AcceptDialog.new()
	scout_dialog.ok_button_text="返回战局"
	add_child(scout_dialog)
	scout_texture=TextureRect.new()
	scout_texture.custom_minimum_size=Vector2(768,480)
	scout_texture.expand_mode=TextureRect.EXPAND_IGNORE_SIZE
	scout_texture.stretch_mode=TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	scout_texture.texture_filter=CanvasItem.TEXTURE_FILTER_NEAREST
	scout_dialog.add_child(scout_texture)
	var content=VBoxContainer.new()
	content.add_theme_constant_override("separation",12)
	order_dialog.add_child(content)
	order_kind=OptionButton.new()
	for title in ORDER_TITLES:order_kind.add_item(title)
	order_kind.custom_minimum_size=Vector2(0,40)
	order_kind.item_selected.connect(select_order)
	content.add_child(order_kind)
	order_description=Label.new()
	order_description.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
	order_description.custom_minimum_size=Vector2(640,62)
	content.add_child(order_description)
	order_target=OptionButton.new()
	order_target.custom_minimum_size=Vector2(0,40)
	content.add_child(order_target)
	order_officer=OptionButton.new()
	order_officer.custom_minimum_size=Vector2(0,40)
	var paper=StyleBoxFlat.new()
	paper.bg_color=Color("f1e2b4")
	for key in ["normal","hover","pressed"]:order_officer.add_theme_stylebox_override(key,paper)
	order_officer.get_popup().add_theme_stylebox_override("panel",paper)
	content.add_child(order_officer)
	for i in range(5):
		var row=HBoxContainer.new()
		row.add_theme_constant_override("separation",16)
		content.add_child(row)
		var title=Label.new()
		title.text=ITEM_NAMES[i]
		title.custom_minimum_size=Vector2(160,38)
		row.add_child(title)
		var quantity=SpinBox.new()
		quantity.min_value=0
		quantity.max_value=999999
		quantity.custom_minimum_size=Vector2(0,38)
		quantity.size_flags_horizontal=Control.SIZE_EXPAND_FILL
		row.add_child(quantity)
		order_rows.append(row)
		order_amounts.append(quantity)

func show_orders() -> void:
	if orders_button.disabled:return
	order_officer.clear()
	for id in state.cities[selected].officer_slots:
		if id==null:continue
		var icon=AtlasTexture.new()
		icon.atlas=names
		icon.region=Rect2(0,(30+int(id))*16,48,16)
		order_officer.add_icon_item(icon,"",int(id))
		if int(id) in chosen:order_officer.select(order_officer.item_count-1)
	select_order(order_kind.selected)
	order_dialog.popup_centered(Vector2i(700,520))

func select_order(index:int) -> void:
	var kind:String=ORDER_KEYS[index]
	var c:Dictionary=state.cities[selected]
	order_target.clear()
	order_target.visible=kind in ["transport","gift","scout"]
	if order_target.visible:
		for id in range(30) if kind=="scout" else c.neighbors:
			var owner=int(state.cities[int(id)].faction_id)
			if kind=="scout" or (kind=="transport" and owner==int(state.current_ruler)) or (kind=="gift" and owner!=int(state.current_ruler) and owner<6):
				order_target.add_item(state.cities[int(id)].name,int(id))
	order_officer.visible=kind in ["award_gold","award_weapons","study","heal"]
	var descriptions=[
		"将物资运至相邻的己方城池，消耗一枚命令书。",
		"向相邻势力赠送宝石、手镯或指环。对方可能拒绝、收礼或允许通行；消耗一枚命令书。",
		"从本城黄金中赏赐所选武将，提高忠诚，消耗一枚命令书。",
		"赏赐武器以提高武力，消耗一枚命令书。",
		"将黄金赏给本城百姓，提高统治度，消耗一枚命令书。",
		"购买武器：每件五金，消耗一枚命令书。",
		"出售宝物：宝石九十金、手镯六十金、指环三十金，消耗一枚命令书。",
		"学习提高知识。进入学问所需持有三十金；费用按知识分为三十、二十或十金，消耗一枚命令书。",
		"治疗需要五十金，恢复武将体力，消耗一枚命令书。",
		"查看所选城池四周的地形；侦查其他城池消耗一枚命令书，查看本城不消耗。"
	]
	order_description.text=descriptions[index]
	var enabled=true
	var service={"buy":0,"study":1,"heal":2,"sell":3}.get(kind,-1)
	if service>=0 and not float(service) in c.shops:
		enabled=false
		order_description.text+="\n本城没有此商店。"
	if order_target.visible and order_target.item_count==0:
		enabled=false
		order_description.text+="\n没有符合条件的相邻城池。"
	if order_officer.visible and order_officer.item_count==0:enabled=false
	order_dialog.get_ok_button().disabled=not enabled
	for i in range(5):
		var row:HBoxContainer=order_rows[i]
		var amount:SpinBox=order_amounts[i]
		row.visible=kind=="transport" or (kind in ["gift","sell"] and i>=2) or (i==0 and kind in ["award_gold","award_weapons","award_people","buy"])
		row.get_child(0).text=ITEM_NAMES[i]
		amount.max_value=int(c.inventory[i])
		amount.value=0
		if i==0 and kind in ["award_gold","award_people","buy","award_weapons"]:
			row.get_child(0).text="武器数量" if kind in ["buy","award_weapons"] else "黄金数量"
			amount.max_value=int(c.gold)/5 if kind=="buy" else int(c.inventory[1]) if kind=="award_weapons" else int(c.gold)
			amount.value=mini(1,int(amount.max_value))

func execute_order() -> void:
	var kind:String=ORDER_KEYS[order_kind.selected]
	var args:Dictionary={"quantity":int(order_amounts[0].value)}
	if order_target.visible and order_target.item_count:args.target=order_target.get_selected_id()
	if order_officer.visible and order_officer.item_count:args.officer=order_officer.get_selected_id()
	if kind in ["transport","gift","sell"]:
		args.items=[]
		for i in range(5):args.items.append(int(order_amounts[i].value) if order_rows[i].visible else 0)
	var result:Dictionary=native.execute_command(selected,kind,args)
	if result.has("error"):
		status.text=result.error
		return
	status.text=ORDER_TITLES[order_kind.selected]+"已完成。"
	if kind=="scout":
		scout_dialog.title=state.cities[int(result.target)].name+" · 城池四周"
		scout_texture.texture=ImageTexture.create_from_image(native.battlefield_image(int(result.target)))
		scout_dialog.popup_centered(Vector2i(800,550))
	if result.has("delta"):status.text+="属性变化：%d。" % int(result.delta)
	if result.has("cost"):status.text+="花费 %d 金。" % int(result.cost)
	if result.has("gold"):status.text+="获得 %d 金。" % int(result.gold)
	if result.has("outcome"):status.text+=["对方允许通行。","对方收下礼物。","对方拒绝，宝物保留。"][int(result.outcome)]
	refresh()

func show_recruit() -> void:
	if recruit_button.disabled:return
	recruit_quantity.max_value=mini(255,int(state.cities[selected].gold)/20)
	recruit_quantity.value=mini(1,int(recruit_quantity.max_value))
	recruit_dialog.popup_centered(Vector2i(620,205))

func recruit() -> void:
	var error:String=native.recruit(selected,int(recruit_quantity.value))
	if not error.is_empty():status.text=error;return
	refresh()
	show_army()

func show_army() -> void:
	state=native.session_snapshot()
	if state.pending_army==null:return
	army_officers.clear()
	for id in state.cities[int(state.pending_army)].officer_slots:
		if id==null:continue
		var atlas=AtlasTexture.new()
		atlas.atlas=names
		atlas.region=Rect2(0,(30+int(id))*16,48,16)
		army_officers.add_icon_item(atlas,"#%03d" % id,int(id))
	if army_officers.item_count>0:
		army_officers.select(0)
		select_army_officer(0)
	army_dialog.popup_centered(Vector2i(550,255))

func select_army_officer(index:int) -> void:
	state=native.session_snapshot()
	var officer:Dictionary=state.officers[army_officers.get_item_id(index)]
	army_quantity.value=(int(officer.infantry)+int(officer.cavalry)+int(officer.archers))/100
	army_status.text="后备兵 %d 人\n步 %d　骑 %d　弓 %d" % [state.cities[int(state.pending_army)].reserves,officer.infantry,officer.cavalry,officer.archers]

func assign_army() -> void:
	var error:String=native.assign_troops(army_officers.get_selected_id(),int(army_quantity.value))
	if not error.is_empty():army_status.text=error;return
	select_army_officer(army_officers.selected)
	refresh()

func finish_recruitment() -> void:
	native.finish_recruitment()
	refresh()
	status.text="征兵与编成已完成。"

func show_remaining() -> void:
	notice.dialog_text="已接入原版 AI 行动、双人开局、势力轮换、月结算、搜索返回、移动、开发和征兵编成。\n运输、赠送、赏赐、商店与30城侦查已接入。\n玩家出征、守城和双人交接布阵、移动与进度保存已接入。\n军令交锋、单挑、双方战术轮换和战后结算已接入。\n原版完整演出、全部音效、特殊战斗边界及自然统一通关验收仍未完成。"
	notice.popup_centered(Vector2i(690,220))

func end_turn() -> void:
	var error: String = native.end_turn()
	if not error.is_empty(): status.text=error;return
	moving=false
	ai_paused=false
	refresh()
	if state.phase=="player_commands": select_current_seat()
	status.text="本回合结束。"

func select_current_seat() -> void:
	var ruler=int(state.current_ruler)
	for c in state.cities:
		if int(c.faction_id)==ruler and ruler in c.officer_slots:
			select_city(int(c.id))
			return
	for c in state.cities:
		if int(c.faction_id)==ruler:
			select_city(int(c.id))
			return

func advance_ai() -> void:
	var result: Dictionary = native.advance_turn()
	if result.has("error"):
		ai_paused=true
		status.text="回合未提交："+str(result.error)
		return
	refresh()
	if state.phase=="player_commands":
		select_current_seat()
		status.text="轮到%s。搜索返回后，进入派遣城查看报告。" % RULERS[int(state.current_ruler)]
	elif state.phase=="battle":
		selected=int(state.battle.target)
		select_city(selected)
		status.text="%s 遭到进攻，双方战斗记录已保留，可保存当前战局。" % state.cities[selected].name
	elif state.phase=="ending":
		status.text="天下统一。" if state.ending.kind=="unification" else "已到达年限。" if state.ending.kind=="year_limit" else "玩家势力已灭亡。"

func show_officers() -> void:
	state=native.session_snapshot()
	var locations: Dictionary={}
	for c in state.cities:
		for id in c.officer_slots:
			if id!=null: locations[int(id)]=c.name
	for party in state.search_parties:
		locations[int(party.officer)]="搜索返回" if party.ready else "外出搜索"
	officer_list.clear()
	for o in state.officers:
		var id=int(o.id)
		var atlas=AtlasTexture.new()
		atlas.atlas=names
		atlas.region=Rect2(0,(30+id)*16,48,16)
		officer_list.add_item("#%03d %s · 知%d 武%d 德%d" % [id,locations.get(id,"未驻城"),o.intelligence,o.martial,o.virtue],atlas)
		officer_list.set_item_tooltip(id,"体 %d　知 %d　武 %d　德 %d\n忠 %s\n步兵 %d　骑兵 %d　弓兵 %d" % [o.stamina,o.intelligence,o.martial,o.virtue,str(int(o.loyalty)) if o.loyalty!=null else "--",o.infantry,o.cavalry,o.archers])
	officer_book.popup_centered()

func _process(delta: float) -> void:
	if initialized and state.phase!="ending":
		clock_remainder+=delta*60.0
		var frames=min(int(clock_remainder),3600)
		if frames>0: native.tick(frames);clock_remainder-=frames
		if state.phase=="ai_turn" and not ai_paused: advance_ai()

func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.button_index==MOUSE_BUTTON_LEFT and initialized:
		for c in state.cities:
			var at = Vector2(float(c.map_position[0]),float(c.map_position[1])+1)*3+Vector2(42,126)
			if Rect2(at-Vector2(6,6),Vector2(36,36)).has_point(event.position):
				select_city(int(c.id))
				accept_event()
				return

func move_cursor(direction: Vector2i) -> void:
	cursor+=direction*8
	cursor.x=clampi(cursor.x,0,248)
	cursor.y=clampi(cursor.y,0,152)
	get_child(0).queue_redraw()

func confirm_cursor() -> void:
	for c in state.cities:
		if Rect2i(Vector2i(int(c.map_position[0]),int(c.map_position[1])),Vector2i(16,16)).has_point(cursor):
			select_city(int(c.id))
			return

func _unhandled_input(event: InputEvent) -> void:
	if not initialized or ending_report.visible or confirm.visible or search_confirm.visible or search_report.visible or recruit_dialog.visible or order_dialog.visible or scout_dialog.visible or expedition_dialog.visible or battle_details.visible or army_dialog.visible or notice.visible or kinds.visible or officer_book.visible: return
	var key = -1
	if event is InputEventKey and event.pressed:
		key=event.keycode
	elif event is InputEventJoypadButton and event.pressed:
		var bindings={JOY_BUTTON_DPAD_UP:KEY_UP,JOY_BUTTON_DPAD_DOWN:KEY_DOWN,JOY_BUTTON_DPAD_LEFT:KEY_LEFT,JOY_BUTTON_DPAD_RIGHT:KEY_RIGHT,JOY_BUTTON_A:KEY_Z,JOY_BUTTON_B:KEY_X}
		key=bindings.get(event.button_index,-1)
	var directions={KEY_UP:Vector2i.UP,KEY_DOWN:Vector2i.DOWN,KEY_LEFT:Vector2i.LEFT,KEY_RIGHT:Vector2i.RIGHT}
	if directions.has(key): move_cursor(directions[key])
	elif key==KEY_Z: confirm_cursor()
	elif key==KEY_X and moving: start_move()
	else: return
	get_viewport().set_input_as_handled()

func refresh_player_strategy(battle:Dictionary,tactical:bool) -> void:
	human_strategy_controls.visible=tactical and not battle.tactics.has("turn_boundary") and not battle.tactics.has("attack") and not battle.tactics.has("retreat") and not battle.tactics.has("scout")
	if not human_strategy_controls.visible:return
	var pending:Dictionary=battle.tactics.get("human_strategy",{})
	var choosing:bool=pending.is_empty()
	human_strategy_choices.visible=choosing
	human_strategy_target.visible=choosing
	human_strategy_prepare.visible=choosing
	human_strategy_confirm.visible=pending.get("stage","")=="confirm"
	human_strategy_cancel.visible=human_strategy_confirm.visible
	human_strategy_finish.visible=pending.get("stage","")=="result"
	if choosing:
		var selected_strategy:int=human_strategy_choices.get_selected_id() if human_strategy_choices.item_count else -1
		var selected_enemy:int=human_strategy_target.get_selected_id() if human_strategy_target.item_count else -1
		human_strategy_choices.clear()
		human_strategy_target.clear()
		var options:Dictionary=native.player_tactical_strategies(int(battle.tactics.selected))
		for choice in options.get("choices",[]):
			var strategy:int=int(choice.strategy)
			human_strategy_choices.add_item("%s · %d机动力" % [TACTICAL_STRATEGY_NAMES[strategy],int(choice.cost)],strategy)
			if strategy==selected_strategy:human_strategy_choices.select(human_strategy_choices.item_count-1)
		var attacking:bool=int(battle.tactics.get("side",128))==128
		for slot in range(12 if attacking else 11):
			if int(state.sram[0xdaa+slot*2 if attacking else 0xdc2+slot*3])>=241:continue
			human_strategy_target.add_item("敌军第 %d 队" % (slot+1),slot)
			if slot==selected_enemy:human_strategy_target.select(human_strategy_target.item_count-1)
		human_strategy_prepare.disabled=human_strategy_choices.item_count==0 or human_strategy_target.item_count==0 or int(battle.tactics.points)<4
		human_strategy_report.text="选择计策与敌军目标；可用计策由武将智力决定，目标须满足地形条件。"
	elif pending.stage=="confirm":
		human_strategy_confirm.text="确认施计（%d机动力）" % int(pending.cost)
		human_strategy_report.text="对敌军第 %d 队使用%s。尚未扣除机动力，取消不消耗本次施计费用。" % [int(pending.target_slot)+1,TACTICAL_STRATEGY_NAMES[int(pending.strategy)]]
	else:
		var result:Dictionary=pending.result
		var outcome:String="计策成功" if result.success else "计策失败"
		if int(result.troops_lost)>0:outcome+="，敌军损失 %d 人" % (int(result.troops_lost)*100)
		if int(result.hp_lost)>0:outcome+="，目标体力减少 %d" % int(result.hp_lost)
		if int(result.converted_slot)>=0:outcome+="，目标已加入本方"
		human_strategy_report.text="%s。已消耗 %d 机动力，剩余 %d。" % [outcome,int(result.cost),int(result.points_after)]
