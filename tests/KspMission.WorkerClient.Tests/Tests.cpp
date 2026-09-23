#include "WorkerClient.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
using namespace ksp;
using json=nlohmann::json;
namespace {
int checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class F> void rejects(F action,const char* fragment){try{action();}catch(const WorkerClientError& e){
 if(std::string(e.what()).find(fragment)==std::string::npos)
  throw std::runtime_error(std::string("wrong validation diagnostic: ")+e.what());
 ++checks;return;}throw std::runtime_error(fragment);}
std::string request(){return json{{"protocol_version",1},{"command","start"},{"request_id","client-test"},
 {"source",{{"mode","synthetic_fixture"},{"expected_snapshot_hash","synthetic-worker-v1"}}},
 {"grid",{{"central_body_id","star"},{"departure_body_id","home"},{"arrival_body_id","target"},
 {"launch_start_ut_s",0},{"launch_end_ut_s",0},{"launch_step_s",100},{"flight_min_s",2500},{"flight_max_s",2500},{"flight_step_s",100},
 {"reference_normal",{0,0,1}},{"branch","short"},{"direction","positive"},{"max_position_residual_m",0.1},
 {"max_velocity_residual_mps",1e-4},{"max_candidates",8},{"stable_seed",42},{"central_atmosphere_altitude_m",0.0},{"central_safety_margin_m",0.0}}},
 {"refine",{{"enabled",false}}}}.dump();}
json event(std::string type){json e={{"protocol_version",1},{"request_id","client-test"},{"type",type},
 {"snapshot_hash","synthetic-worker-v1"},{"source_confidence","synthetic_fixture"}};
 if(type=="started")e["total_cells"]=2;if(type=="progress"){e["sampled_cells"]=1;e["total_cells"]=2;}
 if(type=="complete")e["ranked_candidates"]=json::array();return e;}
void validator_cases(){WorkerEventValidator v("client-test","synthetic-worker-v1","synthetic_fixture",8);
 rejects([&]{v.accept_line("{");},"JSON");rejects([&]{v.accept_line(std::string(1024*1024+1,'x'));},"line limit");
 auto bad=event("started");bad["protocol_version"]=2;rejects([&]{v.accept_line(bad.dump());},"protocol_version");
 bad=event("started");bad["request_id"]="other";rejects([&]{v.accept_line(bad.dump());},"request_id");
 bad=event("started");bad["snapshot_hash"]="other";rejects([&]{v.accept_line(bad.dump());},"snapshot_hash");
 bad=event("started");bad["type"]="fabricated";rejects([&]{v.accept_line(bad.dump());},"event type");
 v.accept_line(event("started").dump());v.accept_line(event("progress").dump());
 bad=event("progress");bad["sampled_cells"]=0;rejects([&]{v.accept_line(bad.dump());},"progress regression");
 bad=event("complete");bad["ranked_candidates"]=json::array({{{"rank",2},{"status","screened_seed"},
  {"snapshot_hash","synthetic-worker-v1"},{"source_confidence","synthetic_fixture"},{"screening_score_mps",1}}});
 rejects([&]{v.accept_line(bad.dump());},"ranked order");
 WorkerEventValidator clean("client-test","synthetic-worker-v1","synthetic_fixture",8);
 clean.accept_line(event("started").dump());clean.accept_line(event("complete").dump());check(clean.terminal(),"terminal accepted");}
json screened_route(){
 json r={{"route_id","synthetic-worker-v1:route"},{"result_label","patched_conic_screened_route"},
  {"snapshot_hash","synthetic-worker-v1"},{"source_confidence","synthetic_fixture"},
  {"home_mars",{{"central_body_id","sun"},{"departure_body_id","home"},{"arrival_body_id","mars"},
   {"departure_ut_s",0.0},{"arrival_ut_s",1000.0},{"departure_vinf_mps",3.0}}},
  {"mars_venus",{{"central_body_id","sun"},{"departure_body_id","mars"},{"arrival_body_id","venus"},
   {"departure_ut_s",5185000.0},{"arrival_ut_s",5186000.0}}},
  {"venus_home",{{"central_body_id","sun"},{"departure_body_id","venus"},{"arrival_body_id","home"},
   {"departure_ut_s",5186000.0},{"arrival_ut_s",5187000.0},{"arrival_vinf_mps",4.0}}},
  {"fixed_stay_s",5184000.0},{"home_injection_mps",10.0},{"mars_capture_mps",20.0},
  {"mars_departure_mps",30.0},{"home_return_capture_mps",40.0},
  {"total_optimistic_delta_v_mps",100.0},{"venus_minimum_periapsis_m",2000.0},
  {"departure_c3_m2_s2",9.0},{"return_c3_m2_s2",16.0},
  {"venus_clearance_radius_m",1000.0},{"venus_periapsis_margin_m",1000.0}};
 return r;
}
void mission_validator_cases(){
 WorkerEventValidator v("client-test","synthetic-worker-v1","synthetic_fixture",5,WorkerResultKind::screened_route);
 auto started=event("started");started["total_cells"]=3;v.accept_line(started.dump());
 auto route=screened_route();route["type"]="route";route["protocol_version"]=1;route["request_id"]="client-test";
 v.accept_line(route.dump());++checks;
 auto bad=route;bad["total_optimistic_delta_v_mps"]=101;
 rejects([&]{v.accept_line(bad.dump());},"burn total");
 bad=route;bad["return_c3_m2_s2"]=17.0;
 rejects([&]{v.accept_line(bad.dump());},"C3");
 bad=route;bad["source_confidence"]="runtime_observed_uncompared";
 rejects([&]{v.accept_line(bad.dump());},"source confidence");
 auto complete=event("complete");auto row=screened_route();row["rank"]=1;
 complete["ranked_routes"]=json::array({row});v.accept_line(complete.dump());check(v.terminal(),"mission terminal accepted");
 WorkerEventValidator duplicate("client-test","synthetic-worker-v1","synthetic_fixture",5,WorkerResultKind::screened_route);
 duplicate.accept_line(started.dump());auto repeated=complete;
 repeated["ranked_routes"].push_back(row);repeated["ranked_routes"][1]["rank"]=2;
 rejects([&]{duplicate.accept_line(repeated.dump());},"duplicate route_id");
 WorkerEventValidator wrong_mode("client-test","synthetic-worker-v1","synthetic_fixture",5);
 wrong_mode.accept_line(started.dump());rejects([&]{wrong_mode.accept_line(route.dump());},"event type");
}
json evaluation_event(const std::string& type){
 json e={{"protocol_version",1},{"request_id","evaluation-client-test"},{"type",type},
  {"snapshot_hash",std::string(64,'a')},{"source_confidence","runtime_observed_uncompared"}};
 if(type=="started"){
  e.update(json{{"source_mode","runtime_snapshot"},{"frame_origin","system_barycenter"},
   {"frame_axes","principia_alicesun_frozen_at_capture"},{"frame_handedness","right"},
   {"state_epoch_ut_s",0.0},{"units","SI"},
   {"result_label","independent_nbody_fixed_impulse_checkpointed_only"}});
 }
 if(type=="progress")e.update(json{{"phase","fixed_impulse_evaluation"},
   {"completed_phases",0},{"total_phases",1}});
 if(type=="complete"){
  json burns=json::array(),checkpoints=json::array();
  const double epochs[4]={0,1000,5185000,5187000};
  const char* names[4]={"launch","Mars capture","Mars pre-departure","home return capture"};
  for(int i=0;i<4;++i){burns.push_back({{"ut_s",epochs[i]},{"delta_v_mps",{1,0,0}},
      {"magnitude_mps",1.0}});
   checkpoints.push_back({{"name",names[i]},{"ut_s",epochs[i]},
      {"position_error_m",0.0},{"velocity_error_mps",0.0},
      {"parking_radius_error_m",0.0},{"radial_velocity_mps",0.0},
      {"tangential_speed_error_mps",0.0}});}
  json pass={{"burns",burns},{"checkpoints",checkpoints},{"accepted_steps",100},
   {"rejected_steps",0},{"venus",{{"body_id","venus"},{"ut_s",5186000.0},
      {"distance_m",1000000.0},{"clearance_m",900000.0},{"safety_margin_m",1000.0}}},
   {"mars_radius",{{"observed_minimum_m",200000.0},{"observed_minimum_ut_s",1000.0},
      {"observed_maximum_m",210000.0},{"observed_maximum_ut_s",5185000.0},
      {"model_interval_lower_m",190000.0},{"model_interval_upper_m",220000.0},
      {"endpoint_count",2},{"root_count",1}}}};
  e.update(json{{"result_label","independent_nbody_fixed_impulse_checkpointed_only"},
   {"role_body_ids",{{"central","sun"},{"home","home"},{"mars","mars"},{"venus","venus"}}},
   {"route_seed_evidence_revalidated",false},{"mars_stay_continuously_verified",false},
   {"frame_origin","system_barycenter"},{"frame_axes","principia_alicesun_frozen_at_capture"},
   {"frame_handedness","right"},{"state_epoch_ut_s",0.0},{"units","SI"},
   {"force_model","newtonian_point_mass"},{"coarse",pass},{"strict",pass},
   {"total_charged_delta_v_mps",4.0},
   {"disagreement",{{"maximum_checkpoint_position_m",0.0},
      {"maximum_checkpoint_velocity_mps",0.0},{"venus_event_time_s",0.0},
      {"venus_radius_m",0.0},{"mars_minimum_radius_m",0.0},
      {"mars_maximum_radius_m",0.0},{"mars_minimum_time_s",0.0},
      {"mars_maximum_time_s",0.0}}}});
 }
 return e;
}
void evaluation_validator_cases(){
 auto create=[](){return WorkerEventValidator("evaluation-client-test",std::string(64,'a'),
   "runtime_observed_uncompared",4,WorkerResultKind::fixed_impulse_evaluation);};
 auto v=create();v.accept_line(evaluation_event("started").dump());
 v.accept_line(evaluation_event("progress").dump());
 auto finished_phase=evaluation_event("progress");finished_phase["completed_phases"]=1;
 v.accept_line(finished_phase.dump());
 auto done=evaluation_event("complete");v.accept_line(done.dump());check(v.terminal(),"evaluation terminal accepted");
 rejects([&]{v.accept_line(done.dump());},"after terminal");
 rejects([&]{auto x=create();x.accept_line(evaluation_event("started").dump());
   x.accept_line(done.dump());},"progress");
 auto bad=evaluation_event("started");bad["units"]="km";
 rejects([&]{auto x=create();x.accept_line(bad.dump());},"units");
 bad=evaluation_event("started");bad["snapshot_hash"]="other";
 rejects([&]{auto x=create();x.accept_line(bad.dump());},"snapshot_hash");
 bad=evaluation_event("complete");bad["role_body_ids"].erase("mars");
 rejects([&]{auto x=create();x.accept_line(evaluation_event("started").dump());x.accept_line(bad.dump());},"role");
 bad=evaluation_event("complete");bad["coarse"]["burns"][1]["ut_s"]=999;
 rejects([&]{auto x=create();x.accept_line(evaluation_event("started").dump());x.accept_line(bad.dump());},"burn");
 bad=evaluation_event("complete");bad["coarse"]["venus"]["body_id"]="other";
 rejects([&]{auto x=create();x.accept_line(evaluation_event("started").dump());x.accept_line(bad.dump());},"Venus");
 bad=evaluation_event("complete");bad["route_seed_evidence_revalidated"]=true;
 rejects([&]{auto x=create();x.accept_line(evaluation_event("started").dump());x.accept_line(bad.dump());},"evidence");
 auto reject_complete=[&](json altered,const char* fragment){rejects([&]{auto x=create();
   x.accept_line(evaluation_event("started").dump());x.accept_line(altered.dump());},fragment);};
 bad=evaluation_event("started");bad["frame_axes"]="wrong";
 rejects([&]{auto x=create();x.accept_line(bad.dump());},"frame");
 bad=evaluation_event("complete");bad["state_epoch_ut_s"]=1;reject_complete(bad,"epoch");
 bad=evaluation_event("complete");bad["force_model"]="other";reject_complete(bad,"force model");
 bad=evaluation_event("complete");bad["result_label"]="independent_nbody_route_evaluated";
 reject_complete(bad,"label");
 bad=evaluation_event("complete");bad["units"]="km";reject_complete(bad,"units");
 bad=evaluation_event("complete");bad["mars_stay_continuously_verified"]=true;
 reject_complete(bad,"evidence");
 bad=evaluation_event("complete");bad["coarse"]["burns"].erase(3);reject_complete(bad,"four burns");
 bad=evaluation_event("complete");bad["coarse"]["burns"][0]["magnitude_mps"]=2;
 reject_complete(bad,"burn magnitude");
 bad=evaluation_event("complete");bad["total_charged_delta_v_mps"]=5;
 reject_complete(bad,"burn total");
 bad=evaluation_event("complete");bad["coarse"]["checkpoints"][1]["name"]="home return capture";
 reject_complete(bad,"checkpoint");
 bad=evaluation_event("complete");bad["strict"]["burns"][2]["ut_s"]=5185001;
 reject_complete(bad,"checkpoint");
 bad=evaluation_event("complete");bad["strict"]["burns"][2]["delta_v_mps"]={2,0,0};
 bad["strict"]["burns"][2]["magnitude_mps"]=2;
 reject_complete(bad,"same impulse");
 bad=evaluation_event("complete");bad["coarse"]["venus"]["safety_margin_m"]=0;
 reject_complete(bad,"Venus");
 bad=evaluation_event("complete");bad["coarse"]["checkpoints"][0]["position_error_m"]=-1;
 reject_complete(bad,"negative");
 bad=evaluation_event("complete");bad["strict"]["mars_radius"]["observed_maximum_m"]=100000;
 reject_complete(bad,"Mars radius");
 bad=evaluation_event("complete");bad["disagreement"]["venus_radius_m"]=-1;
 reject_complete(bad,"negative");
 bad=evaluation_event("complete");bad["coarse"]["mars_radius"]["observed_minimum_m"]=nullptr;
 reject_complete(bad,"observed_minimum_m");
 bad=evaluation_event("complete");bad["disagreement"]["venus_radius_m"]=nullptr;
 reject_complete(bad,"venus_radius_m");
 bad=evaluation_event("complete");bad["ranked_routes"]=json::array();
 reject_complete(bad,"result kind");
 bad=evaluation_event("complete");bad.erase("coarse");reject_complete(bad,"coarse");
 bad=evaluation_event("complete");bad["coarse"].erase("burns");
 reject_complete(bad,"burns");
 bad=evaluation_event("started");bad["source_confidence"]="synthetic_fixture";
 rejects([&]{auto x=create();x.accept_line(bad.dump());},"source confidence");
 bad=evaluation_event("progress");bad["completed_phases"]=1;
 auto progress_validator=create();progress_validator.accept_line(evaluation_event("started").dump());
 progress_validator.accept_line(evaluation_event("progress").dump());
 progress_validator.accept_line(bad.dump());bad["completed_phases"]=0;
 rejects([&]{progress_validator.accept_line(bad.dump());},"progress regression");
 bad=evaluation_event("progress");bad["sampled_cells"]=1;
 rejects([&]{auto x=create();x.accept_line(evaluation_event("started").dump());x.accept_line(bad.dump());},"progress");
 bad=evaluation_event("candidate");
 rejects([&]{auto x=create();x.accept_line(evaluation_event("started").dump());x.accept_line(bad.dump());},"result kind");
 bad=evaluation_event("route");
 rejects([&]{auto x=create();x.accept_line(evaluation_event("started").dump());x.accept_line(bad.dump());},"result kind");
 bad=evaluation_event("refinement");
 rejects([&]{auto x=create();x.accept_line(evaluation_event("started").dump());x.accept_line(bad.dump());},"result kind");
 auto cancelled=create();auto cancel_event=evaluation_event("cancelled");
 cancel_event["status"]="before_numerical_work";cancelled.accept_line(cancel_event.dump());
 check(cancelled.terminal(),"source-bound pre-start evaluation cancellation accepted");
 bad=evaluation_event("cancelled");
 rejects([&]{auto x=create();x.accept_line(bad.dump());},"status");
 bad["status"]="after_numerical_work";
 rejects([&]{auto x=create();x.accept_line(bad.dump());},"status");
 auto source_error=create();json early={{"protocol_version",1},{"request_id","evaluation-client-test"},
   {"type","error"},{"code","source_read_failed"},{"detail","missing"}};
 source_error.accept_line(early.dump());check(source_error.terminal(),"pre-source error accepted");
}
json shooting_event(const std::string& type){
 auto e=evaluation_event(type);e["request_id"]="shooting-client-test";
 if(type=="started"){
  e["result_label"]="independent_nbody_coarse_trial_diagnostic_only";
  e["non_interruptible_stages"]={"planetary_ephemeris_integration","single_spacecraft_probe","strict_coarse_and_fine_repropagation"};
 }
 if(type=="progress"){
  e.erase("completed_phases");e.erase("total_phases");
  e.update(json{{"phase","coarse_probes"},{"completed_probes",1},{"total_probes",80},
   {"frame_origin","system_barycenter"},{"frame_axes","principia_alicesun_frozen_at_capture"},
   {"frame_handedness","right"},{"state_epoch_ut_s",0.0},{"units","SI"}});
 }
 if(type=="complete"){
  auto fixed=e;fixed["coarse"]["venus"].erase("safety_margin_m");
  fixed["strict"]["venus"].erase("safety_margin_m");
  const auto burns=fixed["coarse"]["burns"];
  e.erase("coarse");e.erase("strict");e.erase("disagreement");e.erase("force_model");
  e.erase("total_charged_delta_v_mps");
  json state={{"position_m",{0,0,0}},{"velocity_mps",{0,0,0}}};
  e.update(json{{"status","checkpointed_accepted"},{"completed_probes",1},{"total_probes",80},
   {"iterations",0},{"final_trial",{{"launch_parking_state",state},
    {"impulses_mps",json::array({json::array({1,0,0}),json::array({1,0,0}),json::array({1,0,0}),json::array({1,0,0})})},
    {"checkpoint_targets_relative",json::array({state,state,state,state})}}},
   {"coarse_diagnostics",{{"burns",burns},{"checkpoints",json::array()},
    {"selected_venus",{{"body_id","venus"},{"ut_s",5186000.0},{"distance_m",1500000.0},{"clearance_m",400000.0}}},
    {"venus_event_count",1},{"minimum_observed_venus_boundary_margin_m",399000.0},
    {"mars_radius",{{"observed_minimum_m",1200000.0},{"observed_minimum_ut_s",1000.0},
      {"observed_maximum_m",1210000.0},{"observed_maximum_ut_s",5185000.0},
      {"model_interval_lower_m",1195000.0},{"model_interval_upper_m",1215000.0}}},
    {"total_charged_delta_v_mps",4.0}}},
   {"strict",{{"coarse",fixed["coarse"]},{"fine",fixed["strict"]},
    {"total_charged_delta_v_mps",4.0},{"disagreement",fixed["disagreement"]}}}});
  const double epochs[4]={0,1000,5185000,5187000};
  const char* names[4]={"launch","Mars capture","Mars pre-departure","home return capture"};
  for(int i=0;i<4;++i)e["coarse_diagnostics"]["checkpoints"].push_back({{"name",names[i]},
   {"ut_s",epochs[i]},{"signed_position_residual_m",{0,0,0}},
   {"signed_velocity_residual_mps",{0,0,0}},{"signed_parking_radius_residual_m",0.0},
   {"signed_radial_velocity_mps",0.0},{"signed_tangential_speed_residual_mps",0.0}});
 }
 return e;
}
void shooting_validator_cases(){
 auto create=[](){return WorkerEventValidator("shooting-client-test",std::string(64,'a'),
  "runtime_observed_uncompared",0,WorkerResultKind::bounded_shooting);};
 auto started=shooting_event("started"),progress=shooting_event("progress"),done=shooting_event("complete");
 auto v=create();v.accept_line(started.dump());v.accept_line(progress.dump());v.accept_line(done.dump());
 check(v.terminal(),"shooting accepted stream terminal");
 rejects([&]{v.accept_line(done.dump());},"after terminal");
 auto bad_started=started;bad_started["snapshot_hash"]="wrong";
 rejects([&]{auto x=create();x.accept_line(bad_started.dump());},"snapshot_hash");
 bad_started=started;bad_started["source_confidence"]="synthetic_fixture";
 rejects([&]{auto x=create();x.accept_line(bad_started.dump());},"confidence");
 bad_started=started;bad_started["frame_axes"]="rotating";
 rejects([&]{auto x=create();x.accept_line(bad_started.dump());},"frame");
 auto bad=progress;bad["completed_probes"]=81;
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(bad.dump());},"cap");
 bad=progress;bad["total_probes"]=10001;
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(bad.dump());},"cap");
 bad=done;bad["state_epoch_ut_s"]=1;
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(progress.dump());x.accept_line(bad.dump());},"epoch");
 bad=done;bad["route_seed_evidence_revalidated"]=true;
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(progress.dump());x.accept_line(bad.dump());},"evidence");
 bad=done;bad["coarse_diagnostics"]["checkpoints"][0]["signed_position_residual_m"]={1,2};
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(progress.dump());x.accept_line(bad.dump());},"SI vector");
 bad=done;bad["strict"]["coarse"]["burns"][0]["delta_v_mps"]={0,1,0};
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(progress.dump());x.accept_line(bad.dump());},"impulse");
 bad=done;bad["status"]="strict_rejected";bad["result_label"]="independent_nbody_coarse_trial_diagnostic_only";bad.erase("strict");
 auto diagnostic=create();diagnostic.accept_line(started.dump());diagnostic.accept_line(progress.dump());diagnostic.accept_line(bad.dump());
 check(diagnostic.terminal(),"shooting diagnostic terminal");
 bad["strict"]=done["strict"];
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(progress.dump());x.accept_line(bad.dump());},"strict");
 bad=done;bad["strict"]["coarse"].erase("mars_radius");
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(progress.dump());x.accept_line(bad.dump());},"mars_radius");
 bad=done;bad["strict"]["fine"].erase("accepted_steps");
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(progress.dump());x.accept_line(bad.dump());},"accepted_steps");
 bad=done;bad["principia_comparison"]="matched";
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(progress.dump());x.accept_line(bad.dump());},"unsupported");
 bad=done;bad["final_trial"]["checkpoint_targets_relative"][0]["verification_status"]="verified";
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(progress.dump());x.accept_line(bad.dump());},"unsupported");
 bad=shooting_event("cancelled");bad["status"]="during_numerical_work";
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(progress.dump());x.accept_line(bad.dump());},"count");
 bad["completed_probes"]=0;
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(progress.dump());x.accept_line(bad.dump());},"count");
 auto pre_cancel=shooting_event("cancelled");pre_cancel["status"]="before_numerical_work";
 auto cancelled=create();cancelled.accept_line(pre_cancel.dump());
 check(cancelled.terminal(),"shooting pre-start cancellation terminal");
 auto mid_cancel=shooting_event("cancelled");mid_cancel["status"]="during_numerical_work";
 mid_cancel["completed_probes"]=1;
 auto mid=create();mid.accept_line(started.dump());mid.accept_line(progress.dump());
 mid.accept_line(mid_cancel.dump());check(mid.terminal(),"shooting between-probe cancellation terminal");
 rejects([&]{mid.accept_line(done.dump());},"after terminal");
 auto later=progress;later["completed_probes"]=2;
 auto regressed=progress;regressed["completed_probes"]=1;
 rejects([&]{auto x=create();x.accept_line(started.dump());x.accept_line(later.dump());
  x.accept_line(regressed.dump());},"regression");
}
ClientOptions options(const std::string& path,const std::string& mode){ClientOptions x;x.executable_path=path;x.arguments={mode};
 x.request_line=request();x.request_id="client-test";x.expected_snapshot_hash="synthetic-worker-v1";
 x.expected_source_confidence="synthetic_fixture";x.timeout=std::chrono::milliseconds(5000);
 x.cancel_grace=std::chrono::milliseconds(150);x.max_retained_events=64;return x;}
