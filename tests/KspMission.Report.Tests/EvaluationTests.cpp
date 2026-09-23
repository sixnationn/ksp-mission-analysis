#include "EvaluationReport.hpp"
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
}
int main(){try{
 auto f=fixture();auto document=ksp::compose_fixed_evaluation_report(f.bytes,f.hash,f.request,f.events);
 const auto path=std::filesystem::temp_directory_path()/"ksp-fixed-evaluation-report-test.json";
 ksp::save_fixed_evaluation_report(document,path);check(ksp::load_fixed_evaluation_report(path)==document,"round trip");
 auto bad=document;bad["schema_version"]=2;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"version");
 bad=document;bad["report_kind"]="screened_study";rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"kind");
 bad=document;bad["feasible"]=true;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"top-level unsupported feasibility claim");
 bad=document;bad["events"][3]["principia_equivalent"]=true;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"terminal unsupported Principia claim");
 bad=document;bad["events"][3]["coarse"]["venus"]["installed_game_verified"]=true;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"nested unsupported verification claim");
 bad=document;bad["runtime_snapshot_json_bytes"]=bad["runtime_snapshot_json_bytes"].get<std::string>()+" ";rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"source bytes");
 bad=document;bad["snapshot_sha256"]=std::string(64,'a');rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"source hash");
 bad=document;bad["request"]["source"]["expected_frame_axes"]="other";rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"request frame");
 bad=document;bad["request"]["trial"]["route_seed"]["mars_body_id"]="home";rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"role");
 bad=document;bad["request"]["trial"]["route_seed"]["mars_departure_ut_s"]=5184001;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"stay");
 bad=document;bad["request"]["trial"]["impulses_mps"][0][0]=2;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"trial burn");
 bad=document;bad["events"][3]["role_body_ids"]["venus"]="mars";rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"terminal role");
 bad=document;bad["events"][3]["request_id"]="other";rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"terminal identity");
 bad=document;bad["events"].erase(1);rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"missing progress");
 bad=document;std::swap(bad["events"][1],bad["events"][2]);rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"reordered progress");
 bad=document;bad["events"][3]["coarse"]["venus"]["distance_m"]=2500000;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"Venus radius");
 bad=document;bad["events"][3]["coarse"]["venus"]["safety_margin_m"]=0;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"Venus safety");
 bad=document;bad["events"][3]["route_seed_evidence_revalidated"]=true;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"evidence flag");
 bad=document;bad["events"][3]["type"]="cancelled";rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"cancelled");
 bad=document;bad["events"].erase(3);rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"partial");
 bad=document;bad["events"][3]["coarse"]["checkpoints"][0]["position_error_m"]=11;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"request tolerance");
 bad=document;bad["events"][3]["disagreement"]["venus_event_time_s"]=11;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"disagreement budget");
 auto valid_radius=document;valid_radius["events"][3]["strict"]["venus"]["distance_m"]=1500100.0;
 valid_radius["events"][3]["strict"]["venus"]["clearance_m"]=400100.0;
 valid_radius["events"][3]["strict"]["venus"]["safety_margin_m"]=399100.0;
 valid_radius["events"][3]["disagreement"]["venus_radius_m"]=100.0;
 ksp::validate_fixed_evaluation_report(valid_radius);
 bad=valid_radius;bad["events"][3]["disagreement"]["venus_radius_m"]=101.0;
 rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"Venus disagreement tampering");
 bad=document;bad["events"][3]["disagreement"]["mars_minimum_time_s"]=1.0;
 rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"Mars extremum disagreement tampering");
 bad=valid_radius;bad["events"][3]["strict"]["venus"]["distance_m"]=1540000.0;
 bad["events"][3]["strict"]["venus"]["clearance_m"]=440000.0;
 bad["events"][3]["strict"]["venus"]["safety_margin_m"]=439000.0;
 bad["events"][3]["disagreement"]["venus_radius_m"]=40000.0;
 rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"Venus radius exceeds one tenth safety margin");
 bad=document;bad["events"][3]["coarse"]["mars_radius"]["model_interval_upper_m"]=1220000;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"Mars shell");
 bad=document;bad["events"][3]["strict"]["venus"]["clearance_m"]=401000;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"source atmosphere consistency");
 bad=document;bad["events"].push_back(f.events[3]);rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"postterminal event");
 bad=document;bad["request"]["source_confidence"]="synthetic_fixture";rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"fabricated confidence");
 bad=document;bad["request"]["trial"]["atmosphere_boundaries"]=json::array();rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"atmosphere override");
 bad=document;bad["request"]["validation_status"]="principia_matched";rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"unsupported request claim");
 bad=document;bad["events"][3]["strict"]["validated"]="installed_game";rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"unsupported nested result claim");
 bad=document;bad["request"]["trial"]["coarse_spacecraft"]["max_accepted_steps"]=1000001;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"unbounded spacecraft request");
 bad=document;bad["request"]["trial"]["strict_ephemeris"].erase("step_s");rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"incomplete ephemeris request");
 bad=document;bad["request"]["trial"]["strict_ephemeris"]=bad["request"]["trial"]["coarse_ephemeris"];rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"strict ephemeris must be tighter");
 bad=document;bad["request"]["trial"]["strict_spacecraft"]=bad["request"]["trial"]["coarse_spacecraft"];rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"strict spacecraft must be tighter");
 bad=document;bad["request"]["trial"]["coarse_ephemeris"]["max_velocity_fit_error_mps"]=0.01;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"ephemeris fit consumes position budget");
 bad=document;bad["events"][3]["coarse"]["checkpoints"][0]["parking_radius_error_m"]=10001;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"home launch parking residual");
 bad=document;bad["request"]["trial"]["home_parking_altitude_m"]=1000;rejects([&]{ksp::validate_fixed_evaluation_report(bad);},"home parking below source atmosphere");
 bad=document;bad["result_label"]="feasible";rejects([&]{ksp::save_fixed_evaluation_report(bad,path);},"replacement rejected");check(ksp::load_fixed_evaluation_report(path)==document,"prior valid intact");
 {std::ofstream out(path,std::ios::trunc);out<<"{";}rejects([&]{ksp::load_fixed_evaluation_report(path);},"truncated file");
 ksp::save_fixed_evaluation_report(document,path);check(ksp::load_fixed_evaluation_report(path)==document,"recovery");std::filesystem::remove(path);
 rejects([&]{ksp::compose_fixed_evaluation_report(std::string(16*1024*1024+1,'x'),f.hash,f.request,f.events);},"source cap");
 std::cout<<"evaluation report tests passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
