#include "zhongyuan/original_rom.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <filesystem>

int main(int argc,char **argv) {
    if(argc<3 || argc>4) { std::cerr<<"Usage: rom_inspect ROM.nes inventory.json [tiles.pgm]\n";return 2; }
    try {
        const auto input_path=std::filesystem::u8path(argv[1]);
        const auto output_path=std::filesystem::u8path(argv[2]);
        if(std::filesystem::exists(output_path) && std::filesystem::equivalent(input_path,output_path))
            throw std::runtime_error("Output must not overwrite the original ROM");
        std::ifstream input(input_path,std::ios::binary);
        if(!input) throw std::runtime_error("Cannot read ROM");
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
        const zhongyuan::OriginalRom rom(bytes);
        std::ofstream output(output_path);
        if(!output) throw std::runtime_error("Cannot write inventory");
        output<<rom.inventory().dump(2)<<'\n';
        output.close();if(!output) throw std::runtime_error("Inventory write failed");
        if(argc==4) {
            const auto tiles_path=std::filesystem::u8path(argv[3]);
            if(std::filesystem::exists(tiles_path) && std::filesystem::equivalent(input_path,tiles_path))
                throw std::runtime_error("Tiles must not overwrite ROM");
            std::vector<std::uint8_t> atlas(1024*1024);
            for(std::size_t t=0;t<rom.TILE_COUNT;++t) {
                const auto pixels=rom.tile(t);
                for(std::size_t y=0;y<8;++y) for(std::size_t x=0;x<8;++x)
                    atlas[((t/128)*8+y)*1024+(t%128)*8+x]=pixels[y*8+x]*85;
            }
            std::ofstream pgm(tiles_path,std::ios::binary);
            pgm<<"P5\n1024 1024\n255\n";
            pgm.write(reinterpret_cast<const char*>(atlas.data()),atlas.size());
            pgm.close();if(!pgm) throw std::runtime_error("CHR atlas write failed");
        }
        std::cout<<"Verified selected ROM; decoded 30 city slots, 241 officer slots and 16384 CHR tiles.\n";
        return 0;
    }catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
