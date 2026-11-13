#include "dreamui/dev/command_registry.hpp"
#include <exception>

namespace dreamui::dev {

void CommandRegistry::add(std::string name,std::string help,CommandFn fn) {
    if(name.empty() || name.size()>32 || !fn)return;
    for(const unsigned char ch:name) {
        if(!((ch>='a'&&ch<='z') || (ch>='0'&&ch<='9') || ch=='-' || ch=='_'))
            return;
    }
    commands_.insert_or_assign(std::move(name),Entry{std::move(help),std::move(fn)});
}

bool CommandRegistry::dispatch(CommandContext& ctx,
                               std::string_view line,std::string& out) const {
    if(line.size()>512) {out="command exceeds 512 bytes";return false;}
    for(const char c:line) {
        if(c=='\0' || c=='\r' || c=='\n') {
            out="command contains invalid control characters";return false;
        }
    }
    while(!line.empty() && (line.front()==' ' || line.front()=='\t'))
        line.remove_prefix(1);
    if(line.empty()) {out="empty command";return false;}
    const auto end=line.find_first_of(" \t");
    const auto name=line.substr(0,end);
    const auto it=commands_.find(name);
    if(it==commands_.end()) {
        out="unknown command: ";
        out.append(name);
        return false;
    }
    std::vector<std::string> args;
    if(end!=std::string_view::npos) {
        auto rest=line.substr(end);
        std::size_t pos=0;
        while(pos<rest.size()) {
            while(pos<rest.size() && (rest[pos]==' ' || rest[pos]=='\t')) ++pos;
            if(pos>=rest.size())break;
            const auto begin=pos;
            while(pos<rest.size() && rest[pos]!=' ' && rest[pos]!='\t') ++pos;
            if(args.size()==16) {out="command has more than 16 arguments";return false;}
            args.emplace_back(rest.substr(begin,pos-begin));
        }
    }
    try {
        it->second.fn(ctx,args);
    } catch(const std::exception& e) {
        out="command exception: ";
        out.append(e.what());
        return false;
    } catch(...) {
        out="command raised an unknown exception";
        return false;
    }
    out="ok: ";
    out.append(name);
    return true;
}

std::vector<std::string> CommandRegistry::names() const {
    std::vector<std::string> result;
    result.reserve(commands_.size());
    for(const auto& [name,entry]:commands_)result.push_back(name);
    return result;
}

std::string CommandRegistry::help_text() const {
    std::string out;
    for(const auto& [name,entry]:commands_) {
        out+="  ";
        out+=name;
        if(!entry.help.empty()) {
            out+=" - ";
            out+=entry.help;
        }
        out+='\n';
    }
    return out;
}
} // namespace dreamui::dev
