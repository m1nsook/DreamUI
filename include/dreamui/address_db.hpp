#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string>

namespace dreamui {
struct AddressSections {
    std::uint32_t text_rva=0, text_size=0;
    std::uint32_t rdata_rva=0, rdata_size=0;
    std::uint32_t data_rva=0, data_size=0;
    std::uint32_t image_size=0;
};
struct SymbolAddress {
    std::uint16_t segment=0;
    std::uint32_t offset=0;
    std::uint32_t rva=0;
};
struct AddressDbResult {
    bool valid=false;
    std::uint32_t linker_timestamp=0;
    std::size_t entries_scanned=0;
    std::map<std::uint32_t,SymbolAddress> matches;
    std::string error;
};
// Parses the game's own address database with bounded memory usage.
// Only accepts symbols for a matching executable timestamp and PE section.
AddressDbResult read_address_database(const std::filesystem::path& file,
                                      std::uint32_t exe_timestamp,
                                      const AddressSections& sections,
                                      std::span<const std::uint32_t> wanted);
}