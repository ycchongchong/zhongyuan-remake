extends Control
## Display only: every position and legal deployment square comes from C++.
var battle_state: Dictionary = {}

func update_battle(value: Dictionary) -> void:
	battle_state=value
	queue_redraw()

func _draw() -> void:
	if battle_state.is_empty() or battle_state.battle==null or not battle_state.battle.has("deployment"):
		return
	var scale_factor:float=minf(size.x/256.0,size.y/160.0)
	var cell_size:float=16.0*scale_factor
	var origin:Vector2=(size-Vector2(256,160)*scale_factor)/2.0
	var tactical:bool=battle_state.battle.has("tactics")
	var current:int=int(battle_state.battle.tactics.selected) if tactical else int(battle_state.battle.deployment.current)
	var area:Array=battle_state.battle.deployment_area
	if current>=0:
		for cell in range(160):
			if not area[cell]:continue
			var rect=Rect2(origin+Vector2(cell%16,cell/16)*cell_size,Vector2.ONE*cell_size)
			draw_rect(rect,Color(0.9,1.0,0.7,0.10))
			draw_rect(rect,Color(0.9,1.0,0.7,0.35),false,1)
	for side in range(2):
		for slot in range(11 if side==0 else 12):
			var offset:int=0xdc2+slot*3 if side==0 else 0xdaa+slot*2
			var id:int=int(battle_state.sram[offset])
			var position:int=int(battle_state.sram[offset+1])
			if id==255 or position<16 or position>=160:continue
			var at:Vector2=origin+Vector2(position%16,position/16)*cell_size
			var rect=Rect2(at,Vector2.ONE*cell_size)
			var defending:bool=battle_state.battle.deployment.get("side","defender" if battle_state.phase=="battle" else "attacker")=="defender"
			if tactical:defending=int(battle_state.battle.tactics.get("side",128))==0
			var active:bool=side==(1 if defending else 0) and slot==current
			if tactical and battle_state.battle.tactics.has("attack") and side==(0 if defending else 1) and slot==int(battle_state.battle.tactics.attack.target_slot):active=true
			if tactical and battle_state.battle.tactics.has("scout") and side==(0 if defending else 1) and slot==int(battle_state.battle.tactics.scout.target_slot):active=true
			if active:draw_rect(rect,Color("fff073"),false,2)
