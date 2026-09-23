#include "ReportReopen.hpp"
#include "EvaluationReport.hpp"
#include "StudyReport.hpp"
#include <chrono>
#include "RuntimeReader.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>

using nlohmann::json;
namespace {
void check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);}
void rejects(const std::function<void()>& action,const char* name){try{action();}catch(const ksp::EvaluationReportError&){return;}throw std::runtime_error(name);}
json body(const char* id,json parent,double x,double y,double atmosphere){return {{"id",id},{"parent_id",parent},{"mu_m3_s2",1e18},{"radius_m",1e6},{"atmosphere_boundary_m",atmosphere},{"state_epoch_ut_s",0.0},{"position_m",{x,y,0}},{"velocity_mps",{0,0,0}}};}
struct Fixture {std::string bytes,hash;json request;std::vector<json> events;};
Fixture fixture(){
 json source={{"schema_version",1},{"confidence","runtime_observed_uncompared"},
  {"capture",{{"exporter_id","KspMission.RuntimeExporter"},{"exporter_version","1"},{"game_version","1.12.5"},{"save_id","evaluation-test"},{"capture_ut_s",0.0},{"principia_loaded",true},{"state_source","principia_celestial_from_parent"},{"mods",json::array({{{"id","Principia"},{"version","test"}}})}}},
  {"frame",{{"origin","system_barycenter"},{"axes","principia_alicesun_frozen_at_capture"},{"handedness","right"},{"inertial",true},{"source_frame","Principia/AliceSun"},{"transform_method","parent_relative_sum_then_com_translation"},{"transform_version","1"}}},
  {"bodies",json::array({body("sun",nullptr,-3e9,-3e9,0),body("home","sun",1e11,0,1e5),body("mars","sun",2e11,0,1e5),body("venus","sun",0,3e11,1e5)})},
  {"calendar",{{"day_duration_s",86400},{"display_origin_ut_s",0},{"use_leap_years",false},{"month_lengths",{31,28,31,30,31,30,31,31,30,31,30,31}}}}};
 source["bodies"][0]["mu_m3_s2"]=1e20;
 auto bytes=source.dump(),hash=ksp::sha256_hex(bytes);
 json seed={{"central_body_id","sun"},{"home_body_id","home"},{"mars_body_id","mars"},{"venus_body_id","venus"},{"launch_ut_s",0.0},{"mars_arrival_ut_s",1000.0},{"mars_departure_ut_s",5185000.0},{"venus_encounter_ut_s",5186000.0},{"home_return_ut_s",5187000.0},{"fixed_stay_s",5184000.0},{"snapshot_hash",hash},{"source_confidence","runtime_observed_uncompared"}};
 json state={{"position_m",{0,0,0}},{"velocity_mps",{0,0,0}}};
 json trial={{"route_seed",seed},{"launch_parking_state",state},{"impulses_mps",json::array({json::array({1,0,0}),json::array({1,0,0}),json::array({1,0,0}),json::array({1,0,0})})},{"checkpoint_targets_relative",json::array({state,state,state,state})},
  {"home_parking_altitude_m",101000.0},{"mars_parking_altitude_m",205000.0},{"home_capture_altitude_m",101000.0},{"fixed_position_tolerance_m",10.0},{"fixed_velocity_tolerance_mps",10.0},{"parking_radius_tolerance_m",10000.0},{"parking_radial_velocity_tolerance_mps",10.0},{"parking_tangential_speed_tolerance_mps",10.0},{"venus_window_halfwidth_s",100.0},{"venus_max_encounter_radius_m",2000000.0},{"venus_safety_margin_m",1000.0},{"disagreement_position_m",10.0},{"disagreement_velocity_mps",10.0},{"disagreement_event_time_s",10.0},{"disagreement_mars_extremum_radius_m",10000.0},{"disagreement_mars_extremum_time_s",10.0}};
 json ephemeris={{"start_ut_s",0.0},{"end_ut_s",5187000.0},{"step_s",1000.0},{"max_position_fit_error_m",0.1},{"max_velocity_fit_error_mps",1e-8},{"max_steps",100000}};
 json spacecraft={{"start_ut_s",0.0},{"end_ut_s",5187000.0},{"abs_position_tolerance_m",1.0},{"abs_velocity_tolerance_mps",0.01},{"relative_tolerance",1e-9},{"min_step_s",0.01},{"max_step_s",100.0},{"max_accepted_steps",1000000}};
 trial["coarse_ephemeris"]=ephemeris;trial["strict_ephemeris"]=ephemeris;
 trial["strict_ephemeris"]["step_s"]=500.0;trial["strict_ephemeris"]["max_position_fit_error_m"]=0.05;trial["strict_ephemeris"]["max_velocity_fit_error_mps"]=5e-9;
 trial["coarse_spacecraft"]=spacecraft;trial["strict_spacecraft"]=spacecraft;
 trial["strict_spacecraft"]["abs_position_tolerance_m"]=0.5;trial["strict_spacecraft"]["abs_velocity_tolerance_mps"]=0.005;
 trial["strict_spacecraft"]["relative_tolerance"]=1e-10;trial["strict_spacecraft"]["max_step_s"]=50.0;
 json request={{"protocol_version",1},{"command","evaluate_route"},{"request_id","evaluation-report-test"},{"source",{{"mode","runtime_snapshot"},{"path","historical-only.json"},{"expected_snapshot_hash",hash},{"expected_frame_origin","system_barycenter"},{"expected_frame_axes","principia_alicesun_frozen_at_capture"},{"expected_state_epoch_ut_s",0.0}}},{"trial",trial}};
 auto base=[&](const char* type){return json{{"protocol_version",1},{"request_id","evaluation-report-test"},{"type",type},{"snapshot_hash",hash},{"source_confidence","runtime_observed_uncompared"}};};
 auto started=base("started");started.update({{"source_mode","runtime_snapshot"},{"frame_origin","system_barycenter"},{"frame_axes","principia_alicesun_frozen_at_capture"},{"frame_handedness","right"},{"state_epoch_ut_s",0.0},{"units","SI"},{"result_label","independent_nbody_fixed_impulse_checkpointed_only"}});
 auto p0=base("progress");p0.update({{"phase","fixed_impulse_evaluation"},{"completed_phases",0},{"total_phases",1}});auto p1=p0;p1["completed_phases"]=1;
 json burns=json::array(),checks=json::array();double times[4]={0,1000,5185000,5187000};const char* names[4]={"launch","Mars capture","Mars pre-departure","home return capture"};
 for(int i=0;i<4;++i){burns.push_back({{"ut_s",times[i]},{"delta_v_mps",{1,0,0}},{"magnitude_mps",1.0}});checks.push_back({{"name",names[i]},{"ut_s",times[i]},{"position_error_m",0.0},{"velocity_error_mps",0.0},{"parking_radius_error_m",0.0},{"radial_velocity_mps",0.0},{"tangential_speed_error_mps",0.0}});}
 json pass={{"burns",burns},{"checkpoints",checks},{"accepted_steps",100},{"rejected_steps",0},{"venus",{{"body_id","venus"},{"ut_s",5186000.0},{"distance_m",1500000.0},{"clearance_m",400000.0},{"safety_margin_m",399000.0}}},{"mars_radius",{{"observed_minimum_m",1200000.0},{"observed_minimum_ut_s",1000.0},{"observed_maximum_m",1210000.0},{"observed_maximum_ut_s",5185000.0},{"model_interval_lower_m",1195000.0},{"model_interval_upper_m",1215000.0},{"endpoint_count",2},{"root_count",1}}}};
 auto done=base("complete");done.update({{"result_label","independent_nbody_fixed_impulse_checkpointed_only"},{"role_body_ids",{{"central","sun"},{"home","home"},{"mars","mars"},{"venus","venus"}}},{"route_seed_evidence_revalidated",false},{"mars_stay_continuously_verified",false},{"frame_origin","system_barycenter"},{"frame_axes","principia_alicesun_frozen_at_capture"},{"frame_handedness","right"},{"state_epoch_ut_s",0.0},{"units","SI"},{"force_model","newtonian_point_mass"},{"coarse",pass},{"strict",pass},{"total_charged_delta_v_mps",4.0},{"disagreement",{{"maximum_checkpoint_position_m",0.0},{"maximum_checkpoint_velocity_mps",0.0},{"venus_event_time_s",0.0},{"venus_radius_m",0.0},{"mars_minimum_radius_m",0.0},{"mars_maximum_radius_m",0.0},{"mars_minimum_time_s",0.0},{"mars_maximum_time_s",0.0}}}});
 return {bytes,hash,request,{started,p0,p1,done}};
}
Fixture shooting_fixture(){
 auto f=fixture();f.request["command"]="shoot_route";
 f.request["shooting"]={{"finite_difference_impulse_mps",0.01},{"max_impulse_mps",1000.0},
  {"max_iterations",6},{"max_probe_evaluations",80}};
 auto& started=f.events[0];started["result_label"]="independent_nbody_coarse_trial_diagnostic_only";
 auto progress=f.events[1];progress.erase("completed_phases");progress.erase("total_phases");
 progress.update({{"phase","coarse_probes"},{"completed_probes",1},{"total_probes",80},
  {"frame_origin","system_barycenter"},{"frame_axes","principia_alicesun_frozen_at_capture"},
  {"frame_handedness","right"},{"state_epoch_ut_s",0.0},{"units","SI"}});
 auto done=f.events.back(),strict=done;
 strict["coarse"]["venus"].erase("safety_margin_m");
 strict["strict"]["venus"].erase("safety_margin_m");
 done.erase("coarse");done.erase("strict");done.erase("disagreement");done.erase("force_model");
 done.erase("total_charged_delta_v_mps");
 done.update({{"status","checkpointed_accepted"},{"completed_probes",1},{"total_probes",80},
  {"iterations",0},{"final_trial",{{"launch_parking_state",f.request["trial"]["launch_parking_state"]},
   {"impulses_mps",f.request["trial"]["impulses_mps"]},
   {"checkpoint_targets_relative",f.request["trial"]["checkpoint_targets_relative"]}}},
  {"coarse_diagnostics",{{"burns",strict["coarse"]["burns"]},
   {"checkpoints",json::array()},{"selected_venus",strict["coarse"]["venus"]},
   {"venus_event_count",1},{"minimum_observed_venus_boundary_margin_m",399000.0},
   {"mars_radius",strict["coarse"]["mars_radius"]},{"total_charged_delta_v_mps",4.0}}},
  {"strict",{{"coarse",strict["coarse"]},{"fine",strict["strict"]},
   {"total_charged_delta_v_mps",4.0},{"disagreement",strict["disagreement"]}}}});
 for(const auto& point:strict["coarse"]["checkpoints"])
  done["coarse_diagnostics"]["checkpoints"].push_back({{"name",point["name"]},{"ut_s",point["ut_s"]},
   {"signed_position_residual_m",{0,0,0}},{"signed_velocity_residual_mps",{0,0,0}},
   {"signed_parking_radius_residual_m",0.0},{"signed_radial_velocity_mps",0.0},
   {"signed_tangential_speed_residual_mps",0.0}});
 done["coarse_diagnostics"]["mars_radius"].erase("endpoint_count");
 done["coarse_diagnostics"]["mars_radius"].erase("root_count");
 done["coarse_diagnostics"]["selected_venus"].erase("safety_margin_m");
 f.events={started,progress,done};return f;
}
}
namespace {
json study_report_fixture(){
    const json leg1={{"departure_ut_s",0.0},{"arrival_ut_s",1000.0},{"departure_body_id","home"},
        {"arrival_body_id","mars"},{"branch","short"},{"direction","positive"},
        {"departure_vinf_mps",3.0}};
    const json leg2={{"departure_ut_s",5185000.0},{"arrival_ut_s",5186000.0},{"departure_body_id","mars"},
        {"arrival_body_id","venus"},{"branch","short"},{"direction","positive"}};
    const json leg3={{"departure_ut_s",5186000.0},{"arrival_ut_s",5187000.0},{"departure_body_id","venus"},
        {"arrival_body_id","home"},{"branch","short"},{"direction","positive"},
        {"arrival_vinf_mps",4.0}};
    const json route={{"route_id","route-search-fixture:0:1:2"},{"result_label","patched_conic_screened_route"},
        {"snapshot_hash","route-search-fixture"},{"source_confidence","synthetic_fixture"},
        {"legs",json::array({leg1,leg2,leg3})},{"fixed_stay_s",5184000.0},
        {"home_injection_mps",10.0},{"mars_capture_mps",20.0},{"mars_departure_mps",30.0},
        {"home_return_capture_mps",40.0},{"total_optimistic_delta_v_mps",100.0},
        {"departure_c3_m2_s2",9.0},{"return_c3_m2_s2",16.0},
        {"flyby_periapsis_margin_m",1000.0}};
    return {{"schema_version",1},
        {"source",{{"snapshot_hash","route-search-fixture"},{"confidence","synthetic_fixture"},
            {"frame_origin","barycenter"},{"frame_axes","X,Y,Z"},{"frame_handedness","right"},
            {"frame_inertial",true},{"state_epoch_ut_s",0.0}}},
        {"ephemeris",{{"force_model","newtonian_point_mass"},{"result_label","independent_newtonian_nbody"},
            {"integrator","fixture"},{"integrator_version","1"},{"start_ut_s",0.0},{"end_ut_s",6000000.0},
            {"step_s",6000000.0},{"max_position_fit_error_m",1.0},{"max_velocity_fit_error_mps",0.01},
            {"measured_max_position_error_m",0.0},{"measured_max_velocity_error_mps",0.0}}},
        {"mission",{{"central_body_id","sun"},{"home_body_id","home"},{"mars_body_id","mars"},{"venus_body_id","venus"},
            {"launch_start_ut_s",0.0},{"launch_end_ut_s",0.0},{"launch_step_s",1.0},
            {"flight_grids",json::array({{{"min_s",1000.0},{"max_s",1000.0},{"step_s",1.0}},
                {{"min_s",1000.0},{"max_s",1000.0},{"step_s",1.0}},
                {{"min_s",1000.0},{"max_s",1000.0},{"step_s",1.0}}})},
            {"fixed_stay_s",5184000.0},{"stay_type","parking_orbit"},{"return_condition","parking_capture"},
            {"max_cells",100},{"max_routes",5}}},
        {"calendar",{{"display_only",true},{"use_leap_years",false},{"day_duration_s",86400.0},
            {"display_origin_ut_s",0.0},{"month_lengths",{31,28,31,30,31,30,31,31,30,31,30,31}}}},
        {"result",{{"status","complete"},{"validation_status","patched_conic_screen_only"},
            {"sampled_cells",3},{"total_upper_bound_cells",3},{"ranked_routes",json::array({route})}}}};
}
}

