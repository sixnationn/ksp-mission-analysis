#include "Worker.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
int main(){
    std::string line;
    if(!std::getline(std::cin,line))return 0;
    auto cancelled=std::make_shared<std::atomic<bool>>(false);
    std::string request_id;
    auto parsed=nlohmann::json::parse(line,nullptr,false);
    if(parsed.is_object()&&parsed.contains("request_id")&&parsed["request_id"].is_string())request_id=parsed["request_id"].get<std::string>();
    std::thread([cancelled,request_id](){
        std::string incoming;
        while(std::getline(std::cin,incoming)){
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
