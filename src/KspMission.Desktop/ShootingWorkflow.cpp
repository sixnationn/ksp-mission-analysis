#include "ShootingWorkflow.hpp"
#include "RuntimeReader.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
namespace ksp {
namespace {
using json=nlohmann::json;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
void allowed(const json& value,std::initializer_list<const char*> fields,const char* message){
 require(value.is_object(),message);std::unordered_set<std::string> names(fields.begin(),fields.end());
 for(auto it=value.begin();it!=value.end();++it)require(names.contains(it.key()),message);
}
std::string fmt(double value,int digits=3){
 std::ostringstream out;out<<std::fixed<<std::setprecision(digits)<<value;return out.str();
}
double finite_number(const json& value,const char* key){
 require(value.is_object()&&value.contains(key)&&value.at(key).is_number(),
  "shooting trial SI number missing");
 const auto result=value.at(key).get<double>();
 require(std::isfinite(result),"shooting trial SI number nonfinite");return result;
}
double vector_norm(const json& value){
 require(value.is_array()&&value.size()==3,"shooting SI vector must have three components");
 double squared=0;for(const auto& component:value){
  require(component.is_number(),"shooting SI vector component invalid");
  const auto x=component.get<double>();require(std::isfinite(x),"shooting SI vector nonfinite");
  squared+=x*x;
 }require(std::isfinite(squared),"shooting SI vector magnitude overflow");return std::sqrt(squared);
}
void check_state(const json& value){
 allowed(value,{"position_m","velocity_mps"},"shooting state field invalid");
 require(value.contains("position_m")&&value.contains("velocity_mps"),"shooting state incomplete");
 (void)vector_norm(value.at("position_m"));(void)vector_norm(value.at("velocity_mps"));
}
std::string vec(const json& value){
 require(value.is_array()&&value.size()==3,"signed SI residual vector invalid");
 for(const auto& component:value)require(component.is_number()&&std::isfinite(component.get<double>()),
  "signed SI residual vector nonfinite");
 return "["+fmt(value.at(0).get<double>())+", "+fmt(value.at(1).get<double>())+", "+
  fmt(value.at(2).get<double>())+"]";
}
void reject_hidden_source(const json& value){
 if(value.is_object())for(auto it=value.begin();it!=value.end();++it){
  require(it.key()!="source"&&it.key()!="source_confidence"&&
   it.key()!="atmosphere_boundaries","shooting trial contains hidden source or atmosphere claim");
  reject_hidden_source(it.value());
 }else if(value.is_array())for(const auto& item:value)reject_hidden_source(item);
}
}
json build_shooting_request(const ShootingSource& source,const std::string& trial_bytes,
 const ShootingLimits& limits,const std::string& request_id){
 require(!source.path.empty()&&!source.hash.empty()&&source.confidence=="runtime_observed_uncompared"&&
  source.frame_origin=="system_barycenter"&&source.frame_axes=="principia_alicesun_frozen_at_capture"&&
  std::isfinite(source.state_epoch_ut_s),"imported runtime source/frame invalid");
 require(!request_id.empty()&&request_id.size()<=128,"shooting request ID invalid");
 require(std::isfinite(limits.finite_difference_impulse_mps)&&limits.finite_difference_impulse_mps>0&&
  std::isfinite(limits.max_impulse_mps)&&limits.max_impulse_mps>0&&
  limits.max_iterations>0&&limits.max_iterations<=1000&&
  limits.max_probe_evaluations>0&&limits.max_probe_evaluations<=10000,
  "shooting limits invalid");
 require(!trial_bytes.empty()&&trial_bytes.size()<=1024*1024,"shooting trial exceeds 1 MiB or is empty");
 std::ifstream file(source.path,std::ios::binary|std::ios::ate);
 require(static_cast<bool>(file),"imported runtime path cannot be reopened");
 const auto length=file.tellg();require(length>0&&length<=16*1024*1024,"imported runtime source size invalid");
 std::string bytes(static_cast<std::size_t>(length),'\0');file.seekg(0);
 require(static_cast<bool>(file.read(bytes.data(),length)),"imported runtime source read incomplete");
 file.clear();file.seekg(0,std::ios::end);
 require(file&&file.tellg()==length&&sha256_hex(bytes)==source.hash,
  "imported runtime source changed; import again");
 auto trial=json::parse(trial_bytes,nullptr,false);require(trial.is_object(),"shooting trial JSON invalid");
 allowed(trial,{"route_seed","launch_parking_state","impulses_mps","checkpoint_targets_relative",
  "home_parking_altitude_m","mars_parking_altitude_m","home_capture_altitude_m",
  "fixed_position_tolerance_m","fixed_velocity_tolerance_mps","parking_radius_tolerance_m",
  "parking_radial_velocity_tolerance_mps","parking_tangential_speed_tolerance_mps",
  "venus_window_halfwidth_s","venus_max_encounter_radius_m","venus_safety_margin_m",
  "disagreement_position_m","disagreement_velocity_mps","disagreement_event_time_s",
  "disagreement_mars_extremum_radius_m","disagreement_mars_extremum_time_s",
  "coarse_ephemeris","strict_ephemeris","coarse_spacecraft","strict_spacecraft"},
  "shooting trial contains unsupported source or atmosphere claim");
 require(trial.contains("route_seed")&&trial.at("route_seed").is_object(),"shooting route seed missing");
 const auto& seed=trial.at("route_seed");
 allowed(seed,{"central_body_id","home_body_id","mars_body_id","venus_body_id","launch_ut_s",
  "mars_arrival_ut_s","mars_departure_ut_s","venus_encounter_ut_s","home_return_ut_s",
  "fixed_stay_s","snapshot_hash","source_confidence"},"shooting route seed field invalid");
 require(seed.value("snapshot_hash",std::string{})==source.hash&&
  seed.value("source_confidence",std::string{})==source.confidence,
  "shooting trial route seed differs from imported source");
 const double launch=finite_number(seed,"launch_ut_s"),arrival=finite_number(seed,"mars_arrival_ut_s"),
  departure=finite_number(seed,"mars_departure_ut_s"),encounter=finite_number(seed,"venus_encounter_ut_s"),
  home=finite_number(seed,"home_return_ut_s");
 require(launch<arrival&&arrival<departure&&departure<encounter&&encounter<home&&
  finite_number(seed,"fixed_stay_s")==5184000.0&&departure-arrival==5184000.0,
  "shooting fixed dates or 5,184,000-second Mars stay invalid");
 for(auto it=trial.begin();it!=trial.end();++it)
  if(it.key()!="route_seed")reject_hidden_source(it.value());
 require(trial.contains("launch_parking_state")&&trial.contains("impulses_mps")&&
  trial.contains("checkpoint_targets_relative"),"shooting physical trial incomplete");
 check_state(trial.at("launch_parking_state"));
 const auto& impulses=trial.at("impulses_mps"),&targets=trial.at("checkpoint_targets_relative");
 require(impulses.is_array()&&impulses.size()==4&&targets.is_array()&&targets.size()==4,
  "shooting requires four fixed impulses and targets");
 for(std::size_t i=0;i<4;++i){
  require(vector_norm(impulses.at(i))<=limits.max_impulse_mps,
   "shooting seed impulse exceeds per-impulse cap");
  check_state(targets.at(i));
 }
 const json request={{"protocol_version",1},{"command","shoot_route"},{"request_id",request_id},
  {"source",{{"mode","runtime_snapshot"},{"path",source.path},
   {"expected_snapshot_hash",source.hash},{"expected_frame_origin",source.frame_origin},
   {"expected_frame_axes",source.frame_axes},{"expected_state_epoch_ut_s",source.state_epoch_ut_s}}},
  {"trial",trial},{"shooting",{{"finite_difference_impulse_mps",limits.finite_difference_impulse_mps},
   {"max_impulse_mps",limits.max_impulse_mps},{"max_iterations",limits.max_iterations},
   {"max_probe_evaluations",limits.max_probe_evaluations}}}};
 require(request.dump().size()<=1024*1024,"shooting worker request exceeds 1 MiB");
 return request;
}
ShootingPresentation present_shooting_completion(const json& terminal){
 require(terminal.is_object()&&terminal.value("type",std::string{})=="complete",
  "shooting completion required");
 require(terminal.at("route_seed_evidence_revalidated")==false&&
  terminal.at("mars_stay_continuously_verified")==false,"shooting evidence flags invalid");
 const auto status=terminal.at("status").get<std::string>();
 const bool accepted=status=="checkpointed_accepted";
 static const std::unordered_set<std::string> diagnostics={"strict_rejected","budget_exhausted",
  "missing_venus","venus_boundary_or_radius","mars_stay_failed","coarse_constraints_failed",
  "singular_jacobian","unsafe_difference","line_search_failed","impulse_bound"};
 require(accepted||diagnostics.contains(status),"unsupported shooting completion status");
 const auto expected=accepted?"independent_nbody_fixed_impulse_checkpointed_only":
  "independent_nbody_coarse_trial_diagnostic_only";
 require(terminal.at("result_label")==expected&&terminal.contains("strict")==accepted,
  "shooting status and result label inconsistent");
 const auto& probe=terminal.at("coarse_diagnostics");
 const auto& burns=probe.at("burns"),&checks=probe.at("checkpoints");
 require(burns.is_array()&&burns.size()==4&&checks.is_array()&&checks.size()==4,
  "shooting four burns and checkpoints required");
 std::ostringstream text;text<<(accepted?"Checkpointed nearby-trial result":"Diagnostic non-success: "+status)
  <<" · "<<expected<<"\nRoute seed revalidated: false · continuous Mars stay verified: false";
 for(std::size_t i=0;i<4;++i){const auto& check=checks.at(i);
  text<<"\n"<<check.at("name").get<std::string>()<<" · final burn "<<
   fmt(burns.at(i).at("magnitude_mps").get<double>())<<" m/s"<<
   " · signed position "<<vec(check.at("signed_position_residual_m"))<<" m"<<
   " · signed velocity "<<vec(check.at("signed_velocity_residual_mps"))<<" m/s"<<
   " · parking radius "<<fmt(check.at("signed_parking_radius_residual_m").get<double>())<<" m"<<
   " · radial "<<fmt(check.at("signed_radial_velocity_mps").get<double>())<<" m/s"<<
   " · tangential "<<fmt(check.at("signed_tangential_speed_residual_mps").get<double>())<<" m/s";
 }
 const auto& selected=probe.at("selected_venus");
 if(selected.is_null())text<<"\nVenus: no selected actual event in the request window";
 else text<<"\nVenus actual event UT "<<fmt(selected.at("ut_s").get<double>(),0)<<" s · distance "<<
  fmt(selected.at("distance_m").get<double>())<<" m · clearance "<<
  fmt(selected.at("clearance_m").get<double>())<<" m";
 const auto& mars=probe.at("mars_radius");
 text<<"\nMars observed radius "<<fmt(mars.at("observed_minimum_m").get<double>())<<"–"<<
  fmt(mars.at("observed_maximum_m").get<double>())<<" m";
 if(accepted){const auto& strict=terminal.at("strict"),&fine=strict.at("fine");
  text<<"\nStrict checkpoint metrics:";
  for(const auto& check:fine.at("checkpoints"))
   text<<" "<<check.at("name").get<std::string>()<<" "<<
    fmt(check.at("position_error_m").get<double>())<<" m / "<<
    fmt(check.at("velocity_error_mps").get<double>(),6)<<" m/s;";
  const auto& venus=fine.at("venus"),&disagreement=strict.at("disagreement");
  text<<"\nStrict Venus UT "<<fmt(venus.at("ut_s").get<double>(),0)<<" s · distance "<<
   fmt(venus.at("distance_m").get<double>())<<" m · clearance "<<
   fmt(venus.at("clearance_m").get<double>())<<" m"<<
   " · coarse/strict disagreement "<<
   fmt(disagreement.at("maximum_checkpoint_position_m").get<double>())<<" m / "<<
   fmt(disagreement.at("maximum_checkpoint_velocity_mps").get<double>(),6)<<" m/s";
 }
 return {text.str(),true,accepted};
}
}
