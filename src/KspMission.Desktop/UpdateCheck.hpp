#pragma once

#include "nlohmann/json.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace ksp::update {
enum class Kind {Current,Different,UnknownLocal};
struct LatestBuild {
    Kind kind;
    std::string commit;
    std::string url;
};

inline bool valid_commit(const std::string& value){
    return value.size()==40&&std::all_of(value.begin(),value.end(),[](char c){
        return (c>='0'&&c<='9')||(c>='a'&&c<='f');
    });
}

inline LatestBuild inspect_latest(const std::string& local_commit,const std::string& response){
    const auto document=nlohmann::json::parse(response);
    if(!document.is_object()||!document.contains("workflow_runs")||
       !document["workflow_runs"].is_array()||document["workflow_runs"].empty())
        throw std::runtime_error("No successful main build was returned");
    const auto& run=document["workflow_runs"].front();
    if(!run.is_object()||!run.value("id",nlohmann::json{}).is_number_integer()||
       !run.value("head_sha",nlohmann::json{}).is_string()||
       run.value("conclusion",std::string{})!="success"||
       run.value("status",std::string{})!="completed"||
       run.value("head_branch",std::string{})!="main")
        throw std::runtime_error("Unexpected successful-build response");
    const auto id=run["id"].get<long long>();
    const auto commit=run["head_sha"].get<std::string>();
    if(id<=0||!valid_commit(commit))throw std::runtime_error("Invalid successful-build identity");
    const auto kind=!valid_commit(local_commit)?Kind::UnknownLocal:
        local_commit==commit?Kind::Current:Kind::Different;
    return {kind,commit,
        "https://github.com/sixnationn/ksp-mission-analysis/actions/runs/"+std::to_string(id)};
}
}
