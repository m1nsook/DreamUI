#include "dreamui/pe_probe.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
namespace {
void w16(std::vector<std::uint8_t>& b, std::size_t p, std::uint16_t v){b[p]=v&255;b[p+1]=(v>>8)&255;}
void w32(std::vector<std::uint8_t>& b, std::size_t p, std::uint32_t v){for(int i=0;i<4;i++)b[p+i]=(v>>(8*i))&255;}
}
int main(){
    std::vector<std::uint8_t> invalid(20);
    if (dreamui::inspect_pe(invalid).valid) return 1;
    std::vector<std::uint8_t> data(0x500);
    w16(data,0,0x5A4D);w32(data,0x3C,0x80);
    w32(data,0x80,0x00004550);w16(data,0x84,0x8664);w16(data,0x86,1);
    w32(data,0x88,0x12345678);w16(data,0x94,0xF0);
    w16(data,0x98,0x20B);w32(data,0x98+56,0x3000);
    std::memcpy(data.data()+0x188,".rdata",6);
    w32(data,0x188+16,0x100);w32(data,0x188+20,0x300);
    std::memcpy(data.data()+0x300,"inkStyleResource",16);
    std::memcpy(data.data()+0x330,"inkWidget",9);
    auto result=dreamui::inspect_pe(data);
    if (!result.valid || result.sections.rdata_rva!=0 || !result.x64 || result.coff_timestamp!=0x12345678 || result.image_size!=0x3000 || result.rdata_bytes_scanned!=0x100 || !result.ink_style_resource_literal || result.ink_style_property_literal || !result.ink_widget_literal){
        std::cerr << "valid PE parsing failure: " << result.error << '\n'; return 1;
    }
    w32(data,0x188+20,0x490);
    if (dreamui::inspect_pe(data).valid) return 1;
    w32(data,0x188+20,0x300);w16(data,0x84,0x14C);
    if (dreamui::inspect_pe(data).valid) return 1;
    std::cout << "PE read-only parser: valid and corrupted cases pass\n";
}
