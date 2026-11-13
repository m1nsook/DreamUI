#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dreamui::dev {

struct CommandBatch {
    bool ready=false;
    std::vector<std::string> lines;
    std::string error;
};

// File-based diagnostic command adapter. Never executes an inbox left from
// a previous game session. The file must be created/edited AFTER startup.
// Needs two identical filesystem observations before reading the file.
// Pure C++ and no ImGui, DX12 hooks, game memory access or network services.
class CommandInbox {
public:
    CommandBatch poll(const std::filesystem::path& file) noexcept;
private:
    struct Stamp {
        std::filesystem::file_time_type modified{};
        std::uintmax_t size=0;
        bool operator==(const Stamp&) const = default;
    };
    bool initialized_=false;
    std::optional<Stamp> observed_,processed_;
};
} // namespace dreamui::dev
