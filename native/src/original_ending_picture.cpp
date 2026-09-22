#include "zhongyuan/original_rom.hpp"
#include "original_ending_tiles.hpp"

namespace zhongyuan {
std::vector<std::uint8_t> OriginalRom::unification_score_rgb(int year,int month,int difficulty,int ruler,int score) const {
    if(year<0||year>999||month<1||month>12||difficulty<0||difficulty>2||ruler<0||ruler>5||score<0||score>100)
        throw std::out_of_range("Unification picture parameters");
    std::vector<std::uint8_t> rgb(256*240*3,0);
    const std::array<std::uint8_t,3> black={0,0,0},paper={255,231,162},white={255,255,255};
    const auto pixel=[&](int x,int y,const auto &color){for(int c=0;c<3;++c)rgb[(y*256+x)*3+c]=color[c];};
    for(int y=160;y<240;++y)for(int x=0;x<256;++x)pixel(x,y,paper);
    const auto glyph=[&](int index,int x,int y,bool light=false){
        const auto pattern=tile(index);
        for(int dy=0;dy<8;++dy)for(int dx=0;dx<8;++dx){
            if(light){if(pattern[dy*8+dx]==1)pixel(x+dx,y+dy,white);}
            else pixel(x+dx,y+dy,pattern[dy*8+dx]==0?black:paper);
        }
    };
    const int variant=score<60?0:score<80?2:1;
    for(int n=0;n<216;++n){
        const auto &cell=ending_picture::cells[variant][n];
        const auto pattern=tile(cell.tile);
        for(int y=0;y<8;++y)for(int x=0;x<8;++x)
            pixel(56+(n%18)*8+x,40+(n/18)*8+y,ending_picture::palettes[cell.palette][pattern[y*8+x]]);
    }
    // Sprite CHR page 109 is active above the dialogue split. The final PPU
    // dump has already switched this page and cannot reconstruct these glyphs.
    glyph(109*64+0x29,96,144,true);glyph(109*64+0x1c,112,144,true);
    glyph(109*64+0x3e,112,136,true);glyph(109*64+0x28,128,144,true);
    glyph(109*64+0x30+difficulty+1,152,144,true);
    const auto word=[&](int bank,int code,int x,int y){
        glyph(bank*64+(code&63),x,y);glyph(bank*64+((code+1)&63),x+8,y);
        glyph(bank*64+((code+16)&63),x,y+8);glyph(bank*64+((code+17)&63),x+8,y+8);
    };
    // The selected Chinese ROM retains different first-line font pages in
    // the low/middle endings. Preserve the observed glyphs rather than fixing
    // their text using a modern font or the final-frame PPU bank alone.
    const int date_font=variant==1?210:231;
    word(date_font,0x80,72,160);word(date_font,0x82,104,160);
    word(168,0xc0,184,160);word(168,0xc2,200,160); // honorific
    const int message[]={0x84,0x86,0x88,0x8e,0xa0,0xa2,0xa4,0xa6,0xa8,0xaa};
    for(int i=0;i<10;++i)word(231,message[i],56+i*16,176);
    word(231,0x8a,56,192);word(231,0xac,72,192);word(231,0xae,88,192);
    const auto number=[&](int value,int right,int y){
        do {glyph(168*64+0x30+value%10,right,y);right-=8;value/=10;}while(value);
    };
    number(year,64,168);number(month,96,168);
    if(score==100){
        // This Chinese ROM places the hundreds tile at x88, overwriting the
        // lower-left tile of 度, while the two zeros remain at x112/x120.
        // Preserve the six independently captured perfect-score screens.
        glyph(168*64+0x31,88,200);glyph(168*64+0x30,112,200);glyph(168*64+0x30,120,200);
    }else number(score,120,200);
    const auto name=name_pixels(true,ruler);
    for(int y=0;y<16;++y)for(int x=0;x<48;++x)pixel(136+x,160+y,name[y*48+x]==0?black:paper);
    return rgb;
}
}
