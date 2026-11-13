#include "dreamui/active_theme.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs=std::filesystem;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
int main(){
    using namespace dreamui;
    CHECK(valid_dream_id("dreamui.default"));
    CHECK(valid_dream_id("my.custom-theme"));
    CHECK(valid_dream_id("x9"));
    CHECK(!valid_dream_id(""));
    CHECK(!valid_dream_id("../override"));
    CHECK(!valid_dream_id("DreamUI.Default"));
    CHECK(!valid_dream_id("name with spaces"));
    CHECK(!valid_dream_id("-bad"));
    CHECK(!valid_dream_id(".hidden"));
    CHECK(!valid_dream_id("ends."));
    CHECK(!valid_dream_id("bad..name"));
    CHECK(!valid_dream_id("bad--name"));
    CHECK(!valid_dream_id("bad.-name"));
    CHECK(!valid_dream_id("bad/name"));
    CHECK(!valid_dream_id("bad\\name"));
    CHECK(!valid_dream_id(std::string(81,'a')));
    const auto unique=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto dir=fs::temp_directory_path()/("dreamui-active-theme-"+unique);
    fs::create_directories(dir);
    const auto file=dir/"active-theme.txt";
    std::string error;
    CHECK(read_selected_dream_id(file).empty());
    CHECK(!write_selected_dream_id(file,"../bad",error));
    CHECK(!fs::exists(file));
    CHECK(write_selected_dream_id(file,"dreamui.default",error));
    CHECK(read_selected_dream_id(file)=="dreamui.default");
    CHECK(write_selected_dream_id(file,"my.custom-theme",error));
    CHECK(read_selected_dream_id(file)=="my.custom-theme");
    {
        std::ofstream out(file,std::ios::binary|std::ios::trunc);
        out<<std::string(130,'a');
    }
    CHECK(read_selected_dream_id(file).empty());
    {
        std::ofstream out(file,std::ios::binary|std::ios::trunc);
        out<<"../malicious";
    }
    CHECK(read_selected_dream_id(file).empty());
    {
        std::ofstream out(file,std::ios::binary|std::ios::trunc);
        out<<"custom.theme\r\n";
    }
    CHECK(read_selected_dream_id(file)=="custom.theme");
    fs::remove_all(dir);
    std::cout<<"Theme selection validates identities and persists across sessions\n";
}