namespace {
template<class F> void reopen_rejects(F&& action,const char* name){
 try{action();}catch(const ksp::ReportReopenError&){return;}throw std::runtime_error(name);
}
struct TempReports {
 std::filesystem::path dir;
 TempReports(){
  const auto root=std::filesystem::temp_directory_path();
  for(int i=0;i<32;++i){
   dir=root/("ksp-reopen-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(i));
   if(std::filesystem::create_directory(dir))return;
  }
  throw std::runtime_error("unique test directory unavailable");
 }
 ~TempReports(){std::error_code ignored;std::filesystem::remove_all(dir,ignored);}
 std::filesystem::path write(const char* name,const json& value){
  const auto path=dir/name;std::ofstream file(path,std::ios::binary);file<<value.dump();
  if(!file)throw std::runtime_error("fixture write failed");return path;
 }
};
}
int main(){try{
 TempReports temp;auto f=fixture();
 ksp::CurrentReportSource current{f.hash,"runtime_observed_uncompared","system_barycenter",
  "principia_alicesun_frozen_at_capture","right",0.0};
 auto fixed=ksp::compose_fixed_evaluation_report(f.bytes,f.hash,f.request,f.events);
 auto fixed_path=temp.write("fixed.json",fixed);
 auto opened=ksp::reopen_saved_report(fixed_path,current);
 check(opened.kind==ksp::SavedReportKind::fixed_impulse_evaluation&&
  opened.summary.find("checkpointed")!=std::string::npos,"fixed report dispatch");
 auto shot=shooting_fixture();
 auto shooting=ksp::compose_shooting_report(shot.bytes,shot.hash,shot.request,shot.events);
 auto shoot_path=temp.write("shooting.json",shooting);
 opened=ksp::reopen_saved_report(shoot_path,current);
 check(opened.kind==ksp::SavedReportKind::bounded_shooting&&
  opened.summary.find("checkpointed")!=std::string::npos,"shooting accepted dispatch");
 shot.events.back()["status"]="strict_rejected";
 shot.events.back()["result_label"]="independent_nbody_coarse_trial_diagnostic_only";
 shot.events.back().erase("strict");
 shooting=ksp::compose_shooting_report(shot.bytes,shot.hash,shot.request,shot.events);
 shoot_path=temp.write("diagnostic.json",shooting);
 opened=ksp::reopen_saved_report(shoot_path,current);
 check(opened.summary.find("diagnostic")!=std::string::npos&&
  opened.summary.find("no checkpointed acceptance")!=std::string::npos,"diagnostic stays non-success");
 auto study=study_report_fixture();
 const auto raw=json::parse(f.bytes);
 auto& source=study["source"];
 source["snapshot_hash"]=f.hash;source["confidence"]="runtime_observed_uncompared";
 source["frame_origin"]="system_barycenter";source["frame_axes"]="principia_alicesun_frozen_at_capture";
 source["exporter_id"]="KspMission.RuntimeExporter";source["exporter_version"]="1";
 source["game_version"]="1.12.5";source["save_id"]="evaluation-test";
 source["capture_ut_s"]=0.0;source["principia_loaded"]=true;
 source["state_source"]="principia_celestial_from_parent";source["source_frame"]="Principia/AliceSun";
 source["transform_method"]="parent_relative_sum_then_com_translation";source["transform_version"]="1";
 source["mods"]=raw["capture"]["mods"];
 study["result"]["ranked_routes"][0]["snapshot_hash"]=f.hash;
 study["result"]["ranked_routes"][0]["source_confidence"]="runtime_observed_uncompared";
 study["result"]["ranked_routes"][0]["route_id"]=f.hash+":0:1:2";
 auto study_path=temp.write("study.json",study);
 opened=ksp::reopen_saved_report(study_path,current);
 check(opened.kind==ksp::SavedReportKind::screened_study&&
  opened.summary.find("patched-conic")!=std::string::npos,"study dispatch");
 auto other=current;other.snapshot_hash=std::string(64,'0');
 check(f.hash!=other.snapshot_hash,"different hash fixture");
 reopen_rejects([&]{ksp::reopen_saved_report(fixed_path,other);},"different hash");
 other=current;other.confidence="synthetic_fixture";
 reopen_rejects([&]{ksp::reopen_saved_report(fixed_path,other);},"different confidence");
 other=current;other.frame_axes="different";
 reopen_rejects([&]{ksp::reopen_saved_report(fixed_path,other);},"different frame");
 other=current;other.state_epoch_ut_s=1;
 reopen_rejects([&]{ksp::reopen_saved_report(fixed_path,other);},"different epoch");
 for(const auto& path:{study_path,shoot_path}){
  other=current;other.snapshot_hash=std::string(64,'0');
  reopen_rejects([&]{ksp::reopen_saved_report(path,other);},"other kind different hash");
  other=current;other.frame_origin="different";
  reopen_rejects([&]{ksp::reopen_saved_report(path,other);},"other kind different frame");
 }
 other=current;other.confidence.clear();
 reopen_rejects([&]{ksp::reopen_saved_report(study_path,other);},"no imported runtime source");
 auto bad=fixed;bad["report_kind"]="unknown";
 reopen_rejects([&]{ksp::reopen_saved_report(temp.write("unknown.json",bad),current);},"unknown kind");
 bad=fixed;bad["schema_version"]=2;
 reopen_rejects([&]{ksp::reopen_saved_report(temp.write("version.json",bad),current);},"unsupported schema");
 bad=fixed;bad["runtime_snapshot_json_bytes"]=f.bytes+" ";
 reopen_rejects([&]{ksp::reopen_saved_report(temp.write("tampered.json",bad),current);},"tampered bytes");
 bad=shooting;bad["events"].back()["verification_status"]="verified";
 reopen_rejects([&]{ksp::reopen_saved_report(temp.write("claim.json",bad),current);},"false claim");
 bad=shooting;bad["events"].back()["status"]="checkpointed_accepted";
 reopen_rejects([&]{ksp::reopen_saved_report(temp.write("status.json",bad),current);},"altered diagnostic status");
 bad=shooting;bad["events"].back()["result_label"]="optimizer_verified";
 reopen_rejects([&]{ksp::reopen_saved_report(temp.write("label.json",bad),current);},"altered shooting label");
 std::ofstream(temp.dir/"truncated.json")<<"{\"schema_version\":";
 reopen_rejects([&]{ksp::reopen_saved_report(temp.dir/"truncated.json",current);},"truncated JSON");
 std::ofstream(temp.dir/"oversize.json",std::ios::binary).seekp(33*1024*1024)<<'x';
 reopen_rejects([&]{ksp::reopen_saved_report(temp.dir/"oversize.json",current);},"oversized file");
 {
  std::ofstream padded(temp.dir/"study-overlimit.json",std::ios::binary);
  padded<<study.dump();padded.seekp(17*1024*1024);padded<<' ';
 }
 reopen_rejects([&]{ksp::reopen_saved_report(temp.dir/"study-overlimit.json",current);},"study 16 MiB cap");
 std::cout<<"three report kinds and source binding passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
