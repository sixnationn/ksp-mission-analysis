#include "Worker.hpp"
#include "RuntimeReader.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
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
std::vector<json> run(const std::string& line,const std::function<bool()>& cancel=[](){return false;},
                      const std::function<void()>& after_read={}){
    std::vector<json> messages;process_start_line(line,[&](const json& value){messages.push_back(value);},cancel,after_read);return messages;
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
    check(complete["ranked_candidates"][0]["departure_ut_s"]==full.back()["ranked_candidates"][0]["departure_ut_s"]&&
          complete["ranked_candidates"][0]["flight_time_s"]==full.back()["ranked_candidates"][0]["flight_time_s"],
          "top K matches uncapped best dates");
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
json runtime_document(){
    constexpr double mu=3.986004418e14,r=1e7,R=1.2e7,t=2500;
    const double wa=std::sqrt(mu/(r*r*r)),wb=std::sqrt(mu/(R*R*R)),phase=std::acos(-1.0)/2-wb*t;
    return {{"schema_version",1},{"confidence","runtime_observed_uncompared"},
        {"capture",{{"exporter_id","KspMission.RuntimeExporter"},{"exporter_version","1"},{"game_version","1.12.5"},
            {"save_id","worker-synthetic"},{"capture_ut_s",0.0},{"principia_loaded",true},
            {"state_source","principia_celestial_from_parent"},{"mods",json::array({{{"id","Principia"},{"version","test"}}})}}},
        {"frame",{{"origin","system_barycenter"},{"axes","principia_alicesun_frozen_at_capture"},{"handedness","right"},
            {"inertial",true},{"source_frame","Principia/AliceSun"},{"transform_method","parent_relative_sum_then_com_translation"},{"transform_version","1"}}},
        {"bodies",json::array({
            {{"id","star"},{"parent_id",nullptr},{"mu_m3_s2",mu},{"radius_m",1e6},{"atmosphere_boundary_m",nullptr},
                {"state_epoch_ut_s",0.0},{"position_m",{0,0,0}},{"velocity_mps",{0,0,0}}},
            {{"id","home"},{"parent_id","star"},{"mu_m3_s2",1.0},{"radius_m",0.01},{"atmosphere_boundary_m",nullptr},
                {"state_epoch_ut_s",0.0},{"position_m",{r,0,0}},{"velocity_mps",{0,r*wa,0}}},
            {{"id","target"},{"parent_id","star"},{"mu_m3_s2",1.0},{"radius_m",0.01},{"atmosphere_boundary_m",nullptr},
                {"state_epoch_ut_s",0.0},{"position_m",{R*std::cos(phase),R*std::sin(phase),0}},
                {"velocity_mps",{-R*wb*std::sin(phase),R*wb*std::cos(phase),0}}}})},
        {"calendar",{{"day_duration_s",86400},{"display_origin_ut_s",0},{"use_leap_years",false},
            {"month_lengths",{31,28,31,30,31,30,31,31,30,31,30,31}}}}};
}
void runtime_mode(){
    const auto path=std::filesystem::temp_directory_path()/("ksp-worker-runtime-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
    auto write=[&](const std::string& bytes){std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));file.close();};
    const auto bytes=runtime_document().dump();write(bytes);const auto hash=sha256_hex(bytes);
    auto q=request();q["source"]={{"mode","runtime_snapshot"},{"path",path.string()},{"expected_snapshot_hash",hash}};
    q["ephemeris"]={{"end_ut_s",5000},{"step_s",1},{"max_position_fit_error_m",10},{"max_velocity_fit_error_mps",0.003}};
    const auto accepted=run(q.dump());
    check(has(accepted,"started")&&has(accepted,"candidate")&&has(accepted,"complete"),"runtime screen accepts validated fixture");
    for(const auto& event:accepted)if(event.value("type",std::string{})!="error")
        check(event["snapshot_hash"]==hash&&event["source_confidence"]=="runtime_observed_uncompared","runtime provenance on events");
    check(accepted.front()["force_model"]=="newtonian_point_mass"&&accepted.front()["coverage_end_ut_s"]==5000&&
          accepted.front()["source_mode"]=="runtime_snapshot"&&accepted.front()["units"]=="SI","runtime started model and coverage");
    check(accepted.back()["ranked_candidates"][0]["candidate_id"].get<std::string>().find(hash)==0&&
          accepted.back()["ranked_candidates"][0]["snapshot_hash"]==hash&&
          accepted.back()["ranked_candidates"][0]["source_confidence"]=="runtime_observed_uncompared"&&
          accepted.back()["ranked_candidates"][0]["status"]=="screened_seed","ranked runtime identity");
    int candidates=0;std::vector<json> partial;
    process_start_line(q.dump(),[&](const json& e){partial.push_back(e);if(e.value("type",std::string{})=="candidate")++candidates;},[&](){return candidates>0;});
    check(partial.back()["type"]=="cancelled"&&partial.back()["snapshot_hash"]==hash&&
          partial.back()["ranked_candidates"].size()>=1,"runtime cancellation retains ranked source identity");
    // Mutating the path after started cannot change the in-memory source for this run.
    std::vector<json> immutable;bool replaced=false;
    process_start_line(q.dump(),[&](const json& e){immutable.push_back(e);if(!replaced&&e.value("type",std::string{})=="started"){write("changed");replaced=true;}},[](){return false;});
    check(immutable.back()["type"]=="complete"&&immutable.back()["snapshot_hash"]==hash,"runtime bytes read once");write(bytes);
    const auto grown=run(q.dump(),[](){return false;},[&](){std::ofstream append(path,std::ios::binary|std::ios::app);append<<"x";});
    check(grown.size()==1&&grown.back()["type"]=="error"&&grown.back()["code"]=="source_changed"&&
          !has(grown,"started"),"same-handle appended bytes rejected before hash acceptance");write(bytes);
    auto bad=q;bad["source"]["path"]=path.string()+".missing";check(run(bad.dump()).back()["type"]=="error","unreadable path rejects");
    bad=q;bad["source"]["expected_snapshot_hash"]=std::string(64,'0');check(run(bad.dump()).back()["code"]=="source_mismatch","runtime bad hash rejects");
    write("{bad");bad=q;bad["source"]["expected_snapshot_hash"]=sha256_hex("{bad");check(run(bad.dump()).back()["type"]=="error","runtime bad JSON rejects");write(bytes);
    auto document=runtime_document();document["capture"]["principia_loaded"]=false;auto altered=document.dump();write(altered);
    bad=q;bad["source"]["expected_snapshot_hash"]=sha256_hex(altered);check(run(bad.dump()).back()["type"]=="error","runtime bad provenance rejects");
    document=runtime_document();document["bodies"][1].erase("position_m");altered=document.dump();write(altered);
    bad["source"]["expected_snapshot_hash"]=sha256_hex(altered);check(run(bad.dump()).back()["type"]=="error","runtime missing state rejects");
    document=runtime_document();document["bodies"][1].erase("atmosphere_boundary_m");altered=document.dump();write(altered);
    bad["source"]["expected_snapshot_hash"]=sha256_hex(altered);check(run(bad.dump()).back()["type"]=="error","runtime missing atmosphere rejects");write(bytes);
    bad=q;bad["grid"]["departure_body_id"]="unknown";check(run(bad.dump()).back()["type"]=="error","unknown runtime body rejects");
    bad=q;bad["ephemeris"]["end_ut_s"]=-1;check(run(bad.dump()).back()["type"]=="error","negative end rejects");
    bad=q;bad["ephemeris"]["step_s"]=0;check(run(bad.dump()).back()["type"]=="error","zero step rejects");
    bad=q;bad["ephemeris"]["step_s"]=-1;check(run(bad.dump()).back()["type"]=="error","negative step rejects");
    bad=q;bad["ephemeris"]["step_s"]="NaN";check(run(bad.dump()).back()["type"]=="error","nonfinite typed step rejects");
    bad=q;bad["ephemeris"]["max_position_fit_error_m"]=0;check(run(bad.dump()).back()["type"]=="error","zero fit budget rejects");
    bad=q;bad["ephemeris"]["max_velocity_fit_error_mps"]=-1;check(run(bad.dump()).back()["type"]=="error","negative velocity fit budget rejects");
    bad=q;bad["ephemeris"]["end_ut_s"]=100001;check(run(bad.dump()).back()["type"]=="error","excess integration steps reject");
    bad=q;bad["grid"]["flight_max_s"]=5100;check(run(bad.dump()).back()["type"]=="error","grid coverage rejects");
    bad=q;bad["source"]["mode"]="raw_config";check(run(bad.dump()).back()["type"]=="error","unsupported source rejects");
    bad=q;bad["source"]["expected_snapshot_hash"]=hash.substr(0,63)+"A";check(run(bad.dump()).back()["type"]=="error","uppercase hash rejects");
    write(std::string(16*1024*1024+1,'x'));bad=q;check(run(bad.dump()).back()["type"]=="error","oversized runtime rejects");
    std::filesystem::remove(path);
}
void candidate_identity(){
    GridRequest grid;grid.central_body_id="star";grid.departure_body_id="home";grid.arrival_body_id="target";
    grid.launch_start_ut_s=0;grid.launch_end_ut_s=100;grid.launch_step_s=100;
    grid.flight_min_s=2500;grid.flight_max_s=2600;grid.flight_step_s=100;
    grid.reference_normal={0,0,1};grid.branch=TransferBranch::short_path;grid.direction=AngularMomentumDirection::positive;
    grid.max_position_residual_m=1;grid.max_velocity_residual_mps=0.001;grid.central_atmosphere_altitude_m=0;
    grid.max_candidates=8;grid.stable_seed=42;
    DatedLegCandidate candidate;candidate.snapshot_hash="same-snapshot";candidate.departure_ut_s=0;candidate.flight_time_s=2500;
    candidate.central_body_id="star";candidate.departure_body_id="home";candidate.arrival_body_id="target";
    candidate.ephemeris_metadata.snapshot_hash="same-snapshot";candidate.ephemeris_metadata.start_ut_s=0;
    candidate.ephemeris_metadata.end_ut_s=5000;candidate.ephemeris_metadata.step_s=1;
    const auto original=worker_candidate_id(grid,candidate);
    auto changed=grid;changed.departure_body_id="other";check(worker_candidate_id(changed,candidate)!=original,"role IDs disambiguate search IDs");
    changed=grid;changed.branch=TransferBranch::long_path;check(worker_candidate_id(changed,candidate)!=original,"Lambert branch disambiguates search IDs");
    changed=grid;changed.direction=AngularMomentumDirection::negative;check(worker_candidate_id(changed,candidate)!=original,"direction disambiguates search IDs");
    changed=grid;changed.launch_end_ut_s=200;check(worker_candidate_id(changed,candidate)!=original,"launch bound disambiguates search IDs");
    changed=grid;changed.launch_step_s=50;check(worker_candidate_id(changed,candidate)!=original,"launch step disambiguates search IDs");
    changed=grid;changed.flight_max_s=2700;check(worker_candidate_id(changed,candidate)!=original,"flight bound disambiguates search IDs");
    changed=grid;changed.flight_step_s=50;check(worker_candidate_id(changed,candidate)!=original,"flight step disambiguates search IDs");
    auto different_eph=candidate;different_eph.ephemeris_metadata.step_s=0.5;
    check(worker_candidate_id(grid,different_eph)!=original,"ephemeris settings disambiguate search IDs");
    check(worker_candidate_id(grid,candidate)==original,"candidate ID repeatable");
}
}
int main(){try{protocol_failures();success_and_cancel();refinement_status();no_screened_seed();review_failures();runtime_mode();candidate_identity();std::cout<<"PASS "<<checks<<" checks\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
