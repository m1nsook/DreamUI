#pragma once
#ifdef _WIN32
#include <filesystem>
#include <iosfwd>
namespace dreamui {
// Logs read-only metadata from the on-disk executable; never touches game memory.
void log_executable_probe(const std::filesystem::path& executable, std::ostream& log) noexcept;
}
#endif