std::vector<json> finish(WorkerClient& c){check(c.wait_for(std::chrono::seconds(8)),"client finishes");return c.drain();}
bool error(const std::vector<json>& events,const std::string& code){for(const auto& e:events)if(e.value("type",std::string{})=="client_error"&&e.value("code",std::string{})==code)return true;return false;}
void process_cases(const std::string& fake){WorkerClient c;auto x=options(fake,"valid");x.executable_path=fake+".missing";
 check(c.start(x),"missing executable asynchronous attempt");check(error(finish(c),"spawn_failed"),"missing executable error");
 for(const auto& p:{std::pair{"malformed","invalid_event"},{"oversized","invalid_event"},{"wrong_request","invalid_event"},
  {"wrong_source","invalid_event"},{"progress_regress","invalid_event"},{"premature_eof","premature_eof"},{"nonzero_exit","nonzero_exit"}}){
  x=options(fake,p.first);check(c.start(x),"fake launched");auto got=finish(c);if(!error(got,p.second))throw std::runtime_error(std::string("fake failure classified: ")+p.first+" expected "+p.second+" got "+json(got).dump());}
 x=options(fake,"hang_cancel");check(c.start(x),"hang fake launched");check(!c.start(x),"second start rejected");
 c.cancel();check(error(finish(c),"cancel_timeout"),"cancel escalation and reap");
 x=options(fake,"cancel_ack");check(c.start(x),"cancel acknowledging fake launched");
 std::vector<json> cancelled;bool saw_started=false;
 const auto ready_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 while(!saw_started&&std::chrono::steady_clock::now()<ready_deadline){
  auto batch=c.drain();for(auto& event:batch){
   if(event.value("type",std::string{})=="started")saw_started=true;
   cancelled.push_back(std::move(event));
  }
  if(!saw_started)std::this_thread::sleep_for(std::chrono::milliseconds(5));
 }
 check(saw_started,"fake worker accepted request before cancellation");c.cancel();
 auto terminal=finish(c);cancelled.insert(cancelled.end(),terminal.begin(),terminal.end());
 check(!cancelled.empty()&&cancelled.back().value("type",std::string{})=="cancelled","versioned cancellation acknowledged");
 x=options(fake,"valid");check(c.start(x),"restart after reap");auto events=finish(c);
 check(!events.empty()&&events.back()["type"]=="complete","valid fake completion");}
