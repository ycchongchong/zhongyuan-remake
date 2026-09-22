#include "zhongyuan/original_rom.hpp"
#include <fstream>
#include <iostream>
using namespace zhongyuan;
static std::vector<std::uint8_t> read(const std::string &path) {
    std::ifstream file(std::filesystem::u8path(path),std::ios::binary);
    if(!file)throw std::runtime_error("Cannot read "+path);
    return {std::istreambuf_iterator<char>(file),{}};
}
int main(int argc,char **argv) {try {
    if(argc!=2)throw std::runtime_error("Expected ROM path");
    OriginalRom rom(read(argv[1]));
    const std::string root=std::string(ZHONGYUAN_PROJECT_DIR)+"/reference/fixtures/unification-pictures/";
    const auto cases=Json::parse(read(root+"cases.json"));
    std::size_t pixels=0;
    for(const auto &row:cases){
        const auto expected=read(root+row["rgb"].get<std::string>());
        const auto actual=rom.unification_score_rgb(row["year"],row["month"],row["difficulty"],row["ruler"],row["score"]);
        if(actual!=expected){
            std::size_t differences=0;int first=-1;
            for(std::size_t i=0;i<actual.size();i+=3)if(!std::equal(actual.begin()+i,actual.begin()+i+3,expected.begin()+i)){
                if(first<0)first=int(i/3);++differences;
            }
            throw std::runtime_error(row["name"].get<std::string>()+": "+std::to_string(differences)+" different pixels, first ("+std::to_string(first%256)+","+std::to_string(first/256)+")");
        }
        pixels+=actual.size()/3;
    }
    for(const auto &parameters:std::vector<std::array<int,5>>{{-1,1,0,0,1},{1000,1,0,0,1},{200,0,0,0,1},{200,13,0,0,1},{200,1,3,0,1},{200,1,0,6,1},{200,1,0,0,-1},{200,1,0,0,101}}){
        bool rejected=false;
        try {rom.unification_score_rgb(parameters[0],parameters[1],parameters[2],parameters[3],parameters[4]);}
        catch(const std::out_of_range &){rejected=true;}
        if(!rejected)throw std::runtime_error("Invalid picture parameter accepted");
    }
    std::cout<<"Original ending picture: "<<cases.size()<<" original full screens, "<<pixels<<" pixels and 8 invalid inputs passed\n";
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}}
