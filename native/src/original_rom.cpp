#include "zhongyuan/original_rom.hpp"
#include <stdexcept>
#include <utility>
#include <set>

namespace zhongyuan {
Json OriginalRom::tactical_strategy_tables() const {
    return {{"cost",raw(0x14be6,8)},{"damage",raw(0x1526f,8)},{"heavy_damage",raw(0x15277,8)},{"distance",raw(0x14eab,6)}};
}
Json OriginalRom::player_tactical_strategy_tables() const {
    return {{"tiers",raw(0x14bd2,10)},{"counts",raw(0x14be2,4)},
        {"groups",Json::array({raw(0x14c36,3),raw(0x14c39,4),raw(0x14c3d,2),raw(0x14c3f,4)})},
        {"terrain",raw(0x14bf6,64)},{"cost",raw(0x14be6,8)}};
}
std::array<int,2> OriginalRom::tactical_strategy_offset(int index) const {
    if(index<0||index>=48)throw std::out_of_range("Tactical strategy search index");
    auto decode=[](int value){return (value&128)?-(value&15):value;};
    return {decode(bytes_[0xb878+index*2]),decode(bytes_[0xb879+index*2])};
}
int OriginalRom::tactical_occupied_fort_threshold(int index,bool attack) const {
    if(index<0||index>255)throw std::out_of_range("Occupied-fort probability index");
    // AC19 can read beyond the four probability pairs after B614 changes X.
    return bytes_[0xac38+index+(attack?1:0)];
}
std::array<int,2> OriginalRom::tactical_nearby_offset(int index) const {
    if(index<0||index>=28)throw std::out_of_range("Tactical nearby search index");
    const int value=bytes_[0xb296+index];
    return {(value&128)?-((value>>4)&7):(value>>4)&7,(value&8)?-(value&7):value&7};
}
OriginalRom::OriginalRom(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {
    if (bytes_.size() != FILE_SIZE) throw std::runtime_error("Unsupported ROM size: expected 393232 bytes");
    const std::array<std::uint8_t,16> header = {0x4e,0x45,0x53,0x1a,0x08,0x20,0x33,0x10,0,0,0,0,0,0,0,0};
    if (!std::equal(header.begin(),header.end(),bytes_.begin())) throw std::runtime_error("ROM header does not match the selected Chinese version");
    if (crc32(bytes_) != EXPECTED_CRC32) throw std::runtime_error("ROM fingerprint mismatch; offsets must not be used for another version");
}
std::uint32_t OriginalRom::crc32(const std::vector<std::uint8_t> &bytes) {
    std::uint32_t crc = 0xffffffff;
    for (auto byte : bytes) {
        crc ^= byte;
        for (int bit=0;bit<8;++bit) crc = (crc>>1) ^ ((crc&1) ? 0xedb88320u : 0u);
    }
    return crc ^ 0xffffffff;
}
std::uint32_t OriginalRom::little(std::size_t offset, std::size_t width) const {
    std::uint32_t result=0;
    for (std::size_t i=0;i<width;++i) result |= std::uint32_t(bytes_.at(offset+i)) << (8*i);
    return result;
}
Json OriginalRom::raw(std::size_t offset, std::size_t length) const {
    Json result=Json::array();
    for (std::size_t i=0;i<length;++i) result.push_back(bytes_.at(offset+i));
    return result;
}
Json OriginalRom::city(std::size_t index) const {
    if (index>=CITY_COUNT) throw std::out_of_range("City index");
    const auto offset=CITY_OFFSET+index*CITY_STRIDE;
    Json slots=Json::array();
    for (std::size_t i=16;i<28;++i) slots.push_back(bytes_[offset+i]==255 ? Json(nullptr) : Json(bytes_[offset+i]));
    // Transcribed from the ROM's decoded city-name glyph table (0x262c).
    static const std::array<const char*,30> names={"辽东","幽州","并州","青州","冀州","西安阳","凉州","安定","长安","西平关","徐州","兖州","洛阳","新野","豫州","建业","扬州","建安","交州","郁林","江夏","荆州","衡阳","长沙","零陵","汉中","成都","涪陵","云南","永昌"};
    return {{"id",index},{"name",names[index]},{"name_layout",name_layout(false,index)},
        {"rom_file_offset",offset},{"runtime_cpu_address",0x6000+index*CITY_STRIDE},
        {"raw",raw(offset,CITY_STRIDE)},{"flags_raw",bytes_[offset]},
        {"faction_id_candidate",bytes_[offset]&7},{"faction_id",bytes_[offset]&7},
        {"map_position",{bytes_[0x1de75+index],bytes_[0x1de93+index]}},
        {"gold",little(offset+1,3)},{"land",little(offset+4,2)},
        {"commerce",little(offset+6,2)},{"population",little(offset+8,3)},
        {"control",bytes_[offset+14]},{"officer_slots",slots}};
}
Json OriginalRom::battlefield(int city) const {
    if(city<0||city>=30)throw std::out_of_range("Battlefield city");
    return {{"city",city},{"width",16},{"height",10},{"terrain",raw(0x3e060+city*224,160)},
        {"attributes",raw(0x3e020+city*224,64)}};
}
std::vector<std::uint8_t> OriginalRom::battlefield_pixels(int city) const {
    const auto map=battlefield(city);std::vector<std::uint8_t> pixels(256*160);
    constexpr int palette[]={0x0f,0x29,0x21,0x19,0x0f,0x29,0x27,0x07,0x0f,0x38,0x20,0x0f,0x0f,0x38,0x06,0x20};
    for(int cell=0;cell<160;++cell){
        const int terrain=map["terrain"][cell],cx=cell%16,cy=cell/16;
        const int pal=(map["attributes"][cy/2*8+cx/2].get<int>()>>((cy%2)*4+(cx%2)*2))&3;
        for(int q=0;q<4;++q){
            // Background CHR bank 36 at the top of the split-screen; the
            // end-of-frame PPU dump already contains the dialogue font bank.
            const auto background=tile(0x900+bytes_[0x5da1+terrain*4+q]);
            const auto castle=tile(0x24+q);
            for(int y=0;y<8;++y)for(int x=0;x<8;++x){
                const int bit=background[y*8+x];int color=palette[bit?pal*4+bit:0];
                if(terrain==0&&castle[y*8+x]){constexpr int colors[]={0,0x20,0x17,0x0f};color=colors[castle[y*8+x]];}
                pixels[(cy*16+(q/2)*8+y)*256+cx*16+(q%2)*8+x]=color;
            }
        }
    }
    return pixels;
}
Json OriginalRom::development_tables() const {
    // CPU B521-B5A1 chooses a proposal; B44B-B4D0 applies it.
    Json proposals=Json::array();
    for(int kind=0;kind<3;++kind) {
        Json row=Json::array();
        for(int i=0;i<18;++i)row.push_back({{"index",i},
            {"dialogue_id",bytes_[0x1999c+kind*18+i]},
            {"cost",bytes_[0x199d2+kind*18+i]}});
        proposals.push_back(row);
    }
    return {{"proposals",proposals},{"tier_counts",raw(0x19a08,3)},
        {"tier_starts",raw(0x19a0c,3)},{"intelligence_bonus",raw(0x1b4e1,16)},
        {"random_sequence",raw(0x1ea02,256)},{"calendar_control_bonus",raw(0x1d440,8)}};
}
std::vector<std::uint8_t> OriginalRom::initial_sram(int difficulty) const {
    if(difficulty<0||difficulty>2)throw std::out_of_range("Difficulty");
    // Original initialization: ABBC-ABE8, B12D-B1EF (PRG bank at file E010).
    std::vector<std::uint8_t> state(8192,0);
    std::copy_n(bytes_.begin()+CITY_OFFSET,1080,state.begin());
    // The original copies 2048 bytes, including bytes beyond the 241 records.
    std::copy_n(bytes_.begin()+OFFICER_OFFSET,2048,state.begin()+0x438);
    std::copy_n(bytes_.begin()+0xf200,24,state.begin()+0xd8d);
    std::fill(state.begin()+0xdaa,state.begin()+0xdda,255);
    std::fill(state.begin()+0xc30,state.begin()+0xc8d,255);
    std::fill(state.begin()+0xc8d,state.begin()+0xd7d,255);
    state[0xd85]=200;state[0xd87]=1;state[0xd88]=difficulty;state[0xd89]=128;
    return state;
}
Json OriginalRom::search_tables() const {
    // AEE4-B2B9. Keep the original outcome categories and item offsets.
    return {{"outcomes",raw(0x19cd6,88)},{"candidates",raw(0x19be6,240)},
        {"gold_tens",raw(0x1aff0,48)},{"item_offsets",raw(0x1b04b,6)},
        {"dialogues",raw(0x1af6e,10)}};
}
Json OriginalRom::ai_tables() const {
    return {{"weights",raw(0xbf0f,18)},{"city_tiers",raw(0xbf21,31)},
        {"city_bonus",raw(0xbf3f,32)},{"border_bonus",raw(0xbf4e,9)},
        {"gold_bonus",raw(0xbf57,12)},{"training_mask",raw(0xca1a,6)},
        {"training_choice",raw(0xcbbe,256)},{"terrain",raw(0xce12,30)},
        {"terrain_count",raw(0xce30,30)},{"recruit_threshold",raw(0xccbd,3)},
        {"troop_mix",raw(0x1f82c,26)},{"budget_multiplier",raw(0x1d7ac,3)}};
}
Json OriginalRom::command_tables() const {
    // C85F intentionally indexes the static record table with a 16-byte stride.
    Json award=Json::array();for(int i=0;i<241;++i)award.push_back({bytes_[OFFICER_OFFSET+i*16+5],bytes_[OFFICER_OFFSET+i*16+3]});
    return {{"award_reference",award},{"award_divisors",raw(0x1c867,4)},
        {"award_bonus",raw(0x1c86b,4)},{"control_multiplier",raw(0x1c933,11)},
        {"gift_results",raw(0x19d2e,48)},{"shop_types",raw(0x1cb9c,30)},
        {"shop_count_minus_two",raw(0x1cbba,30)}};
}
Json OriginalRom::deployment_tables(int city) const {
    if(city<0||city>=30)throw std::out_of_range("Deployment city");
    // PRG bank 31 header 800A/800C/800E: nine row limits and two sets
    // of sixteen candidate squares, shared by AI and manual deployment.
    return {{"row_limits",raw(0x33cd+city*9,9)},
            {"defenders",raw(0x34db+city*16,16)},
            {"attackers",raw(0x36bb+city*16,16)}};
}
int OriginalRom::command_books(int difficulty,int city_count) const {
    if(difficulty<0||difficulty>2||city_count<0||city_count>30)throw std::out_of_range("Command book table index");
    // CPU A0D1-A11E counts ownership and selects one of three ROM tables.
    return bytes_[0x19d5d+difficulty*30+city_count];
}
Json OriginalRom::officer(std::size_t index) const {
    if (index>=OFFICER_COUNT) throw std::out_of_range("Officer index");
    const auto offset=OFFICER_OFFSET+index*OFFICER_STRIDE;
    static const std::array<const char*,6> rulers={"袁绍","马腾","曹操","孙权","刘备","刘璋"};
    Json name=index<rulers.size() ? Json(rulers[index]) : Json(nullptr);
    if(index==145) name="赵云";
    return {{"id",index},{"name",name},{"name_layout",name_layout(true,index)},{"rom_file_offset",offset},
        {"runtime_cpu_address",0x6438+index*OFFICER_STRIDE},
        {"raw",raw(offset,OFFICER_STRIDE)},
        {"state_raw",bytes_[offset]}, {"stamina",bytes_[offset+1]},
        {"intelligence",bytes_[offset+2]},{"martial",bytes_[offset+3]},
        {"virtue",bytes_[VIRTUE_OFFSET+index]},
        {"loyalty",bytes_[offset+5]==255 ? Json(nullptr) : Json(bytes_[offset+5])},
        {"equipment_or_rank_raw",bytes_[offset+6]},
        {"archer_low_nibble_raw",bytes_[offset+4]&15},
        {"infantry",(bytes_[offset+4]>>4)*100},
        {"cavalry",(bytes_[offset+7]&15)*100},
        {"archers",(bytes_[offset+7]>>4)*100}};
}
std::array<std::uint8_t,64> OriginalRom::tile(std::size_t index) const {
    if (index>=TILE_COUNT) throw std::out_of_range("CHR tile index");
    std::array<std::uint8_t,64> pixels{};
    const auto offset=CHR_OFFSET+index*16;
    for (std::size_t y=0;y<8;++y) for (std::size_t x=0;x<8;++x) {
        const auto bit=7-x;
        pixels[y*8+x]=((bytes_[offset+y]>>bit)&1) | (((bytes_[offset+y+8]>>bit)&1)<<1);
    }
    return pixels;
}
Json OriginalRom::name_layout(bool is_officer,std::size_t index) const {
    Json columns=Json::array();std::size_t offset=0;std::uint8_t bank=0;
    if(is_officer) {
        if(index>=OFFICER_COUNT) throw std::out_of_range("Officer name index");
        const auto pointer=little(0x2022+index*2,2);
        if(pointer<0x8000 || pointer>=0xa000) throw std::runtime_error("Name pointer out of bank");
        offset=0x2010+pointer-0x8000;bank=bytes_.at(offset);
        for(std::size_t n=1;n<=4;++n) {
            const auto code=bytes_.at(offset+n);
            if(code==255) break;
            if(n>3) throw std::runtime_error("Officer name longer than three glyphs");
            columns.push_back(code);columns.push_back(code+1);
        }
    } else {
        if(index>=CITY_COUNT) throw std::out_of_range("City name index");
        offset=0x262c+index*8;bank=bytes_.at(offset);
        for(std::size_t n=1;n<=6;++n) columns.push_back(bytes_.at(offset+n)==1 ? Json(nullptr) : Json(bytes_.at(offset+n)));
    }
    return {{"file_offset",offset},{"chr_1k_bank",bank},{"columns",columns},
        {"format","Each column: top tile bank*64+(code&63)-16, bottom +16; 8x16 pixels. null is blank."}};
}
std::array<std::uint8_t,48*16> OriginalRom::name_pixels(bool is_officer,std::size_t index) const {
    std::array<std::uint8_t,48*16> pixels{};pixels.fill(1);
    const auto layout=name_layout(is_officer,index);
    const std::size_t bank=layout.at("chr_1k_bank");
    const auto &columns=layout.at("columns");
    for(std::size_t col=0;col<columns.size();++col) {
        if(columns[col].is_null()) continue;
        const auto code=columns[col].get<std::size_t>();
        for(std::size_t row=0;row<2;++row) {
            const auto source=tile(bank*64+(code&63)-16+row*16);
            for(std::size_t y=0;y<8;++y)for(std::size_t x=0;x<8;++x)
                pixels[(row*8+y)*48+col*8+x]=source[y*8+x];
        }
    }
    return pixels;
}
Json OriginalRom::world_map() const {
    // BE29-BE4F follows these pointers and scans (target, auxiliary-byte) pairs.
    // Keep directional auxiliary values unchanged, including asymmetric records.
    std::set<std::array<int,2>> unique_edges;
    Json links=Json::array();
    for(std::size_t i=0;i<CITY_COUNT;++i) {
        const auto pointer=little(0x19a50+i*2,2);
        if(pointer<0x8000 || pointer>=0xa000) throw std::runtime_error("Route pointer outside bank");
        auto offset=0x18010+pointer-0x8000;
        Json row=Json::array();
        for(std::size_t count=0;;++count,offset+=2) {
            if(offset>=0x1a010 || count>30) throw std::runtime_error("Unterminated routes");
            if(bytes_[offset]==255) break;
            const int target=bytes_[offset];
            if(target>=30 || target==static_cast<int>(i)) throw std::runtime_error("Invalid route target");
            row.push_back({{"target",target},{"aux_index_raw",bytes_.at(offset+1)},{"file_offset",offset}});
            unique_edges.insert({std::min(static_cast<int>(i),target),std::max(static_cast<int>(i),target)});
        }
        links.push_back(row);
    }
    Json edges=Json::array();for(auto e:unique_edges)edges.push_back(e);
    Json positions=Json::array(), sprites=Json::array();
    for(std::size_t i=0;i<CITY_COUNT;++i) positions.push_back({bytes_[0x1de75+i],bytes_[0x1de93+i]});
    for(std::size_t i=0;i<6;++i) sprites.push_back({{"tile",bytes_[0x1de69+i]},
        {"palette",bytes_[0x1de6f+i]},{"pixels",tile(bytes_[0x1de69+i])}});
    return {{"positions",positions},{"edges",edges},{"links",links},{"sprites",sprites},{"road_pointer_table_offset",0x19a50},
        {"coordinates_file_offsets",{0x1de75,0x1de93}},
        {"coordinate_semantics","NES OAM x and y; displayed sprite top is y+1"},
        {"road_evidence","Original road-screen pixel components: reference/fixtures/world-roads.json"}};
}
Json OriginalRom::inventory() const {
    Json cities=Json::array(), officers=Json::array();
    for (std::size_t i=0;i<CITY_COUNT;++i) cities.push_back(city(i));
    for (std::size_t i=0;i<OFFICER_COUNT;++i) officers.push_back(officer(i));
    return {{"schema","original-rom-inventory-v2"},
        {"rom_sha256","9658ef6e04fa725bd52c326745768abeae72179b4ecc882e0b7ceb4ff7484959"},
        {"crc32","b0cf9573"},{"file_size",FILE_SIZE},{"mapper",19},
        {"prg_bytes",131072},{"chr_bytes",262144},{"chr_tile_count",TILE_COUNT},
        {"offset_basis","absolute file offsets, including 16-byte iNES header"},
        {"scope","Initial ROM tables, not the state after AI turns; unknown fields retained raw"},
        {"field_evidence",{
            {"city_fields","Offsets matched to Xinye screen: gold 200, land 25, commerce 20, population 19000, control 55"},
            {"officer_fields","Matched to Liu Bei and Zhao Yun query screens, including different troop compositions"},
            {"remaining_records","Same table layout; not individually verified in game yet"},
            {"faction_id","Low three bits confirmed by CPU A49B-A4A6 and DE28-DE2C; high bits retained raw"},
            {"counts","Contiguous record-layout inference from adjacent table starts; initialization loop audit pending"}}},
        {"cities",cities},{"officers",officers},{"world_map",world_map()}};
}
int OriginalRom::tactical_mobility(int units) const {
    if(units<1||units>11)throw std::out_of_range("Tactical unit count");
    return bytes_[0x14557+units]; // C547, count zero is never a playable turn.
}
Json OriginalRom::clash_terrain(int pair) const {
    if(pair<0||pair>63)throw std::out_of_range("clash terrain pair");
    return {{"map",bytes_[0x10288+pair]},{"orientation",bytes_[0x102c0+pair]}};
}
int OriginalRom::clash_position(int side,int formation,int unit) const {
    if(side<0||side>1||formation<0||formation>3||unit<0||unit>10)throw std::out_of_range("clash formation position");
    return bytes_[0x10333+side*44+formation*11+unit];
}
Json OriginalRom::clash_map(int scene) const {
    if(scene<0||scene>15)throw std::out_of_range("clash scene");
    return raw(0x3c070+scene*224,160);
}
int OriginalRom::clash_direction(int index) const {
    if(index<0||index>=128)throw std::out_of_range("clash direction table");
    return bytes_[0x1e7dd+index]; // Fixed-bank E7CD, shared by all four unit kinds.
}
// Initial, stationary clash board: 9B1F palette and 9DBD tile/attribute writes.
// CHR pages 40/42; later animation, portraits and command overlays are separate.
std::vector<std::uint8_t> OriginalRom::clash_rgb(int scene,int first_faction,int second_faction,const Json &units) const {
    if(scene<0||scene>15||first_faction<0||first_faction>5||second_faction<0||second_faction>5||!units.is_array()||units.size()!=66)throw std::out_of_range("clash image");
    for(const auto &v:units)if(!v.is_number_integer()||v<Json(0)||v>Json(255))throw std::out_of_range("clash image unit");
    auto color=[](int index)->std::array<std::uint8_t,3>{
        switch(index){
            case 0:return {117,117,117};case 15:return {0,0,0};case 16:return {190,190,190};
            case 17:return {0,113,239};case 22:return {219,40,0};case 23:return {203,77,12};
            case 33:return {60,190,255};case 36:return {247,121,255};case 38:return {255,117,97};
            case 39:return {255,154,56};case 41:return {130,211,16};case 56:return {255,231,162};
            default:throw std::out_of_range("clash palette");
        }
    };
    const int first=bytes_[0x103ad+first_faction*16+second_faction*2],second=bytes_[0x103ae + first_faction*16+second_faction*2];
    const int left=bytes_[0x1038b+scene*2],right=bytes_[0x1038c+scene*2];
    const int palette[]={15,56,first,left,15,56,second,right,15,56,first,right,15,56,second,left};
    std::array<int,160> occupants;occupants.fill(-1);
    for(int i=0;i<66;i+=3)if(units[i]!=255){const int position=units[i+2];if(position<16||position>=160)throw std::out_of_range("clash image position");occupants[position]=i;}
    std::vector<std::uint8_t> rgb(256*160*3);
    for(int cell=0;cell<160;++cell){
        const int x=cell%16,y=cell/16,terrain=bytes_[0x3c070+scene*224+cell];
        int pal=(bytes_[0x3c030+scene*224+(y/2)*8+x/2]>>((y%2)*4+(x%2)*2))&3;
        int table=0x1092c+terrain*4;
        if(occupants[cell]>=0){
            const int i=occupants[cell],side=i>=33,kind=units[i].get<int>()&3;
            table=0x1079b+((units[i].get<int>()&128?4:0)+kind+(terrain==5?8:0))*4;
            pal=side?(pal==1?1:3):(pal?2:0);
        }
        for(int q=0;q<4;++q){
            const int id=bytes_[table+q];const auto pattern=tile(0x1000+id+(id>=64?64:0));
            for(int v=0;v<8;++v)for(int u=0;u<8;++u){
                const int bit=pattern[v*8+u];const auto c=color(palette[bit?pal*4+bit:0]);
                const int at=((y*16+q/2*8+v)*256+x*16+q%2*8+u)*3;
                std::copy(c.begin(),c.end(),rgb.begin()+at);
            }
        }
    }
    return rgb;
}
int OriginalRom::clash_retreat_threshold(int first_count,int second_count) const {
    if(first_count<0||first_count>10||second_count<0||second_count>10)throw std::out_of_range("clash troop-count table");
    return bytes_[0xbc4f+(first_count<<4)+second_count]; // BC3F in the tactical AI bank.
}
int OriginalRom::clash_desertion_threshold(int officer) const {
    if(officer<0||officer>=241)throw std::out_of_range("clash retreat officer");
    return bytes_[0x12d0d+bytes_[0x1083b+officer]/10];
}
int OriginalRom::duel_ai_choice(int own_hp,int enemy_hp,int random,int frame) const {
    if(own_hp<0||own_hp>100||enemy_hp<0||enemy_hp>100||random<0||random>255||frame<0||frame>255)throw std::runtime_error("Invalid duel AI input");
    const int row=bytes_[0x113b3+enemy_hp/10],column=bytes_[0x113be + own_hp/10]+(random&3);
    const auto pointer=little(0x10d2a+row,2);
    const int packed=bytes_[0x10010+(pointer-0x8000)+column];
    return (packed>>((frame&1)?4:0))&15;
}
int OriginalRom::clash_desertion_size(int choice) const {
    if(choice<0||choice>3)throw std::out_of_range("clash retreat size");
    return bytes_[0x12d17+choice];
}

// B0E7/B0C1: AI uses the complete high nibble, halved, OR terrain.
int OriginalRom::tactical_ai_step_cost(int officer_byte,int terrain) const {
    if(officer_byte<0||officer_byte>255||terrain<0||terrain>255)throw std::out_of_range("AI movement cost input");
    const int index=((officer_byte&240)>>1)|terrain;
    return (bytes_[0xb0d1+index]*((officer_byte&15)<6?2:3))&255;
}
int OriginalRom::tactical_step_cost(int officer_byte,int terrain) const {
    const int value=bytes_[0x10303+(officer_byte&0x30)+(terrain&15)];
    return (value*((officer_byte&15)<6?2:3))&255; // Original 8-bit ASL/ADC.
}

} // namespace zhongyuan