void evaluation_process_cases(const std::string& fake){
 WorkerClient client;auto x=options(fake,"evaluation_valid");
 x.request_id="evaluation-client-test";x.expected_snapshot_hash=std::string(64,'a');
 x.expected_source_confidence="runtime_observed_uncompared";
 x.result_kind=WorkerResultKind::fixed_impulse_evaluation;
 x.request_line=json{{"protocol_version",1},{"command","evaluate_route"},
  {"request_id",x.request_id}}.dump();
 check(client.start(x),"evaluation fake starts without blocking caller");
 const auto valid=finish(client);
 check(valid.size()==4&&valid.front()["type"]=="started"&&valid.back()["type"]=="complete",
  "evaluation fake process round-trip");
 x.arguments={"evaluation_wrong_venus"};check(client.start(x),"evaluation invalid fake starts");
 check(error(finish(client),"invalid_event"),"evaluation malformed child event rejected");
 x.arguments={"evaluation_eof"};check(client.start(x),"evaluation EOF fake starts");
 check(error(finish(client),"premature_eof"),"evaluation premature EOF bounded and reaped");
 x.arguments={"evaluation_cancelled"};check(client.start(x),"evaluation cancel fake starts");
 const auto cancelled=finish(client);
 check(cancelled.size()==1&&cancelled.back()["type"]=="cancelled",
  "evaluation source-bound pre-start cancellation terminal");
}
void shooting_process_cases(const std::string& fake){
 WorkerClient client;auto x=options(fake,"shooting_valid");
 x.request_id="shooting-client-test";x.expected_snapshot_hash=std::string(64,'a');
 x.expected_source_confidence="runtime_observed_uncompared";
 x.result_kind=WorkerResultKind::bounded_shooting;
 x.request_line=json{{"protocol_version",1},{"command","shoot_route"},
  {"request_id",x.request_id}}.dump();
 check(client.start(x),"shooting fake starts without blocking caller");
 const auto valid=finish(client);
 check(valid.size()==3&&valid.front()["type"]=="started"&&valid.back()["type"]=="complete",
  "shooting fake process accepted round-trip");
 x.arguments={"shooting_wrong_impulse"};check(client.start(x),"shooting invalid fake starts");
 check(error(finish(client),"invalid_event"),"shooting malformed child event rejected");
}
void blocked_write_cases(const std::string& fake){
 auto x=options(fake,"never_read");x.request_line=std::string(1024*1024-128,'x');
 x.timeout=std::chrono::milliseconds(350);x.cancel_grace=std::chrono::milliseconds(100);
 WorkerClient timed;auto begun=std::chrono::steady_clock::now();
 check(timed.start(x),"blocked request starts asynchronously");
 check(error(finish(timed),"timeout"),"blocked request write reaches deadline");
 check(std::chrono::steady_clock::now()-begun<std::chrono::seconds(3),"blocked request deadline bounded");
 x.timeout=std::chrono::seconds(10);WorkerClient cancelled;begun=std::chrono::steady_clock::now();
 check(cancelled.start(x),"blocked request starts for cancellation");cancelled.cancel();
 check(error(finish(cancelled),"cancelled"),"blocked request write responds to cancellation");
 check(std::chrono::steady_clock::now()-begun<std::chrono::seconds(3),"blocked request cancellation bounded");
 begun=std::chrono::steady_clock::now();{WorkerClient owner;check(owner.start(x),"blocked request starts for destruction");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));}
 check(std::chrono::steady_clock::now()-begun<std::chrono::seconds(3),"blocked request destruction bounded");
}
void real_case(const std::string& worker){WorkerClient c;auto x=options(worker,"valid");x.arguments.clear();
 check(c.start(x),"real worker launched");auto events=finish(c);
 if(events.empty()||events.front().value("type",std::string{})!="started"||
    events.back().value("type",std::string{})!="complete")
  throw std::runtime_error("real worker ordered terminal: "+json(events).dump());
 ++checks;}
