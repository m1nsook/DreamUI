#include "dreamui/address_db.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <optional>

namespace dreamui {
namespace {
std::optional<std::string_view> field(std::string_view record, std::string_view name) {
    const auto begin=record.find(name);
    if(begin==record.npos) return {};
    auto pos=record.find(':',begin+name.size());
    if(pos==record.npos) return {};
    ++pos;
    while(pos<record.size() && (record[pos]==' ' || record[pos]=='\t')) ++pos;
    if(pos==record.size()) return {};
    if(record[pos]=='"') {
        const auto first=++pos;
        const auto end=record.find('"',pos);
        if(end==record.npos) return {};
        return record.substr(first,end-first);
    }
    const auto first=pos;
    while(pos<record.size() && record[pos]!=',' && record[pos]!='}' && record[pos]!='\r' && record[pos]!='\n') ++pos;
    return record.substr(first,pos-first);
}
std::optional<std::uint32_t> number(std::string_view str, int radix) {
    if(str.substr(0,2)=="0x" || str.substr(0,2)=="0X") str.remove_prefix(2);
    if(str.empty() || str.size()>16) return {};
    std::uint64_t value=0;
    const auto result=std::from_chars(str.data(),str.data()+str.size(),value,radix);
    if(result.ec!=std::errc{} || result.ptr!=str.data()+str.size() || value>UINT32_MAX) return {};
    return static_cast<std::uint32_t>(value);
}
std::optional<SymbolAddress> offset_to_rva(std::string_view encoded, const AddressSections& s) {
    if(encoded.size()<6 || encoded[4]!=':') return {};
    const auto segment=number(encoded.substr(0,4),16);
    const auto offset=number(encoded.substr(5),16);
    if(!segment || !offset) return {};
    std::uint32_t base=0, length=0;
    switch(*segment){
        case 1: base=s.text_rva; length=s.text_size; break;
        case 2: base=s.rdata_rva; length=s.rdata_size; break;
        case 3: base=s.data_rva; length=s.data_size; break;
        default: return {};
    }
    if(!base || !length || *offset>=length || *offset>s.image_size || base>s.image_size-*offset) return {};
    return SymbolAddress{static_cast<std::uint16_t>(*segment),*offset,base+*offset};
}
}
AddressDbResult read_address_database(const std::filesystem::path& path,
                                      std::uint32_t exe_timestamp,
                                      const AddressSections& sections,
                                      std::span<const std::uint32_t> wanted) {
    AddressDbResult result;
    if(!exe_timestamp || !sections.image_size || wanted.empty()) {result.error="missing executable metadata or requested hashes";return result;}
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file){result.error="address database file missing";return result;}
    const auto size=file.tellg();
    if(size<256 || size>512LL*1024*1024){result.error="address database size outside safe limits";return result;}
    file.seekg(0);
    std::array<char,8192> header{};
    file.read(header.data(),header.size());
    const std::string_view lead(header.data(),static_cast<std::size_t>(file.gcount()));
    auto stamp=field(lead,"\"Linker map timestamp\"");
    if(!stamp){result.error="linker-map timestamp missing";return result;}
    const auto timestamp=number(*stamp,16);
    if(!timestamp){result.error="invalid linker-map timestamp";return result;}
    result.linker_timestamp=*timestamp;
    if(*timestamp!=exe_timestamp){result.error="linker-map timestamp does not match game executable";return result;}
    file.clear();file.seekg(0);
    std::string buffer;
    buffer.reserve(1024*1024+16384);
    std::array<char,1024*1024> chunk{};
    while(file){
        file.read(chunk.data(),chunk.size());
        const auto count=file.gcount();
        if(count>0) buffer.append(chunk.data(),static_cast<std::size_t>(count));
        std::size_t consumed=0;
        while(true){
            const auto start=buffer.find("{\"hash\"",consumed);
            if(start==std::string::npos){
                if(buffer.size()>16) buffer.erase(0,buffer.size()-16);
                break;
            }
            const auto end=buffer.find('}',start);
            if(end==std::string::npos){
                if(buffer.size()-start>16384){result.error="oversized address-database entry";return result;}
                buffer.erase(0,start);
                break;
            }
            ++result.entries_scanned;
            const auto record=std::string_view(buffer).substr(start,end-start+1);
            const auto hashField=field(record,"\"hash\"");
            if(hashField){
                const auto hash=number(*hashField,10);
                if(hash && std::find(wanted.begin(),wanted.end(),*hash)!=wanted.end()){
                    auto pathField=field(record,"\"offset\"");
                    if(!pathField){result.error="selected symbol lacks offset";return result;}
                    const auto addr=offset_to_rva(*pathField,sections);
                    if(!addr){result.error="selected symbol outside known executable section";return result;}
                    if(!result.matches.emplace(*hash,*addr).second){result.error="duplicate address hash";return result;}
                }
            }
            consumed=end+1;
        }
        if(count==0) break;
    }
    if(file.bad()){result.error="database read failure";return result;}
    if(result.entries_scanned==0){result.error="no address entries recognized";return result;}
    result.valid=true;
    return result;
}
}