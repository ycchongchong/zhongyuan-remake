/* Local read-only study hooks: never execute bus read handlers. */
RETRO_API void retro_study_cpu(uint8_t *out) {
  unsigned a;
  for (a=0;a<65536;a++) {
    if(a<0x2000) out[a]=RAM[a&0x7ff];
    else if(a>=0x6000 && Page[a>>11]) out[a]=Page[a>>11][a];
    else out[a]=0;
  }
}
RETRO_API void retro_study_ppu(uint8_t *out) {
  unsigned a; extern uint8_t PALRAM[0x20];
  for(a=0;a<0x4000;a++) {
    if(a<0x2000) out[a]=VPage[a>>10] ? VPage[a>>10][a] : 0;
    else if(a<0x3f00) out[a]=vnapage[(a>>10)&3][a&0x3ff];
    else out[a]=PALRAM[a&31];
  }
}
RETRO_API unsigned retro_study_prg_offset(unsigned addr) {
  const uint8_t *p;
  if(addr<0x8000 || addr>0xffff || !Page[addr>>11]) return 0xffffffff;
  p=Page[addr>>11]+addr;
  if(p<PRGptr[0] || p>=PRGptr[0]+PRGsize[0]) return 0xffffffff;
  return (unsigned)(p-PRGptr[0]);
}
RETRO_API unsigned retro_study_ppuctrl(void) { return PPU[0]; }
