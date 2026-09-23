#include "Worker.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
int main(){
    std::string line;
    constexpr std::size_t max_request_bytes=1024*1024;
    while(true){
        const int ch=std::cin.get();
        if(ch==std::char_traits<char>::eof()){if(line.empty())return 0;break;}
        if(ch=='\n')break;
        if(line.size()>=max_request_bytes){
            std::cout<<nlohmann::json{{"protocol_version",1},{"request_id",""},{"type","error"},
                {"code","request_too_large"},{"detail","request line exceeds 1 MiB"}}.dump()<<std::endl;
            std::cout.flush();std::_Exit(0);
        }
        line.push_back(static_cast<char>(ch));
    }
    auto cancelled=std::make_shared<std::atomic<bool>>(false);
    std::string request_id;
    auto parsed=nlohmann::json::parse(line,nullptr,false);
    if(parsed.is_object()&&parsed.contains("request_id")&&parsed["request_id"].is_string())request_id=parsed["request_id"].get<std::string>();
    std::thread([cancelled,request_id](){
        constexpr std::size_t max_control_bytes=1024*1024;
        while(true){
            std::string incoming;
            while(true){
                const int ch=std::cin.get();
                if(ch==std::char_traits<char>::eof())return;
                if(ch=='\n')break;
                if(incoming.size()>=max_control_bytes)return;
                incoming.push_back(static_cast<char>(ch));
            }
            if(ksp::is_cancel_line(incoming,request_id)){*cancelled=true;return;}
        }
    }).detach();
    ksp::process_start_line(line,[](const nlohmann::json& event){std::cout<<event.dump()<<std::endl;},[cancelled](){return cancelled->load();});
    // The detached cancellation reader may still hold std::cin's lock in a
    // blocking read. Flush the terminal event, then end this one-request
    // process without running iostream static teardown against that reader.
    std::cout.flush();
    std::_Exit(0);
}