void real_mission_case(const std::string& worker){
 json request={{"protocol_version",1},{"command","start_mission"},{"request_id","mission-client-test"},
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
 WorkerClient c;ClientOptions x;x.executable_path=worker;x.request_line=request.dump();
 x.request_id="mission-client-test";x.expected_snapshot_hash="synthetic-mission-worker-v1";
 x.expected_source_confidence="synthetic_fixture";x.result_kind=WorkerResultKind::screened_route;
 x.max_candidates=5;x.timeout=std::chrono::seconds(15);
 check(c.start(x),"real mission worker launched");
 check(c.wait_for(std::chrono::seconds(18)),"real mission worker exits");
 const auto events=c.drain();
 if(events.empty()||events.front().value("type",std::string{})!="started"||
    events.back().value("type",std::string{})!="complete"||
    events.back()["ranked_routes"].empty())
  throw std::runtime_error("real mission event stream: "+json(events).dump());
 ++checks;
}
}
int main(int argc,char** argv){try{if(argc!=3)throw std::runtime_error("paths required");validator_cases();mission_validator_cases();evaluation_validator_cases();shooting_validator_cases();process_cases(argv[2]);evaluation_process_cases(argv[2]);shooting_process_cases(argv[2]);blocked_write_cases(argv[2]);real_case(argv[1]);real_mission_case(argv[1]);
 std::cout<<"PASS "<<checks<<" worker client checks\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
