#include "dreamui/active_theme.hpp"
#include <fstream>
#include <iterator>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace dreamui {
bool valid_dream_id(std::string_view value) noexcept {
    if(value.empty() || value.size()>80)return false;
    bool separator=false;
    for(unsigned char ch:value) {
        if((ch>='a'&&ch<='z') || (ch>='0'&&ch<='9')) {
            separator=false;
        } else if(ch=='.' || ch=='-') {
            if(separator) return false;
            separator=true;
        } else return false;
    }
    // A name must never start/end with punctuation, nor have double separators.
    return !separator && value.front()!='.' && value.front()!='-';
}

std::string read_selected_dream_id(const std::filesystem::path& path) noexcept {
    try {
        std::error_code ec;
        if(!std::filesystem::is_regular_file(path,ec) || ec)return {};
        if(std::filesystem::file_size(path,ec)>82 || ec)return {};
        std::ifstream stream(path,std::ios::binary);
        if(!stream)return {};
        std::string content((std::istreambuf_iterator<char>(stream)),
                            std::istreambuf_iterator<char>());
        if(content.size()>82)return {};
        if(!content.empty() && content.back()=='\n')content.pop_back();
        if(!content.empty() && content.back()=='\r')content.pop_back();
        return valid_dream_id(content)?content:std::string{};
    } catch(...) {return {};}
}

bool write_selected_dream_id(const std::filesystem::path& path,
                             std::string_view id,std::string& error) noexcept {
    error.clear();
    if(!valid_dream_id(id)) {error="invalid DreamUI theme identity";return false;}
    try {
        auto temp=path;temp+=L".new";
        {
            std::ofstream out(temp,std::ios::binary|std::ios::trunc);
            if(!out){error="cannot open temporary active-theme file";return false;}
            out<<id<<'\n';
            out.flush();
            if(!out){error="cannot save active-theme preference";return false;}
        }
        std::error_code ec;
#if defined(_WIN32)
        // std::filesystem::rename replacement behavior differs on Windows.
        // Use the OS's atomic replace, without deleting the original first.
        const bool replaced=MoveFileExW(temp.c_str(),path.c_str(),
            MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        std::filesystem::rename(temp,path,ec);
        const bool replaced=!ec;
#endif
        if(!replaced) {
            std::filesystem::remove(temp,ec);
            error="active-theme preference atomic replacement failed";
            return false;
        }
        return true;
    } catch(...) {
        error="active-theme preference save exception";return false;
    }
}
}
