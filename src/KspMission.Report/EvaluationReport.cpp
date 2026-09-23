#include "EvaluationReport.hpp"
#include "RuntimeReader.hpp"
#include "WorkerClient.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <set>
#include <unordered_set>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ksp {
namespace {
using json=nlohmann::json;
constexpr std::size_t source_limit=16*1024*1024,report_limit=32*1024*1024,event_limit=16;
constexpr const char* label="independent_nbody_fixed_impulse_checkpointed_only";
void require(bool condition,const char* message){if(!condition)throw EvaluationReportError(message);}
const json& object(const json& parent,const char* key){require(parent.is_object()&&parent.contains(key)&&parent.at(key).is_object(),"required object missing");return parent.at(key);}
const json& array(const json& parent,const char* key){require(parent.is_object()&&parent.contains(key)&&parent.at(key).is_array(),"required array missing");return parent.at(key);}
std::string string(const json& parent,const char* key){require(parent.is_object()&&parent.contains(key)&&parent.at(key).is_string(),"required string missing");auto s=parent.at(key).get<std::string>();require(!s.empty(),"required string empty");return s;}
double number(const json& parent,const char* key){require(parent.is_object()&&parent.contains(key)&&parent.at(key).is_number(),"required number missing");double x=parent.at(key).get<double>();require(std::isfinite(x),"nonfinite report number");return x;}
bool close_numbers(double a,double b){return std::abs(a-b)<=std::max(1e-8,1e-9*std::max(std::abs(a),std::abs(b)));}
void equal_number(double a,double b,const char* message){require(close_numbers(a,b),message);}
void allowed(const json& value,std::initializer_list<const char*> keys,const char* message){require(value.is_object(),message);std::unordered_set<std::string> names(keys.begin(),keys.end());for(auto it=value.begin();it!=value.end();++it)require(names.contains(it.key()),message);}
void reject_claims(const json& value){
 if(value.is_object())for(auto it=value.begin();it!=value.end();++it){
  require(it.key()!="feasible"&&it.key()!="principia_equivalent"&&it.key()!="installed_game_verified"&&it.key()!="optimized","unsupported verification claim");reject_claims(it.value());}
 else if(value.is_array())for(const auto& item:value)reject_claims(item);
}
void vector3(const json& value){require(value.is_array()&&value.size()==3,"SI vector must have three elements");for(const auto& item:value)require(item.is_number()&&std::isfinite(item.get<double>()),"SI vector nonfinite");}
void state(const json& value){allowed(value,{"position_m","velocity_mps"},"unsupported state field");vector3(array(value,"position_m"));vector3(array(value,"velocity_mps"));}
std::size_t cap(const json& value,const char* key,std::size_t maximum){require(value.is_object()&&value.contains(key)&&value.at(key).is_number_integer(),"request cap missing");auto count=value.at(key).get<std::int64_t>();require(count>0&&static_cast<std::uint64_t>(count)<=maximum,"request cap exceeds limit");return static_cast<std::size_t>(count);}
void check_settings(const json& trial,double launch,double arrival,double departure,double encounter,double end){
 const double longest=std::max({arrival-launch,departure-arrival,end-departure});
 for(const char* key:{"coarse_ephemeris","strict_ephemeris"}){const auto& q=object(trial,key);allowed(q,{"start_ut_s","end_ut_s","step_s","max_position_fit_error_m","max_velocity_fit_error_mps","max_steps"},"unsupported ephemeris setting");double first=number(q,"start_ut_s"),last=number(q,"end_ut_s"),step=number(q,"step_s");auto steps=cap(q,"max_steps",100000);
  require(first<=launch&&last>=end&&last>first&&step>0&&number(q,"max_position_fit_error_m")>0&&number(q,"max_velocity_fit_error_mps")>0,"ephemeris coverage or fit invalid");
  const double count=(last-first)/step;require(std::isfinite(count)&&count>=1&&count<=steps,"ephemeris steps exceed request cap");
  require(number(q,"max_position_fit_error_m")+number(q,"max_velocity_fit_error_mps")*longest<=0.1*number(trial,"fixed_position_tolerance_m"),"ephemeris fit exceeds position budget");}
 const auto& coarse_ep=object(trial,"coarse_ephemeris"),&strict_ep=object(trial,"strict_ephemeris");
 require(number(strict_ep,"start_ut_s")==number(coarse_ep,"start_ut_s")&&number(strict_ep,"end_ut_s")==number(coarse_ep,"end_ut_s")&&
  number(strict_ep,"step_s")<number(coarse_ep,"step_s")&&number(strict_ep,"max_position_fit_error_m")<number(coarse_ep,"max_position_fit_error_m")&&
  number(strict_ep,"max_velocity_fit_error_mps")<number(coarse_ep,"max_velocity_fit_error_mps"),"strict ephemeris settings not tighter");
 for(const char* key:{"coarse_spacecraft","strict_spacecraft"}){const auto& q=object(trial,key);allowed(q,{"start_ut_s","end_ut_s","abs_position_tolerance_m","abs_velocity_tolerance_mps","relative_tolerance","min_step_s","max_step_s","max_accepted_steps"},"spacecraft source/impulse override forbidden");
  require(number(q,"start_ut_s")==launch&&number(q,"end_ut_s")==end&&cap(q,"max_accepted_steps",1000000)>0,"spacecraft coverage/cap invalid");
  require(number(q,"abs_position_tolerance_m")>0&&number(q,"abs_velocity_tolerance_mps")>0&&number(q,"relative_tolerance")>0&&number(q,"min_step_s")>0&&number(q,"max_step_s")>=number(q,"min_step_s"),"spacecraft settings invalid");}
 const auto& coarse_sc=object(trial,"coarse_spacecraft"),&strict_sc=object(trial,"strict_spacecraft");
 require(number(strict_sc,"max_step_s")<number(coarse_sc,"max_step_s")&&
  number(strict_sc,"abs_position_tolerance_m")<number(coarse_sc,"abs_position_tolerance_m")&&
  number(strict_sc,"abs_velocity_tolerance_mps")<number(coarse_sc,"abs_velocity_tolerance_mps")&&
  number(strict_sc,"relative_tolerance")<number(coarse_sc,"relative_tolerance"),"strict spacecraft settings not tighter");
 require(number(trial,"venus_window_halfwidth_s")<std::min(encounter-departure,end-encounter),"Venus event window overlaps a burn");
}
void check_pass(const json& pass,const json& trial,const json& seed,const RuntimeLoad& loaded){
 allowed(pass,{"burns","checkpoints","accepted_steps","rejected_steps","venus","mars_radius"},"unsupported evaluation pass field");
 const auto& burns=array(pass,"burns"),&checks=array(pass,"checkpoints");require(burns.size()==4&&checks.size()==4,"four burns/checkpoints required");
 const auto& impulses=array(trial,"impulses_mps"),&targets=array(trial,"checkpoint_targets_relative");
 const char* epochs[]={"launch_ut_s","mars_arrival_ut_s","mars_departure_ut_s","home_return_ut_s"};
 for(std::size_t i=0;i<4;++i){allowed(burns[i],{"ut_s","delta_v_mps","magnitude_mps"},"unsupported burn field");
  allowed(checks[i],{"name","ut_s","position_error_m","velocity_error_mps","parking_radius_error_m","radial_velocity_mps","tangential_speed_error_mps"},"unsupported checkpoint field");
  require(number(burns[i],"ut_s")==number(seed,epochs[i]),"burn date differs from trial");
  const auto& actual=array(burns[i],"delta_v_mps");vector3(actual);for(std::size_t axis=0;axis<3;++axis)equal_number(actual[axis].get<double>(),impulses[i][axis].get<double>(),"burn vector differs from trial");
  require(number(checks[i],"ut_s")==number(seed,epochs[i]),"checkpoint date differs from trial");
  require(number(checks[i],"position_error_m")<=number(trial,"fixed_position_tolerance_m"),"checkpoint position exceeds request tolerance");
  require(number(checks[i],"velocity_error_mps")<=number(trial,"fixed_velocity_tolerance_mps"),"checkpoint velocity exceeds request tolerance");
  require(number(checks[i],"parking_radius_error_m")<=number(trial,"parking_radius_tolerance_m"),"parking radius exceeds request tolerance");
  require(number(checks[i],"radial_velocity_mps")<=number(trial,"parking_radial_velocity_tolerance_mps"),"parking radial speed exceeds request tolerance");
  require(number(checks[i],"tangential_speed_error_mps")<=number(trial,"parking_tangential_speed_tolerance_mps"),"parking tangential speed exceeds request tolerance");
  state(targets[i]);
 }
 const auto& venus=object(pass,"venus");allowed(venus,{"body_id","ut_s","distance_m","clearance_m","safety_margin_m"},"unsupported Venus event field");const double encounter=number(seed,"venus_encounter_ut_s"),ut=number(venus,"ut_s");
 require(std::abs(ut-encounter)<=number(trial,"venus_window_halfwidth_s"),"Venus event outside request window");
 const double distance=number(venus,"distance_m");require(distance<=number(trial,"venus_max_encounter_radius_m"),"Venus encounter radius exceeds request");
 const auto id=string(seed,"venus_body_id");auto body=std::find_if(loaded.snapshot.bodies.begin(),loaded.snapshot.bodies.end(),[&](const Body& b){return b.id==id;});
 require(body!=loaded.snapshot.bodies.end(),"Venus body absent");double atmosphere=0;
 auto atmosphere_entry=std::find_if(loaded.atmosphere_boundaries.begin(),loaded.atmosphere_boundaries.end(),[&](const AtmosphereBoundary& a){return a.body_id==id;});
 require(atmosphere_entry!=loaded.atmosphere_boundaries.end(),"Venus source atmosphere boundary missing");atmosphere=atmosphere_entry->altitude_m;
 equal_number(number(venus,"clearance_m"),distance-body->radius_m-atmosphere,"Venus clearance differs from source boundary");
 equal_number(number(venus,"safety_margin_m"),distance-body->radius_m-atmosphere-number(trial,"venus_safety_margin_m"),"Venus safety margin differs from source/request");
 require(number(venus,"safety_margin_m")>0,"Venus safety margin nonpositive");
 const auto& mars=object(pass,"mars_radius");allowed(mars,{"observed_minimum_m","observed_minimum_ut_s","observed_maximum_m","observed_maximum_ut_s","model_interval_lower_m","model_interval_upper_m","endpoint_count","root_count"},"unsupported Mars radius field");const auto mars_id=string(seed,"mars_body_id");
 auto mars_body=std::find_if(loaded.snapshot.bodies.begin(),loaded.snapshot.bodies.end(),[&](const Body& b){return b.id==mars_id;});require(mars_body!=loaded.snapshot.bodies.end(),"Mars body absent");
 const double shell=mars_body->radius_m+number(trial,"mars_parking_altitude_m"),tolerance=number(trial,"parking_radius_tolerance_m");
 auto mars_atmosphere=std::find_if(loaded.atmosphere_boundaries.begin(),loaded.atmosphere_boundaries.end(),[&](const AtmosphereBoundary& a){return a.body_id==mars_id;});
 require(mars_atmosphere!=loaded.atmosphere_boundaries.end()&&number(mars,"model_interval_lower_m")>mars_body->radius_m+mars_atmosphere->altitude_m,"Mars parking monitor reaches source atmosphere");
 require(number(mars,"model_interval_lower_m")>=shell-tolerance&&number(mars,"model_interval_upper_m")<=shell+tolerance,"Mars radius monitor exceeds requested shell");
}
void check_document(const json& document){
 require(document.is_object(),"report object required");
 reject_claims(document);
 allowed(document,{"schema_version","report_kind","result_label","route_seed_evidence_revalidated","mars_stay_continuously_verified","runtime_snapshot_json_bytes","snapshot_sha256","request","events"},"unsupported report field");
 require(document.contains("schema_version")&&document.at("schema_version").is_number_integer()&&document.at("schema_version")==1,"report schema version invalid");
 require(string(document,"report_kind")=="fixed_impulse_evaluation","report kind invalid");
 require(string(document,"result_label")==label,"report label invalid");
 require(document.value("route_seed_evidence_revalidated",true)==false&&document.value("mars_stay_continuously_verified",true)==false,"report evidence flags invalid");
 const auto bytes=string(document,"runtime_snapshot_json_bytes");require(bytes.size()<=source_limit,"runtime source exceeds 16 MiB");
 const auto& events=array(document,"events");require(events.size()==4&&events.size()<=event_limit,"event count invalid");
 require(document.dump().size()<=report_limit,"report exceeds 32 MiB");
 const auto hash=string(document,"snapshot_sha256");require(hash.size()==64&&sha256_hex(bytes)==hash,"runtime exact-byte hash mismatch");
 const auto loaded=read_runtime_snapshot(bytes,hash);require(loaded.snapshot.confidence=="runtime_observed_uncompared","runtime confidence invalid");
 const auto& request=object(document,"request");allowed(request,{"protocol_version","command","request_id","source","trial"},"unsupported evaluation request field");require(request.contains("protocol_version")&&request.at("protocol_version")==1&&string(request,"command")=="evaluate_route","evaluation request invalid");
 const auto id=string(request,"request_id");require(id.size()<=128&&request.dump().size()<=1024*1024,"request exceeds worker cap");
 require(!request.contains("source_confidence")&&!request.contains("atmosphere_boundaries"),"top-level source override forbidden");
 const auto& source=object(request,"source");allowed(source,{"mode","path","expected_snapshot_hash","expected_frame_origin","expected_frame_axes","expected_state_epoch_ut_s"},"source override or unsupported field");
 require(string(source,"mode")=="runtime_snapshot"&&string(source,"path").size()>0&&string(source,"expected_snapshot_hash")==hash,"request source identity mismatch");
 require(string(source,"expected_frame_origin")==loaded.snapshot.frame.origin&&string(source,"expected_frame_axes")==loaded.snapshot.frame.axes,"request frame mismatch");
 require(number(source,"expected_state_epoch_ut_s")==*loaded.snapshot.state_epoch_ut_s,"request state epoch mismatch");
 const auto& trial=object(request,"trial");allowed(trial,{"route_seed","launch_parking_state","impulses_mps","checkpoint_targets_relative","home_parking_altitude_m","mars_parking_altitude_m","home_capture_altitude_m","fixed_position_tolerance_m","fixed_velocity_tolerance_mps","parking_radius_tolerance_m","parking_radial_velocity_tolerance_mps","parking_tangential_speed_tolerance_mps","venus_window_halfwidth_s","venus_max_encounter_radius_m","venus_safety_margin_m","disagreement_position_m","disagreement_velocity_mps","disagreement_event_time_s","disagreement_mars_extremum_radius_m","disagreement_mars_extremum_time_s","coarse_ephemeris","strict_ephemeris","coarse_spacecraft","strict_spacecraft"},"trial override or unsupported field");
 const auto& seed=object(trial,"route_seed");allowed(seed,{"central_body_id","home_body_id","mars_body_id","venus_body_id","launch_ut_s","mars_arrival_ut_s","mars_departure_ut_s","venus_encounter_ut_s","home_return_ut_s","fixed_stay_s","snapshot_hash","source_confidence"},"route seed override or unsupported field");
 require(string(seed,"snapshot_hash")==hash&&string(seed,"source_confidence")==loaded.snapshot.confidence,"route seed provenance mismatch");
 std::set<std::string> roles;for(const char* key:{"central_body_id","home_body_id","mars_body_id","venus_body_id"}){auto role=string(seed,key);roles.insert(role);require(std::any_of(loaded.snapshot.bodies.begin(),loaded.snapshot.bodies.end(),[&](const Body& b){return b.id==role;}),"role absent from runtime source");}require(roles.size()==4,"route roles not distinct");
 const double t0=number(seed,"launch_ut_s"),t1=number(seed,"mars_arrival_ut_s"),t2=number(seed,"mars_departure_ut_s"),t3=number(seed,"venus_encounter_ut_s"),t4=number(seed,"home_return_ut_s");
 require(t0<t1&&t1<t2&&t2<t3&&t3<t4&&t2-t1==5184000.0&&number(seed,"fixed_stay_s")==5184000.0,"fixed stay or dates invalid");
 check_settings(trial,t0,t1,t2,t3,t4);
 auto boundary_for=[&](const std::string& id){auto found=std::find_if(loaded.atmosphere_boundaries.begin(),loaded.atmosphere_boundaries.end(),[&](const AtmosphereBoundary& a){return a.body_id==id;});require(found!=loaded.atmosphere_boundaries.end(),"source atmosphere boundary missing");return found->altitude_m;};
 require(number(trial,"home_parking_altitude_m")>=boundary_for(string(seed,"home_body_id"))&&
  number(trial,"home_capture_altitude_m")>=boundary_for(string(seed,"home_body_id"))&&
  number(trial,"mars_parking_altitude_m")>=boundary_for(string(seed,"mars_body_id")),"parking altitude below source atmosphere");
 auto venus_body=std::find_if(loaded.snapshot.bodies.begin(),loaded.snapshot.bodies.end(),[&](const Body& b){return b.id==string(seed,"venus_body_id");});
 require(venus_body!=loaded.snapshot.bodies.end()&&number(trial,"venus_max_encounter_radius_m")>venus_body->radius_m+boundary_for(string(seed,"venus_body_id"))+number(trial,"venus_safety_margin_m"),"Venus encounter maximum is unsafe");
 state(object(trial,"launch_parking_state"));const auto& impulses=array(trial,"impulses_mps"),&targets=array(trial,"checkpoint_targets_relative");require(impulses.size()==4&&targets.size()==4,"four trial impulses/targets required");for(std::size_t i=0;i<4;++i){vector3(impulses[i]);state(targets[i]);}
 for(const char* key:{"fixed_position_tolerance_m","fixed_velocity_tolerance_mps","parking_radius_tolerance_m","parking_radial_velocity_tolerance_mps","parking_tangential_speed_tolerance_mps","venus_window_halfwidth_s","venus_max_encounter_radius_m","disagreement_position_m","disagreement_velocity_mps","disagreement_event_time_s","disagreement_mars_extremum_radius_m","disagreement_mars_extremum_time_s"})require(number(trial,key)>0,"request tolerance invalid");
 for(const char* key:{"home_parking_altitude_m","mars_parking_altitude_m","home_capture_altitude_m","venus_safety_margin_m"})require(number(trial,key)>=0,"request altitude/margin invalid");
 WorkerEventValidator validator(id,hash,loaded.snapshot.confidence,0,WorkerResultKind::fixed_impulse_evaluation);
 bool start=false,zero=false,one=false;std::size_t complete=0;
 for(std::size_t i=0;i<events.size();++i){const auto& event=events[i];require(event.is_object(),"event object required");require(event.dump().size()<=1024*1024,"event line exceeds worker client cap");
  const auto type=string(event,"type");require(type=="started"||type=="progress"||type=="complete","incomplete worker terminal in report");
  if(type=="started")allowed(event,{"protocol_version","request_id","type","snapshot_hash","source_confidence","source_mode","frame_origin","frame_axes","frame_handedness","state_epoch_ut_s","units","result_label","non_interruptible_stages"},"unsupported started event field");
  if(type=="progress")allowed(event,{"protocol_version","request_id","type","snapshot_hash","source_confidence","phase","completed_phases","total_phases"},"unsupported progress event field");
  if(type=="complete")allowed(event,{"protocol_version","request_id","type","snapshot_hash","source_confidence","result_label","role_body_ids","route_seed_evidence_revalidated","mars_stay_continuously_verified","frame_origin","frame_axes","frame_handedness","state_epoch_ut_s","units","force_model","coarse","strict","total_charged_delta_v_mps","disagreement"},"unsupported complete event field");
  if(type=="started"){require(i==0&&!start,"started event order invalid");start=true;require(string(event,"frame_origin")==loaded.snapshot.frame.origin&&string(event,"frame_axes")==loaded.snapshot.frame.axes&&string(event,"frame_handedness")==loaded.snapshot.frame.handedness,"started frame mismatch");require(number(event,"state_epoch_ut_s")==*loaded.snapshot.state_epoch_ut_s,"started epoch mismatch");}
  if(type=="progress"){require(start&&!complete,"progress event order invalid");double phase=number(event,"completed_phases");if(phase==0){require(!one,"zero progress after finished");zero=true;}else if(phase==1){require(zero,"finished progress without zero");one=true;}}
  if(type=="complete"){require(i+1==events.size()&&start&&zero&&one&&++complete==1,"complete event order invalid");const auto& mapped=object(event,"role_body_ids");allowed(mapped,{"central","home","mars","venus"},"unsupported role field");for(const auto& [actual,seed_key]:std::array<std::pair<const char*,const char*>,4>{{{"central","central_body_id"},{"home","home_body_id"},{"mars","mars_body_id"},{"venus","venus_body_id"}}})require(string(mapped,actual)==string(seed,seed_key),"complete role differs from request");
   require(string(event,"frame_origin")==loaded.snapshot.frame.origin&&string(event,"frame_axes")==loaded.snapshot.frame.axes&&string(event,"frame_handedness")==loaded.snapshot.frame.handedness,"complete frame mismatch");require(number(event,"state_epoch_ut_s")==*loaded.snapshot.state_epoch_ut_s,"complete epoch mismatch");
   check_pass(object(event,"coarse"),trial,seed,loaded);check_pass(object(event,"strict"),trial,seed,loaded);
   const auto& disagreement=object(event,"disagreement");allowed(disagreement,{"maximum_checkpoint_position_m","maximum_checkpoint_velocity_mps","venus_event_time_s","venus_radius_m","mars_minimum_radius_m","mars_maximum_radius_m","mars_minimum_time_s","mars_maximum_time_s"},"unsupported disagreement field");
   for(const auto& [metric,budget]:std::array<std::pair<const char*,const char*>,7>{{{"maximum_checkpoint_position_m","disagreement_position_m"},{"maximum_checkpoint_velocity_mps","disagreement_velocity_mps"},{"venus_event_time_s","disagreement_event_time_s"},{"mars_minimum_radius_m","disagreement_mars_extremum_radius_m"},{"mars_maximum_radius_m","disagreement_mars_extremum_radius_m"},{"mars_minimum_time_s","disagreement_mars_extremum_time_s"},{"mars_maximum_time_s","disagreement_mars_extremum_time_s"}}})require(number(disagreement,metric)<=number(trial,budget),"coarse/strict disagreement exceeds request budget");
   const auto& coarse=object(event,"coarse"),&strict=object(event,"strict");
   const auto& cv=object(coarse,"venus"),&sv=object(strict,"venus");
   require(number(disagreement,"venus_radius_m")<=0.1*std::min(number(cv,"safety_margin_m"),number(sv,"safety_margin_m")),"Venus radius disagreement exceeds safety margin rule");
   auto derived=[&](const char* report_key,const json& left,const json& right,const char* value_key){
    equal_number(number(disagreement,report_key),std::abs(number(left,value_key)-number(right,value_key)),"reported disagreement differs from passes");};
   derived("venus_event_time_s",cv,sv,"ut_s");derived("venus_radius_m",cv,sv,"distance_m");
   const auto& cm=object(coarse,"mars_radius"),&sm=object(strict,"mars_radius");
   derived("mars_minimum_radius_m",cm,sm,"observed_minimum_m");
   derived("mars_maximum_radius_m",cm,sm,"observed_maximum_m");
   derived("mars_minimum_time_s",cm,sm,"observed_minimum_ut_s");
   derived("mars_maximum_time_s",cm,sm,"observed_maximum_ut_s");
  }
  validator.accept_line(event.dump());
 }
 require(complete==1&&validator.terminal(),"completed worker stream missing");
}
void write_synced(const std::filesystem::path& path,const std::string& bytes){
#ifdef _WIN32
 FILE* file=_wfopen(path.c_str(),L"wb");
#else
 FILE* file=std::fopen(path.c_str(),"wb");
#endif
 if(!file)throw EvaluationReportError("temporary report open failed");
 bool ok=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size()&&std::fflush(file)==0;
#ifdef _WIN32
 if(ok)ok=_commit(_fileno(file))==0;
#else
 if(ok)ok=fsync(fileno(file))==0;
#endif
 if(std::fclose(file)!=0)ok=false;require(ok,"temporary report write failed");
}
void replace_file(const std::filesystem::path& temporary,const std::filesystem::path& path){
#ifdef _WIN32
 require(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"atomic report replacement failed");
#else
 require(std::rename(temporary.c_str(),path.c_str())==0,"atomic report replacement failed");
#endif
}
}
nlohmann::json compose_fixed_evaluation_report(const std::string& runtime_bytes,const std::string& expected_sha256,const json& request,const std::vector<json>& events){
 try{require(runtime_bytes.size()<=source_limit,"runtime source exceeds 16 MiB");json document={{"schema_version",1},{"report_kind","fixed_impulse_evaluation"},{"result_label",label},{"route_seed_evidence_revalidated",false},{"mars_stay_continuously_verified",false},{"runtime_snapshot_json_bytes",runtime_bytes},{"snapshot_sha256",expected_sha256},{"request",request},{"events",events}};validate_fixed_evaluation_report(document);return document;}
 catch(const EvaluationReportError&){throw;}catch(const std::exception& e){throw EvaluationReportError(e.what());}
}
void validate_fixed_evaluation_report(const json& document){try{check_document(document);}catch(const EvaluationReportError&){throw;}catch(const std::exception& e){throw EvaluationReportError(e.what());}}
void save_fixed_evaluation_report(const json& document,const std::filesystem::path& path){
 validate_fixed_evaluation_report(document);require(!path.empty(),"report path missing");const auto bytes=document.dump(2)+"\n";require(bytes.size()<=report_limit,"report exceeds 32 MiB");
 auto temporary=path;temporary+=std::filesystem::path(".tmp."+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 try{write_synced(temporary,bytes);replace_file(temporary,path);}catch(...){std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
}
json load_fixed_evaluation_report(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary|std::ios::ate);require(static_cast<bool>(file),"report open failed");const auto length=file.tellg();require(length>=0&&length<=static_cast<std::streamoff>(report_limit),"report exceeds 32 MiB");std::string bytes(static_cast<std::size_t>(length),'\0');file.seekg(0);require(static_cast<bool>(file.read(bytes.data(),length)),"report read failed");auto document=json::parse(bytes,nullptr,false);require(!document.is_discarded(),"report JSON invalid");validate_fixed_evaluation_report(document);return document;}
}
