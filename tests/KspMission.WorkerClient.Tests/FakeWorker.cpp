#include <nlohmann/json.hpp>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
using json=nlohmann::json;
json evaluation_pass(){
 json burns=json::array(),checkpoints=json::array();
 const double epochs[4]={0,1000,5185000,5187000};
 const char* names[4]={"launch","Mars capture","Mars pre-departure","home return capture"};
 for(int i=0;i<4;++i){burns.push_back({{"ut_s",epochs[i]},{"delta_v_mps",{1,0,0}},
    {"magnitude_mps",1.0}});
  checkpoints.push_back({{"name",names[i]},{"ut_s",epochs[i]},
    {"position_error_m",0.0},{"velocity_error_mps",0.0},{"parking_radius_error_m",0.0},
    {"radial_velocity_mps",0.0},{"tangential_speed_error_mps",0.0}});}
 return {{"burns",burns},{"checkpoints",checkpoints},{"accepted_steps",100},{"rejected_steps",0},
  {"venus",{{"body_id","venus"},{"ut_s",5186000.0},{"distance_m",1000000.0},
    {"clearance_m",900000.0},{"safety_margin_m",1000.0}}},
  {"mars_radius",{{"observed_minimum_m",200000.0},{"observed_minimum_ut_s",1000.0},
    {"observed_maximum_m",210000.0},{"observed_maximum_ut_s",5185000.0},
    {"model_interval_lower_m",190000.0},{"model_interval_upper_m",220000.0},
    {"endpoint_count",2},{"root_count",1}}}};
}
int main(int argc,char** argv){const std::string mode=argc>1?argv[1]:"valid";
 if(mode=="never_read"){std::this_thread::sleep_for(std::chrono::seconds(30));return 0;}
 std::string request;std::getline(std::cin,request);
 if(mode.rfind("shooting_",0)==0){
  json base={{"protocol_version",1},{"request_id","shooting-client-test"},
   {"snapshot_hash",std::string(64,'a')},{"source_confidence","runtime_observed_uncompared"}};
  auto started=base;started.update({{"type","started"},{"source_mode","runtime_snapshot"},
   {"frame_origin","system_barycenter"},{"frame_axes","principia_alicesun_frozen_at_capture"},
   {"frame_handedness","right"},{"state_epoch_ut_s",0.0},{"units","SI"},
   {"result_label","independent_nbody_coarse_trial_diagnostic_only"}});
  auto progress=base;progress.update({{"type","progress"},{"phase","coarse_probes"},
   {"completed_probes",1},{"total_probes",80},{"frame_origin","system_barycenter"},
   {"frame_axes","principia_alicesun_frozen_at_capture"},{"frame_handedness","right"},
   {"state_epoch_ut_s",0.0},{"units","SI"}});
  auto done=base;done.update({{"type","complete"},{"status","checkpointed_accepted"},
   {"result_label","independent_nbody_fixed_impulse_checkpointed_only"},
   {"completed_probes",1},{"total_probes",80},{"iterations",0},
   {"frame_origin","system_barycenter"},{"frame_axes","principia_alicesun_frozen_at_capture"},
   {"frame_handedness","right"},{"state_epoch_ut_s",0.0},{"units","SI"},
   {"role_body_ids",{{"central","sun"},{"home","home"},{"mars","mars"},{"venus","venus"}}},
   {"route_seed_evidence_revalidated",false},{"mars_stay_continuously_verified",false}});
  json state={{"position_m",{0,0,0}},{"velocity_mps",{0,0,0}}};
  done["final_trial"]={{"launch_parking_state",state},
   {"impulses_mps",json::array({json::array({1,0,0}),json::array({1,0,0}),json::array({1,0,0}),json::array({1,0,0})})},
   {"checkpoint_targets_relative",json::array({state,state,state,state})}};
  auto pass=evaluation_pass();pass["venus"].erase("safety_margin_m");
  json signed_checks=json::array();
  for(const auto& point:pass["checkpoints"])
   signed_checks.push_back({{"name",point["name"]},{"ut_s",point["ut_s"]},
    {"signed_position_residual_m",{0,0,0}},{"signed_velocity_residual_mps",{0,0,0}},
    {"signed_parking_radius_residual_m",0.0},{"signed_radial_velocity_mps",0.0},
    {"signed_tangential_speed_residual_mps",0.0}});
  done["coarse_diagnostics"]={{"burns",pass["burns"]},{"checkpoints",signed_checks},
   {"selected_venus",pass["venus"]},{"venus_event_count",1},
   {"minimum_observed_venus_boundary_margin_m",1000.0},
   {"mars_radius",pass["mars_radius"]},{"total_charged_delta_v_mps",4.0}};
  done["coarse_diagnostics"]["mars_radius"].erase("endpoint_count");
  done["coarse_diagnostics"]["mars_radius"].erase("root_count");
  done["strict"]={{"coarse",pass},{"fine",pass},{"total_charged_delta_v_mps",4.0},
   {"disagreement",{{"maximum_checkpoint_position_m",0.0},{"maximum_checkpoint_velocity_mps",0.0},
    {"venus_event_time_s",0.0},{"venus_radius_m",0.0},{"mars_minimum_radius_m",0.0},
    {"mars_maximum_radius_m",0.0},{"mars_minimum_time_s",0.0},{"mars_maximum_time_s",0.0}}}};
  if(mode=="shooting_wrong_impulse")done["strict"]["fine"]["burns"][0]["delta_v_mps"]={2,0,0};
  std::cout<<started.dump()<<std::endl<<progress.dump()<<std::endl<<done.dump()<<std::endl;return 0;
 }
 if(mode.rfind("evaluation_",0)==0){
  json base={{"protocol_version",1},{"request_id","evaluation-client-test"},
   {"snapshot_hash",std::string(64,'a')},{"source_confidence","runtime_observed_uncompared"}};
  if(mode=="evaluation_cancelled"){
   base["type"]="cancelled";base["status"]="before_numerical_work";
   std::cout<<base.dump()<<std::endl;return 0;}
  auto started=base;started["type"]="started";
  started.update(json{{"source_mode","runtime_snapshot"},{"frame_origin","system_barycenter"},
   {"frame_axes","principia_alicesun_frozen_at_capture"},{"frame_handedness","right"},
   {"state_epoch_ut_s",0.0},{"units","SI"},
   {"result_label","independent_nbody_fixed_impulse_checkpointed_only"}});
  auto progress=base;progress["type"]="progress";
  progress.update(json{{"phase","fixed_impulse_evaluation"},{"completed_phases",0},{"total_phases",1}});
  auto complete=base;complete["type"]="complete";
  complete.update(json{{"result_label","independent_nbody_fixed_impulse_checkpointed_only"},
   {"role_body_ids",{{"central","sun"},{"home","home"},{"mars","mars"},{"venus","venus"}}},
   {"route_seed_evidence_revalidated",false},{"mars_stay_continuously_verified",false},
   {"frame_origin","system_barycenter"},{"frame_axes","principia_alicesun_frozen_at_capture"},
   {"frame_handedness","right"},{"state_epoch_ut_s",0.0},{"units","SI"},
   {"force_model","newtonian_point_mass"},{"coarse",evaluation_pass()},
   {"strict",evaluation_pass()},{"total_charged_delta_v_mps",4.0},
   {"disagreement",{{"maximum_checkpoint_position_m",0.0},{"maximum_checkpoint_velocity_mps",0.0},
    {"venus_event_time_s",0.0},{"venus_radius_m",0.0},{"mars_minimum_radius_m",0.0},
    {"mars_maximum_radius_m",0.0},{"mars_minimum_time_s",0.0},{"mars_maximum_time_s",0.0}}}});
  if(mode=="evaluation_wrong_venus")complete["coarse"]["venus"]["body_id"]="other";
  std::cout<<started.dump()<<std::endl;
  if(mode=="evaluation_eof")return 0;
  std::cout<<progress.dump()<<std::endl;
  progress["completed_phases"]=1;
  std::cout<<progress.dump()<<std::endl<<complete.dump()<<std::endl;return 0;
 }
 auto started=json{{"protocol_version",1},{"request_id","client-test"},{"type","started"},
  {"snapshot_hash","synthetic-worker-v1"},{"source_confidence","synthetic_fixture"},{"total_cells",2}};
 auto progress=started;progress["type"]="progress";progress["sampled_cells"]=1;
 auto complete=started;complete["type"]="complete";complete["ranked_candidates"]=json::array();
 if(mode=="cancel_ack"){
  std::cout<<started.dump()<<std::endl;
  std::string control;std::getline(std::cin,control);
  auto parsed=json::parse(control,nullptr,false);
  if(!parsed.is_object()||parsed.value("protocol_version",0)!=1||parsed.value("command",std::string{})!="cancel"||
     parsed.value("request_id",std::string{})!="client-test")return 9;
  complete["type"]="cancelled";std::cout<<complete.dump()<<std::endl;return 0;}
 if(mode=="hang_cancel"){std::this_thread::sleep_for(std::chrono::seconds(30));return 0;}
 if(mode=="malformed"){std::cout<<"{bad\n";return 0;}
 if(mode=="oversized"){std::cout<<std::string(1024*1024+1,'x')<<'\n';return 0;}
 if(mode=="wrong_request")started["request_id"]="other";
 if(mode=="wrong_source")started["snapshot_hash"]="other";
 std::cout<<started.dump()<<std::endl;
 if(mode=="premature_eof")return 0;if(mode=="nonzero_exit")return 7;
 std::cout<<progress.dump()<<std::endl;
 if(mode=="progress_regress"){progress["sampled_cells"]=0;std::cout<<progress.dump()<<std::endl;}
 std::cout<<complete.dump()<<std::endl;}
