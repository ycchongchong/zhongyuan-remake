#include "zhongyuan/original_rom.hpp"
#include <stdexcept>

namespace zhongyuan {
std::vector<std::uint8_t> OriginalRom::tactical_pixels(int city,const std::vector<std::uint8_t> &sram) const {
    if(sram.size()!=8192)throw std::out_of_range("Tactical picture SRAM");
    const int attacker=sram[0xde3]>>4,defender=sram[0xde3]&15;
    if(attacker>5||defender>5)throw std::out_of_range("Tactical picture factions");
    auto pixels=battlefield_pixels(city);
    // B611-B638 loads the shared unit palette from bank3C. BBED-BC17
    // chooses complementary glyph sets, including Liu Bei's white colour.
    const int palette_table=0x10010+little(0x18016,2);
    const int palette[]={15,56,bytes_[palette_table+attacker*16+defender*2],bytes_[palette_table+attacker*16+defender*2+1]};
    int styles=defender==4?0x12:0x21;
    if(attacker!=4&&(styles&15)!=2)styles=1;
    for(int side:{0,128})for(int slot=0;slot<(side?11:12);++slot){
        const int base=side?0xdc2+slot*3:0xdaa+slot*2,id=sram[base],position=sram[base+1];
        if(id==255)continue;
        if(id>=241)throw std::out_of_range("Tactical picture officer");
        if(position==255)continue;
        if(position<16||position>=160)throw std::out_of_range("Tactical picture position");
        const int style=side?styles>>4:styles&15;
        const int table=0x8010+little(0x10243+style*2,2),troops=sram[0x438+id*8+6];
        // BAAD-BB08: tens/commander, troop low nibble, two-bit emblem.
        const int tiles[]={bytes_[table+17+((troops&15)>=10)+(side&&(sram[base+2]&128)?2:0)],
            bytes_[table+(troops&15)],bytes_[table+11+((troops>>4)&3)*2],bytes_[table+12+((troops>>4)&3)*2]};
        for(int q=0;q<4;++q){
            const auto pattern=tile(0x900+tiles[q]);
            for(int y=0;y<8;++y)for(int x=0;x<8;++x)
                pixels[(position/16*16+q/2*8+y)*256+position%16*16+q%2*8+x]=palette[pattern[y*8+x]];
        }
    }
    // Occupied castles show the complete unit glyph. The terrain's castle
    // picture becomes visible again when the ledger position is vacated.
    return pixels;
}
}
