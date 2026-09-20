#include "zhongyuan/original_rom.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
void check(bool ok,const std::string &message){if(!ok)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string &path){std::ifstream f(std::filesystem::u8path(path),std::ios::binary);check(bool(f),path);return {std::istreambuf_iterator<char>(f),{}};}
std::array<std::uint8_t,3> color(int index){switch(index){
 case 15:return {0,0,0};case 17:return {0,113,239};case 22:return {219,40,0};case 23:return {203,77,12};
 case 25:return {0,150,0};case 32:return {255,255,255};case 33:return {60,190,255};case 36:return {247,121,255};
 case 38:return {255,117,97};case 39:return {255,154,56};case 41:return {130,211,16};case 56:return {255,231,162};
 case 7:return {125,8,0};default:throw std::runtime_error("Unknown palette index "+std::to_string(index));}}
int main(int argc,char **argv){try{
 check(argc==2,"Expected ROM");OriginalRom rom(read(argv[1]));const std::string root=std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/";
 const auto rows=Json::parse(read(root+"tactical-picture.json"));std::size_t total=0;
 for(const auto &row:rows){
  const auto sram=read(root+row["sram"].get<std::string>()),expected=read(root+row["rgb"].get<std::string>());
  auto indices=rom.tactical_pixels(row["target"],sram);std::vector<std::uint8_t> actual;for(int index:indices){auto c=color(index);actual.insert(actual.end(),c.begin(),c.end());}
  check(actual.size()==expected.size(),"Original viewport size");int mismatch=0;for(std::size_t i=0;i<actual.size();i+=3)if(!std::equal(actual.begin()+i,actual.begin()+i+3,expected.begin()+i))++mismatch;
  check(mismatch==0,"Original tactical RGB "+row["rgb"].dump()+": "+std::to_string(mismatch)+" different pixels");total+=indices.size();
 }
 const auto raw=read(root+rows[0]["sram"].get<std::string>());auto fail=[&](int city,const std::vector<std::uint8_t>&data){try{rom.tactical_pixels(city,data);}catch(const std::out_of_range&){return;}throw std::runtime_error("Invalid picture input accepted");};
 fail(-1,raw);fail(30,raw);fail(0,{});auto bad=raw;bad[0xde3]=0x60;fail(0,bad);bad=raw;bad[0xdaa]=241;fail(0,bad);bad=raw;bad[0xdab]=160;fail(0,bad);
 auto absent=raw;absent[0xdaa]=255;absent[0xdab]=254;check(rom.tactical_pixels(12,absent).size()==40960,"Departed officer does not draw a stale position");
 absent=raw;absent[0xdab]=255;check(rom.tactical_pixels(12,absent).size()==40960,"Not-yet-deployed officer is hidden");
 std::cout<<"Original tactical picture: "<<rows.size()<<" reference views, "<<total<<" exact RGB pixels; invalid and absent records checked\n";
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
