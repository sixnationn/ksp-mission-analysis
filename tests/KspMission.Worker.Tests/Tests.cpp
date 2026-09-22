#include "Worker.hpp"
#include "RuntimeReader.hpp"
#include "MissionSearch.hpp"
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
json mission_request(){
    return {{"protocol_version",1},{"command","start_mission"},{"request_id","mission-test"},
        {"source",{{"mode","synthetic_mission_fixture"},{"expected_snapshot_hash","synthetic-mission-worker-v1"},
            {"expected_frame_origin","barycenter"},{"expected_frame_axes","X,Y,Z"},{"expected_state_epoch_ut_s",0.0}}},
        {"ephemeris",{{"end_ut_s",310000000.0},{"step_s",10000.0},
            {"max_position_fit_error_m",1e6},{"max_velocity_fit_error_mps",1000.0}}},
        {"mission",{{"central_body_id","sun"},{"home_body_id","home"},{"mars_body_id","mars"},{"venus_body_id","venus"},
            {"reference_normal",{0,0,1}},{"launch_start_ut_s",0.0},{"launch_end_ut_s",0.0},{"launch_step_s",1.0},
            {"legs",json::array({{{"min_s",100000000.0},{"max_s",100000000.0},{"step_s",1.0},{"branch","short"},{"direction","positive"}},
                {{"min_s",100000000.0},{"max_s",100000000.0},{"step_s",1.0},{"branch","short"},{"direction","negative"}},
                {{"min_s",50000000.0},{"max_s",50000000.0},{"step_s",1.0},{"branch","short"},{"direction","positive"}}})},
            {"fixed_stay_s",5184000.0},{"time_tolerance_s",0.0},{"max_total_duration_s",310000000.0},
            {"home_parking_altitude_m",100000.0},{"mars_parking_altitude_m",200000.0},{"return_capture_altitude_m",100000.0},
            {"venus_safety_margin_m",1000.0},{"venus_maximum_periapsis_m",1e12},{"venus_speed_tolerance_mps",1000.0},
            {"max_lambert_position_residual_m",1000000.0},{"max_lambert_velocity_residual_mps",100.0},
            {"central_safety_margin_m",0.0},{"max_cells",100},{"max_routes",5}}}};
}
Snapshot mission_snapshot(){
    Snapshot s;s.analysis_ready=true;s.confidence="synthetic_fixture";s.snapshot_hash="synthetic-mission-worker-v1";
    s.state_epoch_ut_s=0;s.frame={"barycenter","X,Y,Z","right",true};
    const double radius=1e11,angle=std::acos(-1.0)/3,c=std::cos(angle),t=std::sin(angle),speed=1000;
    s.bodies={{"sun",1e17,1e5,State{{0,0,0},{0,0,0}},0},
        {"home",1e12,1e4,State{{radius,0,0},{0,speed,0}},0},
        {"mars",2e12,1e4,State{{radius*c,radius*t,0},{-speed*t,speed*c,0}},0},
        {"venus",1e13,1e4,State{{radius*c,-radius*t,0},{speed*t,speed*c,0}},0}};
    Vec3 centre{},drift{};double total_mu=0;
    for(const auto& body:s.bodies){total_mu+=body.mu_m3_s2;
        centre=centre+body.state->position_m*body.mu_m3_s2;
        drift=drift+body.state->velocity_mps*body.mu_m3_s2;}
    centre=centre*(1/total_mu);drift=drift*(1/total_mu);
    for(auto& body:s.bodies){body.state->position_m=body.state->position_m-centre;
        body.state->velocity_mps=body.state->velocity_mps-drift;}return s;
}
void mission_protocol(){
    auto q=mission_request();auto out=run(q.dump());
    if(!(has(out,"started")&&has(out,"progress")&&has(out,"complete")&&!has(out,"error")))
        throw std::runtime_error(std::string("mission command emits lifecycle: ")+json(out).dump());
    check(out.front()["request_id"]=="mission-test"&&out.front()["snapshot_hash"]=="synthetic-mission-worker-v1"&&
          out.front()["source_confidence"]=="synthetic_fixture","mission source identity");
    check(out.front()["fixture_description"].get<std::string>().find("test-only")!=std::string::npos,
          "synthetic mission explicitly test-only");
    check(out.front()["role_atmosphere_altitudes_m"]["venus"]==1000&&
          out.front()["role_atmosphere_altitudes_m"]["home"]==0,"source atmosphere disclosed");
    check(out.back().contains("ranked_routes")&&out.back()["ranked_routes"].is_array(),"mission final routes field");
    check(out.back().contains("ephemeris_metadata")&&
          out.back()["ephemeris_metadata"]["force_model"]=="newtonian_point_mass"&&
          out.back()["ephemeris_metadata"]["measured_max_position_error_m"].is_number(),
          "mission final records independent ephemeris fit evidence");
    check(!out.back()["ranked_routes"].empty(),"synthetic mission yields screened route");
    const auto& row=out.back()["ranked_routes"][0];
    check(row["result_label"]=="patched_conic_screened_route"&&row["rank"]==1&&
          row["home_mars"]["departure_ut_s"]==0&&row["mars_venus"]["departure_ut_s"]==105184000&&
          row["venus_home"]["departure_ut_s"]==205184000,"dated three-leg route schema");
    check(row["home_injection_mps"]>0&&row["mars_capture_mps"]>0&&row["mars_departure_mps"]>0&&
          row["home_return_capture_mps"]>0&&row["total_optimistic_delta_v_mps"]>0&&
          row["venus_periapsis_margin_m"]>=0,"four burns and flyby clearance");
    check(row.contains("departure_c3_m2_s2")&&row.contains("return_c3_m2_s2")&&
          std::abs(row["departure_c3_m2_s2"].get<double>()-
              std::pow(row["home_mars"]["departure_vinf_mps"].get<double>(),2))<1e-6&&
          std::abs(row["return_c3_m2_s2"].get<double>()-
              std::pow(row["venus_home"]["arrival_vinf_mps"].get<double>(),2))<1e-6,
          "departure and return C3 conventions");
    check(has(out,"route"),"provisional route update emitted");
    const auto s=mission_snapshot();Settings settings;settings.start_ut_s=0;settings.end_ut_s=310000000;
    settings.step_s=10000;settings.max_position_fit_error_m=1e6;settings.max_velocity_fit_error_mps=1000;settings.max_steps=100000;
    const auto e=integrate(s,settings);MissionSearchRequest direct;auto& r=direct.route;
    r.central_body_id="sun";r.home_body_id="home";r.mars_body_id="mars";r.venus_body_id="venus";r.reference_normal={0,0,1};
    r.max_lambert_position_residual_m=1e6;r.max_lambert_velocity_residual_mps=100;
    r.launch_start_ut_s=0;r.launch_end_ut_s=0;r.fixed_stay_s=5184000;r.time_tolerance_s=0;r.max_total_duration_s=310000000;
    r.home_parking_altitude_m=100000;r.mars_parking_altitude_m=200000;r.return_capture_altitude_m=100000;
    r.home_atmosphere_altitude_m=0;r.mars_atmosphere_altitude_m=0;r.venus_atmosphere_altitude_m=1000;
    r.venus_safety_margin_m=1000;r.venus_maximum_periapsis_m=1e12;r.venus_speed_tolerance_mps=1000;
    direct.launch_step_s=1;direct.legs={FlightGrid{100000000,100000000,1},
        FlightGrid{100000000,100000000,1,TransferBranch::short_path,AngularMomentumDirection::negative},
        FlightGrid{50000000,50000000,1}};
    direct.central_atmosphere_altitude_m=0;direct.central_safety_margin_m=0;direct.max_cells=100;direct.max_routes=5;
    const auto direct_result=search_mission(s,e,direct);
    check(direct_result.routes.size()==1&&row["route_id"]==direct_result.routes[0].route_id&&
          std::abs(row["total_optimistic_delta_v_mps"].get<double>()-direct_result.routes[0].total_optimistic_delta_v_mps)<1e-9,
          "worker matches direct mission search");
    std::cout<<"mission optimistic delta-v m/s="<<row["total_optimistic_delta_v_mps"]
             <<" flyby periapsis margin m="<<row["venus_periapsis_margin_m"]<<'\n';
    std::size_t last=0;for(const auto& e:out){check(e["protocol_version"]==1&&e["request_id"]=="mission-test"&&
        e["snapshot_hash"]=="synthetic-mission-worker-v1"&&e["source_confidence"]=="synthetic_fixture","mission event envelope");
        if(e.value("type",std::string{})=="progress"){auto sampled=e["sampled_cells"].get<std::size_t>();
            check(sampled>=last&&sampled<=e["total_cells"].get<std::size_t>(),"mission progress monotonic");last=sampled;}}
    const auto again=run(q.dump());check(out==again,"mission deterministic events");
    q["source"]["expected_snapshot_hash"]="wrong";check(run(q.dump()).back()["code"]=="source_mismatch","mission hash rejects");
    q=mission_request();q["mission"]["home_body_id"]="missing";check(run(q.dump()).back()["type"]=="error","mission role rejects");
    q=mission_request();q["mission"]["home_atmosphere_altitude_m"]=123;check(run(q.dump()).back()["type"]=="error","mission atmosphere override rejects");
    q=mission_request();q["mission"]["legs"][1]["step_s"]=0;check(run(q.dump()).back()["type"]=="error","mission invalid grid rejects");
    q=mission_request();q["mission"]["launch_end_ut_s"]=1.5;check(run(q.dump()).back()["type"]=="error","mission noninteger grid rejects");
    q=mission_request();q["mission"]["max_cells"]=1;check(run(q.dump()).back()["type"]=="error","mission work cap rejects");
    q=mission_request();q["mission"]["max_routes"]=129;
    check(run(q.dump()).back()["type"]=="error","mission event-size route cap rejects");
    q=mission_request();q["ephemeris"]["end_ut_s"]=100;check(run(q.dump()).back()["type"]=="error","mission coverage rejects");
    q=mission_request();q["mission"]["venus_safety_margin_m"]=5e7;
    out=run(q.dump());check(out.back()["type"]=="complete"&&out.back()["ranked_routes"].empty(),"impossible flyby yields no route");
    q=mission_request();q["mission"]["max_total_duration_s"]=5186999;
    out=run(q.dump());check(out.back()["type"]=="complete"&&out.back()["ranked_routes"].empty(),"duration restriction yields no route");
    q=mission_request();int starts=0;out=run(q.dump(),[&](){return starts++>0;});
    check(out.front()["type"]=="started"&&out.back()["type"]=="cancelled"&&out.back().contains("ranked_routes"),"mission cancellation terminal");
    q=mission_request();q["mission"]["launch_end_ut_s"]=10.0;q["mission"]["launch_step_s"]=10.0;
    const auto full=run(q.dump());check(full.back()["ranked_routes"].size()==2,"two dated synthetic routes");
    q["mission"]["max_routes"]=1;const auto capped=run(q.dump());
    check(capped.back()["ranked_routes"].size()==1&&
          capped.back()["ranked_routes"][0]["route_id"]==full.back()["ranked_routes"][0]["route_id"],
          "deterministic capped top route");
    bool accepted=false;std::vector<json> partial;
    process_start_line(q.dump(),[&](const json& message){partial.push_back(message);
        if(message.value("type",std::string{})=="progress"&&message.value("retained_routes",0)>0)accepted=true;},
        [&](){return accepted;});
    check(partial.back()["type"]=="cancelled"&&partial.back()["ranked_routes"].size()==1&&
          partial.back()["sampled_cells"]>=3,"mission cancellation keeps accepted partial route");
}
void mission_runtime_mode(){
    auto document=runtime_document();document["bodies"]=json::array();
    const auto s=mission_snapshot();
    Vec3 centre{},velocity{};double mass=0;
    for(const auto& body:s.bodies){mass+=body.mu_m3_s2;
        centre=centre+body.state->position_m*body.mu_m3_s2;
        velocity=velocity+body.state->velocity_mps*body.mu_m3_s2;}
    centre=centre*(1/mass);velocity=velocity*(1/mass);
    for(const auto& body:s.bodies){const auto& state=*body.state;
        const auto position=state.position_m-centre,speed=state.velocity_mps-velocity;
        document["bodies"].push_back({{"id",body.id},{"parent_id",body.id=="sun"?json(nullptr):json("sun")},
            {"mu_m3_s2",body.mu_m3_s2},{"radius_m",body.radius_m},
            {"atmosphere_boundary_m",body.id=="venus"?1000.0:0.0},{"state_epoch_ut_s",0.0},
            {"position_m",{position.x,position.y,position.z}},
            {"velocity_mps",{speed.x,speed.y,speed.z}}});}
    const auto path=std::filesystem::temp_directory_path()/("ksp-mission-worker-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
    auto write=[&](const std::string& bytes){std::ofstream file(path,std::ios::binary|std::ios::trunc);
        file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));};
    auto bytes=document.dump();write(bytes);const auto hash=sha256_hex(bytes);
    auto q=mission_request();q["source"]={{"mode","runtime_snapshot"},{"path",path.string()},
        {"expected_snapshot_hash",hash},{"expected_frame_origin","system_barycenter"},
        {"expected_frame_axes","principia_alicesun_frozen_at_capture"},{"expected_state_epoch_ut_s",0.0}};
    const auto out=run(q.dump());
    if(!(out.front()["type"]=="started"&&out.back()["type"]=="complete"))
        throw std::runtime_error(std::string("runtime mission completes: ")+json(out).dump());
    check(!out.back()["ranked_routes"].empty(),"runtime mission fixture yields screened route");
    check(out.front()["snapshot_hash"]==hash&&out.front()["source_confidence"]=="runtime_observed_uncompared"&&
          out.front()["source_mode"]=="runtime_snapshot","runtime mission exact source provenance");
    for(const auto& event:out)check(event["snapshot_hash"]==hash&&event["source_confidence"]=="runtime_observed_uncompared",
        "runtime mission event provenance");
    auto bad=q;bad["source"]["expected_snapshot_hash"]=std::string(64,'0');
    check(run(bad.dump()).back()["code"]=="source_mismatch","runtime mission wrong hash rejects");
    bad=q;bad["source"]["expected_frame_axes"]="wrong";
    check(!has(run(bad.dump()),"started"),"runtime mission wrong frame rejects before work");
    bad=q;bad["source"]["expected_state_epoch_ut_s"]=1;
    check(!has(run(bad.dump()),"started"),"runtime mission wrong epoch rejects before work");
    bad=q;bad["mission"]["home_parking_altitude_m"]=-1;
    check(!has(run(bad.dump()),"started"),"runtime mission invalid parking rejects before work");
    document["bodies"][3].erase("atmosphere_boundary_m");bytes=document.dump();write(bytes);
    bad=q;bad["source"]["expected_snapshot_hash"]=sha256_hex(bytes);
    check(!has(run(bad.dump()),"started"),"runtime mission missing atmosphere rejects");
    write(document.dump()+"x");bad=q;bad["source"]["expected_snapshot_hash"]=sha256_hex(document.dump()+"x");
    check(!has(run(bad.dump()),"started"),"runtime mission malformed exact bytes reject");
    std::filesystem::remove(path);
}
}
int main(){try{protocol_failures();success_and_cancel();refinement_status();no_screened_seed();review_failures();runtime_mode();candidate_identity();mission_protocol();mission_runtime_mode();std::cout<<"PASS "<<checks<<" checks\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
