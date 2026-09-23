#include "ShootingWorkflow.hpp"
#include "RuntimeReader.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
using nlohmann::json;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void rejects(const std::function<void()>& action,const char* message){
 try{action();}catch(const std::exception&){return;}throw std::runtime_error(message);
}
}
int main(){try{
 const auto path=std::filesystem::temp_directory_path()/("ksp-shoot-ui-"+
  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
 const std::string bytes="{\"fixture\":true}";{std::ofstream file(path,std::ios::binary);file<<bytes;}
 const auto hash=ksp::sha256_hex(bytes);
 ksp::ShootingSource source{path.string(),hash,"runtime_observed_uncompared",
  "system_barycenter","principia_alicesun_frozen_at_capture",0.0};
 json state={{"position_m",{1,2,3}},{"velocity_mps",{4,5,6}}};
 json trial={{"route_seed",{{"snapshot_hash",hash},{"source_confidence","runtime_observed_uncompared"},
  {"launch_ut_s",0.0},{"mars_arrival_ut_s",1000.0},{"mars_departure_ut_s",5185000.0},
  {"venus_encounter_ut_s",5186000.0},{"home_return_ut_s",5187000.0},{"fixed_stay_s",5184000.0}}},
  {"launch_parking_state",state},{"impulses_mps",json::array({json::array({1,0,0}),
    json::array({2,0,0}),json::array({3,0,0}),json::array({4,0,0})})},
  {"checkpoint_targets_relative",json::array({state,state,state,state})}};
 const ksp::ShootingLimits limits{0.01,1000.0,6,80};
 const auto request=ksp::build_shooting_request(source,trial.dump(),limits,"shoot-ui-1");
 check(request["command"]=="shoot_route"&&request["protocol_version"]==1&&
  request["source"]["expected_snapshot_hash"]==hash&&request["trial"]==trial&&
  request["shooting"]["max_probe_evaluations"]==80,
  "request retains exact source trial and bounded shooting limits");
 auto stale=source;stale.hash=std::string(64,'0');
 rejects([&]{ksp::build_shooting_request(stale,trial.dump(),limits,"shoot-ui-2");},"stale source hash rejected");
 auto bad=trial;bad["route_seed"]["snapshot_hash"]="other";
 rejects([&]{ksp::build_shooting_request(source,bad.dump(),limits,"shoot-ui-2");},"seed hash rejected");
 bad=trial;bad["atmosphere_boundaries"]=json::array();
 rejects([&]{ksp::build_shooting_request(source,bad.dump(),limits,"shoot-ui-2");},"atmosphere override rejected");
 bad=trial;bad["route_seed"]["mars_departure_ut_s"]=5184001.0;
 rejects([&]{ksp::build_shooting_request(source,bad.dump(),limits,"shoot-ui-2");},"fixed Mars stay rejected");
 bad=trial;bad["impulses_mps"][0][0]=1001.0;
 rejects([&]{ksp::build_shooting_request(source,bad.dump(),limits,"shoot-ui-2");},"over-cap seed burn rejected");
 bad=trial;bad["checkpoint_targets_relative"].erase(3);
 rejects([&]{ksp::build_shooting_request(source,bad.dump(),limits,"shoot-ui-2");},"missing fourth target rejected");
 bad=trial;bad["source"]="forged";
 rejects([&]{ksp::build_shooting_request(source,bad.dump(),limits,"shoot-ui-2");},"hidden source rejected");
 bad=trial;bad["coarse_spacecraft"]={{"atmosphere_boundaries",json::array()}};
 rejects([&]{ksp::build_shooting_request(source,bad.dump(),limits,"shoot-ui-2");},
  "nested atmosphere override rejected before worker launch");
 rejects([&]{ksp::build_shooting_request(source,"{",limits,"shoot-ui-2");},"bad JSON rejected");
 rejects([&]{ksp::build_shooting_request(source,std::string(1024*1024+1,'x'),limits,"shoot-ui-2");},
  "oversized trial rejected");
 auto invalid=limits;invalid.max_probe_evaluations=10001;
 rejects([&]{ksp::build_shooting_request(source,trial.dump(),invalid,"shoot-ui-2");},"probe cap rejected");
 invalid=limits;invalid.finite_difference_impulse_mps=0;
 rejects([&]{ksp::build_shooting_request(source,trial.dump(),invalid,"shoot-ui-2");},"nonpositive limit rejected");
 {std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<bytes<<" ";}
 rejects([&]{ksp::build_shooting_request(source,trial.dump(),limits,"shoot-ui-2");},"changed source bytes rejected");
 json complete={{"type","complete"},{"status","checkpointed_accepted"},
  {"result_label","independent_nbody_fixed_impulse_checkpointed_only"},
  {"route_seed_evidence_revalidated",false},{"mars_stay_continuously_verified",false},
  {"coarse_diagnostics",{{"burns",json::array()}, {"checkpoints",json::array()},
   {"selected_venus",{{"ut_s",5186000.0},{"distance_m",1500000.0},{"clearance_m",400000.0}}},
   {"mars_radius",{{"observed_minimum_m",1200000.0},{"observed_maximum_m",1210000.0}}}}},
  {"strict",{{"fine",{{"checkpoints",json::array()},
    {"venus",{{"ut_s",5186000.0},{"distance_m",1500000.0},{"clearance_m",400000.0}}}}},
    {"disagreement",{{"maximum_checkpoint_position_m",1.0},{"maximum_checkpoint_velocity_mps",0.01}}}}}};
 const char* names[]={"launch","Mars capture","Mars pre-departure","home return capture"};
 for(int i=0;i<4;++i){complete["coarse_diagnostics"]["burns"].push_back({{"magnitude_mps",i+1.0}});
  complete["coarse_diagnostics"]["checkpoints"].push_back({{"name",names[i]},
   {"signed_position_residual_m",{1,0,0}},{"signed_velocity_residual_mps",{0,0,0}},
   {"signed_parking_radius_residual_m",0.0},{"signed_radial_velocity_mps",0.0},
   {"signed_tangential_speed_residual_mps",0.0}});
  complete["strict"]["fine"]["checkpoints"].push_back({{"name",names[i]},
   {"position_error_m",1.0},{"velocity_error_mps",0.01}});}
 const auto accepted=ksp::present_shooting_completion(complete);
 check(accepted.saveable&&accepted.accepted&&accepted.text.find("independent_nbody_fixed_impulse_checkpointed_only")!=std::string::npos&&
  accepted.text.find("Mars capture")!=std::string::npos&&accepted.text.find("Venus")!=std::string::npos,
  "accepted presentation has burns residuals strict metrics and partial label");
 auto diagnostic=complete;diagnostic["status"]="missing_venus";
 diagnostic["result_label"]="independent_nbody_coarse_trial_diagnostic_only";
 diagnostic["coarse_diagnostics"]["selected_venus"]=nullptr;diagnostic.erase("strict");
 const auto shown=ksp::present_shooting_completion(diagnostic);
 check(shown.saveable&&!shown.accepted&&shown.text.find("missing_venus")!=std::string::npos&&
  shown.text.find("non-success")!=std::string::npos,"diagnostic presentation has no strict acceptance");
 auto fabricated=diagnostic;fabricated["status"]="optimized";
 rejects([&]{ksp::present_shooting_completion(fabricated);},"unsupported diagnostic status rejected");
 auto cancelled=complete;cancelled["type"]="cancelled";
 rejects([&]{ksp::present_shooting_completion(cancelled);},"cancel cannot display completed report");
 std::filesystem::remove(path);
 std::cout<<"shooting workflow headless checks passed\n";
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
