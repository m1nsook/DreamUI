#include "dreamui/dev/command_inbox.hpp"
#include <fstream>
#include <iterator>
#include <string>

namespace dreamui::dev {
CommandBatch CommandInbox::poll(const std::filesystem::path& file) noexcept {
    CommandBatch out;
    try {
        std::error_code ec;
        if(!std::filesystem::is_regular_file(file,ec) || ec) {
            observed_.reset();
            processed_.reset();
            initialized_=true;
            return out;
        }
        Stamp stamp{std::filesystem::last_write_time(file,ec),0};
        if(ec)return out;
        stamp.size=std::filesystem::file_size(file,ec);
        if(ec)return out;
        if(!initialized_) {
            // An old request on disk must not execute at game startup.
            initialized_=true;
            observed_=processed_=stamp;
            return out;
        }
        if(!observed_ || !(*observed_==stamp)) {
            observed_=stamp;
            return out;
        }
        if(processed_ && *processed_==stamp)return out;
        // Debounce file saves: act only after the next unchanged poll.
        processed_=stamp;
        out.ready=true;
        if(stamp.size>4096) {
            out.error="command inbox exceeds 4 KiB limit";
            return out;
        }
        std::ifstream input(file,std::ios::binary);
        if(!input) {
            out.error="command inbox cannot be read";
            return out;
        }
        std::string content((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
        if(content.size()>4096 || content.find('\0')!=std::string::npos) {
            out.error="command inbox content invalid/oversized";
            return out;
        }
        if(content.size()>=3 &&
           static_cast<unsigned char>(content[0])==0xef &&
           static_cast<unsigned char>(content[1])==0xbb &&
           static_cast<unsigned char>(content[2])==0xbf) {
            content.erase(0,3); // Accept UTF-8 BOM from some editors.
        }
        std::size_t start=0;
        while(start<content.size()) {
            auto end=content.find('\n',start);
            if(end==std::string::npos)end=content.size();
            std::string command=content.substr(start,end-start);
            if(!command.empty() && command.back()=='\r')command.pop_back();
            const auto first=command.find_first_not_of(" \t");
            if(first!=std::string::npos && command[first]!='#') {
                if(command.size()>512 || out.lines.size()==16) {
                    out.lines.clear();
                    out.error="command inbox exceeds per-line or 16-line limit";
                    return out;
                }
                out.lines.push_back(std::move(command));
            }
            if(end==content.size())break;
            start=end+1;
        }
    } catch(...) {
        out.ready=true;
        out.error="command inbox inspection failed";
    }
    return out;
}
} // namespace dreamui::dev
