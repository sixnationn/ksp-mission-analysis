#include "Worker.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace ksp;
using json=nlohmann::json;
namespace {
int checks=0;
void check(bool okay,const char* name){++checks;if(!okay)throw std::runtime_error(name);}
json request(){return {
    {"protocol_version",1},{"command","start"},{"request_id","test-1"},
    {"source",{{"mode","synthetic_fixture"},{"expected_snapshot_hash","synthetic-worker-v1"}}},
    {"grid",{{"central_body_id","star"},{"departure_body_id","home"},{"arrival_body_id","target"},
        {"launch_start_ut_s",0},{"launch_end_ut_s",0},{"launch_step_s",100},
        {"flight_min_s",2500},{"flight_max_s",2500},{"flight_step_s",100},
        {"reference_normal",{0,0,1}},{"branch","short"},{"direction","positive"},
        {"max_position_residual_m",0.1},{"max_velocity_residual_mps",1e-4},{"max_candidates",8},{"stable_seed",42},
        {"central_atmosphere_altitude_m",0.0},{"central_safety_margin_m",0.0}}},
    {"refine",{{"enabled",false}}}
};}
std::vector<json> run(const std::string& line,const std::function<bool()>& cancel=[](){return false;}){
    std::vector<json> messages;process_start_line(line,[&](const json& value){messages.push_back(value);},cancel);return messages;
}
bool has(const std::vector<json>& messages,const std::string& type){for(const auto& m:messages)if(m.value("type",std::string{})==type)return true;return false;}
void protocol_failures(){
    auto out=run("{");check(out.size()==1&&out[0]["type"]=="error"&&out[0]["code"]=="invalid_json","bad JSON error");
    auto q=request();q["protocol_version"]=2;out=run(q.dump());check(out[0]["code"]=="unsupported_version","version gate");
    q=request();q["command"]="erase";out=run(q.dump());check(out[0]["code"]=="invalid_request","command gate");
    q=request();q["source"]["expected_snapshot_hash"]="wrong";out=run(q.dump());check(out[0]["code"]=="source_mismatch"&&out.size()==1,"hash gate no partial result");
    q=request();q["grid"]["flight_step_s"]=0;out=run(q.dump());check(out[0]["type"]=="error"&&out[0]["code"]=="invalid_request","grid error");
}
void success_and_cancel(){
    auto q=request();auto out=run(q.dump());
    check(has(out,"started")&&has(out,"progress")&&has(out,"candidate")&&has(out,"complete"),"event sequence");
    check(!has(out,"error")&&!has(out,"cancelled"),"successful terminal status");
    for(const auto& m:out){check(m["protocol_version"]==1&&m["request_id"]=="test-1","envelope identity");
        if(m["type"]!="error")check(m["snapshot_hash"]=="synthetic-worker-v1"&&m["source_confidence"]=="synthetic_fixture","source identity");}
    const auto& candidate=*std::find_if(out.begin(),out.end(),[](const json& m){return m["type"]=="candidate";});
    check(candidate["status"]=="screened_seed"&&candidate["departure_ut_s"]==0&&candidate["flight_time_s"]==2500,"dated screened candidate");
    q["grid"]["launch_end_ut_s"]=200;
    int emitted_candidates=0;std::vector<json> partial;
    process_start_line(q.dump(),[&](const json& m){partial.push_back(m);if(m.value("type",std::string{})=="candidate")++emitted_candidates;},
        [&](){return emitted_candidates>=1;});
    check(has(partial,"candidate")&&has(partial,"cancelled")&&!has(partial,"complete"),"cancel retains partial candidate");
    const auto& cancelled=partial.back();check(cancelled["type"]=="cancelled"&&cancelled["retained_candidates"]>=1,"cancel counts retained");
    auto again=run(request().dump());check(out==again,"repeatable JSON sequence");
}
void refinement_status(){
    auto q=request();q["refine"]["enabled"]=true;
    const auto out=run(q.dump());
    auto it=std::find_if(out.begin(),out.end(),[](const json& m){return m.value("type",std::string{})=="refinement";});
    check(it!=out.end(),"refinement event present");
    check((*it)["status"]=="terminal_position_targeted","terminal position refinement reached");
    check((*it)["position_residual_m"]<200&&(*it)["strict_disagreement_m"]<200,"refinement bounds");
    std::cout<<"terminal position residual m="<<(*it)["position_residual_m"]
             <<" strict disagreement m="<<(*it)["strict_disagreement_m"]<<'\n';
}
void no_screened_seed(){
    auto q=request();q["grid"]["max_position_residual_m"]=1e-20;
    const auto out=run(q.dump());
    check(!has(out,"candidate")&&!has(out,"refinement")&&has(out,"complete"),"empty screen completes");
    check(out.back()["status"]=="no_screened_seed"&&out.back()["retained_candidates"]==0,"empty result is explicit");
}
void review_failures(){
    auto q=request();q["grid"]["launch_end_ut_s"]=200;q["grid"]["max_candidates"]=1;
    const auto out=run(q.dump());
    check(out.front()["type"]=="started","started before precompute");
    check(out.front()["non_interruptible_stages"].is_array(),"bounded cancellation disclosure");
    const auto& complete=out.back();
    check(complete["type"]=="complete"&&complete["ranked_candidates"].size()==1,"final top K visible");
    check(complete["ranked_candidates"][0]["candidate_id"].is_string(),"stable final candidate ID");
    q["grid"]["max_candidates"]=8;
    const auto full=run(q.dump());
    check(complete["ranked_candidates"][0]["candidate_id"]==full.back()["ranked_candidates"][0]["candidate_id"],"top K matches uncapped best");
    q["grid"]["max_candidates"]=1;
    const auto again=run(q.dump());check(complete["ranked_candidates"]==again.back()["ranked_candidates"],"ranked identity repeatable");
    int started=0;auto cancelled=run(q.dump(),[&](){return started++>0;});
    check(cancelled.front()["type"]=="started"&&cancelled.back()["type"]=="cancelled","precompute cancellation terminal");
    q=request();q["refine"]["enabled"]=true;
    bool refine_announced=false;std::vector<json> during_refine;
    process_start_line(q.dump(),[&](const json& m){during_refine.push_back(m);if(m.value("phase",std::string{})=="refinement")refine_announced=true;},
        [&](){return refine_announced;});
    check(during_refine.back()["type"]=="cancelled"&&!has(during_refine,"complete"),"refinement cancellation terminal");
    const auto refined=run(q.dump());const auto it=std::find_if(refined.begin(),refined.end(),[](const json& m){return m.value("type",std::string{})=="refinement";});
    check(it!=refined.end()&&(*it)["initial_offset_m"]==1000&&(*it)["target_offset_m"]==1000,"synthetic offsets declared");
    check((*it)["actual_target_position_m"].is_array()&&(*it)["actual_target_position_m"].size()==3,"actual target declared");
    check(!is_cancel_line("{\"protocol_version\":\"oops\",\"command\":\"cancel\",\"request_id\":\"test-1\"}","test-1"),"malformed cancel safe");
    check(is_cancel_line("{\"protocol_version\":1,\"command\":\"cancel\",\"request_id\":\"test-1\"}","test-1"),"valid cancel recognized");
    const auto failure=run(request().dump(),[]()->bool{throw std::runtime_error("callback fault");});
    check(failure.front()["type"]=="started"&&failure.back()["code"]=="work_failed","work failure distinct from invalid input");
    q=request();q["grid"]["central_body_id"]="missing";
    const auto invalid=run(q.dump());
    check(invalid.back()["type"]=="error"&&invalid.back()["code"]=="invalid_request","numerical request validation classified");
}
}
int main(){try{protocol_failures();success_and_cancel();refinement_status();no_screened_seed();review_failures();std::cout<<"PASS "<<checks<<" checks\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
