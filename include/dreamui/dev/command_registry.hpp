#pragma once
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dreamui::dev {

// Based on the user's DreamUI developer prototype. The registry is UI-free:
// an optional future native Ink console, a text command inbox or a hotkey
// can all use the same commands. Invoke only on the DreamUI worker thread.
struct CommandContext {
    std::function<void(std::string)> append;
    bool* request_refresh = nullptr;
};

using CommandFn = std::function<void(CommandContext&, std::span<const std::string>)>;

class CommandRegistry {
public:
    void add(std::string name, std::string help, CommandFn fn);
    bool dispatch(CommandContext& ctx, std::string_view line, std::string& out) const;
    std::vector<std::string> names() const;
    std::string help_text() const;

private:
    struct Entry { std::string help; CommandFn fn; };
    std::map<std::string,Entry,std::less<>> commands_;
};
} // namespace dreamui::dev
