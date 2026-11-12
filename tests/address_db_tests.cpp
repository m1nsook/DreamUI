#include "dreamui/address_db.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <array>
#include <string>
#include <cstdint>
int main(int argc,char** argv){
    if(argc!=2) return 2;
    auto path=std::filesystem::path(argv[1])/"test_addresses.json";
    const dreamui::AddressSections s{0x1000,0x2000,0x4000,0x1000,0x6000,0x1000,0x9000};
    const std::array<std::uint32_t,3> needed={0x783C1034u,0xC86F39B7u,0x4A610F64u};
    auto put=[&](std::string v){std::ofstream o(path,std::ios::binary);o << v;};
    const std::string sample="{\"Linker map timestamp\":\"6ad55daf\",\"Addresses\":["
       "{\"hash\":\"2017202228\",\"symbol\":\"loader\",\"offset\":\"0003:00000120\"},"
       "{\"hash\":\"3362732471\",\"offset\":\"0001:00000050\"},"
       "{\"hash\":\"1247874916\",\"offset\":\"0001:00000075\"}]}";
    put(sample+std::string(300,' '));
    const auto r=dreamui::read_address_database(path,0x6ad55daf,s,needed);
    if(!r.valid || r.matches.size()!=3 ||r.matches.at(needed[0]).rva!=0x6120 || r.matches.at(needed[1]).rva!=0x1050){
        std::cerr<<"lookup failed: "<<r.error<<" found="<<r.matches.size()<<'\n';return 1;
    }
    if(dreamui::read_address_database(path,0x6ad55dae,s,needed).valid) return 1;
    put(sample.substr(0,sample.find("0003:00000120"))+"0003:00FFFFFF"+sample.substr(sample.find("0003:00000120")+13)+std::string(300,' '));
    if(dreamui::read_address_database(path,0x6ad55daf,s,needed).valid) return 1;
    std::filesystem::remove(path);
    std::cout<<"address map bounds + timestamp tests pass\n";
}