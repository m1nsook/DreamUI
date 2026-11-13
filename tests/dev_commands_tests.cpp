#include "dreamui/dev/command_registry.hpp"
#include "dreamui/dev/command_inbox.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(x) do {if(!(x)){ \
    std::cerr<<"FAIL line "<<__LINE__<<" : "<<#x<<"\n";return 1;\
}}while(false)

namespace fs=std::filesystem;
int main() {
    using namespace dreamui::dev;
    CommandRegistry commands;
    std::vector<std::string> output;
    bool trigger=false;
    CommandContext ctx{
        [&](std::string line){output.push_back(std::move(line));},
        &trigger
    };
    commands.add("probe","request native diagnostic",[](CommandContext& c,std::span<const std::string> args){
        if(c.request_refresh)*c.request_refresh=true;
        c.append(args.empty()?"unspecified":args[0]);
    });
    commands.add("throw","simulated failure",[](CommandContext&,auto){
        throw std::runtime_error("bad input");
    });
    commands.add("!bad","invalid name",[](CommandContext&,auto){});
    std::string response;
    CHECK(commands.names().size()==2);
    CHECK(!commands.dispatch(ctx,"",response));
    CHECK(!commands.dispatch(ctx,"invalid",response));
    CHECK(!commands.dispatch(ctx,std::string(513,'x'),response));
    const std::string embedded_nul("probe\0ignored",13);
    CHECK(!commands.dispatch(ctx,embedded_nul,response));
    CHECK(!commands.dispatch(ctx,"throw",response) && response.find("bad input")!=std::string::npos);
    CHECK(commands.dispatch(ctx," probe    main-menu  ",response));
    CHECK(trigger && output.size()==1 && output.back()=="main-menu");
    CHECK(commands.help_text().find("probe - request native diagnostic")!=std::string::npos);
    std::string too_many="probe";
    for(int i=0;i<17;i++)too_many+=" a";
    CHECK(!commands.dispatch(ctx,too_many,response));

    const auto unique=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto folder=fs::temp_directory_path()/("dreamui-dev-commands-"+unique);
    const auto file=folder/"commands.txt";
    fs::create_directories(folder);
    auto write=[&](const std::string& content) {
        std::ofstream s(file,std::ios::binary|std::ios::trunc);
        if(!s)throw std::runtime_error("can't create test inbox");
        s<<content;s.close();
        const auto current=fs::last_write_time(file);
        fs::last_write_time(file,current+std::chrono::seconds(2));
    };
    write("probe stale-request\n");
    CommandInbox inbox;
    CHECK(!inbox.poll(file).ready); // never replay stale request at boot
    CHECK(!inbox.poll(file).ready);
    write("#comment\nprobe main-menu\nprobe inventory\n");
    CHECK(!inbox.poll(file).ready); // save stability/debounce
    const auto first=inbox.poll(file);
    CHECK(first.ready && first.error.empty() && first.lines.size()==2);
    CHECK(first.lines[0]=="probe main-menu" && first.lines[1]=="probe inventory");
    CHECK(!inbox.poll(file).ready); // no duplicate
    write("help\n");
    CHECK(!inbox.poll(file).ready);
    const auto second=inbox.poll(file);
    CHECK(second.ready && second.lines.size()==1 && second.lines[0]=="help");
    write(std::string(4100,'x'));
    CHECK(!inbox.poll(file).ready);
    const auto huge=inbox.poll(file);
    CHECK(huge.ready && !huge.error.empty() && huge.lines.empty());
    CHECK(!inbox.poll(file).ready); // don't repeatedly log the same error
    fs::remove(file);
    CHECK(!inbox.poll(file).ready);
    write("probe gameplay\n");
    CHECK(!inbox.poll(file).ready);
    const auto recreated=inbox.poll(file);
    CHECK(recreated.ready && recreated.lines.size()==1);
    CHECK(recreated.lines[0]=="probe gameplay");
    fs::remove_all(folder);
    std::cout<<"User-provided dev command registry and file inbox: tests passed\n";
}
