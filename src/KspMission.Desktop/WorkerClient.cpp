#include "WorkerClient.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <set>
#include <sstream>
#include <utility>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace ksp {
namespace {
using json=nlohmann::json;
constexpr std::size_t line_limit=1024*1024,stderr_limit=4096;
std::size_t count(const json& value,const char* key){
    if(!value.contains(key)||!value.at(key).is_number_integer())throw WorkerClientError(std::string(key)+" invalid");
    const auto number=value.at(key).get<std::int64_t>();
    if(number<0)throw WorkerClientError(std::string(key)+" invalid");
    return static_cast<std::size_t>(number);
}
std::string text_field(const json& value,const char* key){
    if(!value.contains(key)||!value.at(key).is_string())throw WorkerClientError(std::string(key)+" invalid");
    return value.at(key).get<std::string>();
}
double finite_field(const json& value,const char* key){
    if(!value.is_object()||!value.contains(key)||!value.at(key).is_number())
        throw WorkerClientError(std::string(key)+" invalid");
    const double result=value.at(key).get<double>();
    if(!std::isfinite(result))throw WorkerClientError(std::string(key)+" nonfinite");
    return result;
}
const json& object_field(const json& value,const char* key){
    if(!value.is_object()||!value.contains(key)||!value.at(key).is_object())
        throw WorkerClientError(std::string(key)+" invalid");
    return value.at(key);
}
double check_route(const json& route,const std::string& hash,const std::string& confidence){
    const auto id=text_field(route,"route_id");
    if(id.rfind(hash+":",0)!=0)throw WorkerClientError("route_id source mismatch");
    if(text_field(route,"result_label")!="patched_conic_screened_route")
        throw WorkerClientError("route result_label invalid");
    if(text_field(route,"snapshot_hash")!=hash)throw WorkerClientError("route snapshot_hash mismatch");
    if(text_field(route,"source_confidence")!=confidence)throw WorkerClientError("route source confidence mismatch");
    const auto& first=object_field(route,"home_mars");
    const auto& second=object_field(route,"mars_venus");
    const auto& third=object_field(route,"venus_home");
    const auto central=text_field(first,"central_body_id");
    const auto home=text_field(first,"departure_body_id"),mars=text_field(first,"arrival_body_id");
    const auto venus=text_field(second,"arrival_body_id");
    if(central!=text_field(second,"central_body_id")||central!=text_field(third,"central_body_id")||
       mars!=text_field(second,"departure_body_id")||venus!=text_field(third,"departure_body_id")||
       home!=text_field(third,"arrival_body_id")||
       std::set<std::string>{central,home,mars,venus}.size()!=4)
        throw WorkerClientError("route roles invalid");
    const double start1=finite_field(first,"departure_ut_s"),end1=finite_field(first,"arrival_ut_s");
    const double start2=finite_field(second,"departure_ut_s"),end2=finite_field(second,"arrival_ut_s");
    const double start3=finite_field(third,"departure_ut_s"),end3=finite_field(third,"arrival_ut_s");
    if(end1<=start1||end2<=start2||end3<=start3||start2!=end1+5184000.0||start3!=end2||
       finite_field(route,"fixed_stay_s")!=5184000.0)
        throw WorkerClientError("route dates or fixed stay invalid");
    const double a=finite_field(route,"home_injection_mps"),b=finite_field(route,"mars_capture_mps"),
                 c=finite_field(route,"mars_departure_mps"),d=finite_field(route,"home_return_capture_mps"),
                 total=finite_field(route,"total_optimistic_delta_v_mps");
    if(a<0||b<0||c<0||d<0||std::abs((a+b+c+d)-total)>1e-6)
        throw WorkerClientError("route burn total invalid");
    const double departure_vinf=finite_field(first,"departure_vinf_mps");
    const double return_vinf=finite_field(third,"arrival_vinf_mps");
    const double departure_c3=finite_field(route,"departure_c3_m2_s2");
    const double return_c3=finite_field(route,"return_c3_m2_s2");
    if(departure_vinf<0||return_vinf<0||departure_c3<0||return_c3<0||
       std::abs(departure_c3-departure_vinf*departure_vinf)>std::max(1e-6,departure_c3*1e-9)||
       std::abs(return_c3-return_vinf*return_vinf)>std::max(1e-6,return_c3*1e-9))
        throw WorkerClientError("route C3 convention invalid");
    const double periapsis=finite_field(route,"venus_minimum_periapsis_m");
    const double clearance=finite_field(route,"venus_clearance_radius_m");
    const double margin=finite_field(route,"venus_periapsis_margin_m");
    if(clearance<0||periapsis<clearance||margin<0||std::abs((periapsis-clearance)-margin)>1e-6)
        throw WorkerClientError("route flyby clearance invalid");
    return total;
}
void check_ranked_routes(const json& event,std::size_t cap,const std::string& hash,const std::string& confidence){
    if(!event.contains("ranked_routes")||!event.at("ranked_routes").is_array())
        throw WorkerClientError("ranked_routes missing");
    const auto& routes=event.at("ranked_routes");
    if(routes.size()>cap)throw WorkerClientError("ranked_routes exceeds cap");
    std::set<std::string> ids;double previous=-1;
    for(std::size_t i=0;i<routes.size();++i){
        const auto& route=routes[i];
        if(count(route,"rank")!=i+1)throw WorkerClientError("ranked_routes rank invalid");
        const auto total=check_route(route,hash,confidence);
        if(total<previous)throw WorkerClientError("ranked_routes objective regression");
        previous=total;
        if(!ids.insert(text_field(route,"route_id")).second)throw WorkerClientError("duplicate route_id");
    }
}
void check_ranked(const json& value,std::size_t max_candidates,const std::string& hash,const std::string& confidence){
    if(!value.contains("ranked_candidates")||!value.at("ranked_candidates").is_array())throw WorkerClientError("ranked order missing");
    const auto& rows=value.at("ranked_candidates");
    if(rows.size()>max_candidates)throw WorkerClientError("ranked order exceeds candidate cap");
    double previous=-std::numeric_limits<double>::infinity();
    for(std::size_t i=0;i<rows.size();++i){const auto& row=rows[i];
        if(!row.is_object()||count(row,"rank")!=i+1||text_field(row,"status")!="screened_seed"||
           text_field(row,"snapshot_hash")!=hash||text_field(row,"source_confidence")!=confidence||
           !row.contains("screening_score_mps")||!row.at("screening_score_mps").is_number())
            throw WorkerClientError("ranked order or provenance invalid");
        const double score=row.at("screening_score_mps").get<double>();
        if(!std::isfinite(score)||score<previous)throw WorkerClientError("ranked order objective regression");
        previous=score;
    }
}
double nonnegative_field(const json& value,const char* key){
    const double number=finite_field(value,key);
    if(number<0)throw WorkerClientError(std::string(key)+" negative");
    return number;
}
double vector_norm(const json& value,const char* key){
    if(!value.is_object()||!value.contains(key)||!value.at(key).is_array()||value.at(key).size()!=3)
        throw WorkerClientError(std::string(key)+" SI vector invalid");
    double squared=0;
    for(const auto& coordinate:value.at(key)){
        if(!coordinate.is_number())throw WorkerClientError(std::string(key)+" SI vector invalid");
        const double x=coordinate.get<double>();
        if(!std::isfinite(x))throw WorkerClientError(std::string(key)+" SI vector nonfinite");
        squared+=x*x;
    }
    if(!std::isfinite(squared))throw WorkerClientError(std::string(key)+" SI vector overflow");
    return std::sqrt(squared);
}
struct EvaluatedPass {std::array<double,4> burn_ut{};double burn_total=0;};
EvaluatedPass check_evaluation_pass(const json& value,const std::string& venus_id){
    if(!value.is_object())throw WorkerClientError("evaluation pass invalid");
    if(!value.contains("burns")||!value.contains("checkpoints"))
        throw WorkerClientError("evaluation burns or checkpoints missing");
    const auto& burns=value.at("burns"),&checkpoints=value.at("checkpoints");
    if(!burns.is_array()||burns.size()!=4||!checkpoints.is_array()||checkpoints.size()!=4)
        throw WorkerClientError("evaluation four burns and checkpoints required");
    EvaluatedPass pass;
    constexpr const char* names[]={"launch","Mars capture","Mars pre-departure","home return capture"};
    for(std::size_t i=0;i<4;++i){
        const auto& burn=burns.at(i),&checkpoint=checkpoints.at(i);
        const double ut=finite_field(burn,"ut_s"),magnitude=nonnegative_field(burn,"magnitude_mps");
        const double norm=vector_norm(burn,"delta_v_mps");
        if(i&&ut<=pass.burn_ut[i-1])throw WorkerClientError("evaluation burn order invalid");
        if(std::abs(norm-magnitude)>std::max(1e-8,magnitude*1e-9))
            throw WorkerClientError("evaluation burn magnitude mismatch");
        pass.burn_ut[i]=ut;pass.burn_total+=magnitude;
        if(text_field(checkpoint,"name")!=names[i]||finite_field(checkpoint,"ut_s")!=ut)
            throw WorkerClientError("evaluation checkpoint order or burn epoch invalid");
        for(const char* key:{"position_error_m","velocity_error_mps","parking_radius_error_m",
            "radial_velocity_mps","tangential_speed_error_mps"})nonnegative_field(checkpoint,key);
    }
    if(pass.burn_ut[2]-pass.burn_ut[1]!=5184000.0)
        throw WorkerClientError("evaluation Mars fixed stay invalid");
    const auto& venus=object_field(value,"venus");
    if(text_field(venus,"body_id")!=venus_id)throw WorkerClientError("evaluation Venus body ID mismatch");
    const double venus_ut=finite_field(venus,"ut_s"),distance=finite_field(venus,"distance_m");
    const double clearance=finite_field(venus,"clearance_m");
    const double safety=finite_field(venus,"safety_margin_m");
    if(venus_ut<=pass.burn_ut[2]||venus_ut>=pass.burn_ut[3]||distance<=0||clearance<=0||
       safety<=0||safety>clearance)
        throw WorkerClientError("evaluation Venus event or safety margin invalid");
    const auto& mars=object_field(value,"mars_radius");
    const double low=finite_field(mars,"observed_minimum_m"),high=finite_field(mars,"observed_maximum_m");
    const double interval_low=finite_field(mars,"model_interval_lower_m");
    const double interval_high=finite_field(mars,"model_interval_upper_m");
    const double low_ut=finite_field(mars,"observed_minimum_ut_s");
    const double high_ut=finite_field(mars,"observed_maximum_ut_s");
    if(low<=0||high<low||interval_low<=0||interval_low>low||interval_high<high||
       low_ut<pass.burn_ut[1]||low_ut>pass.burn_ut[2]||
       high_ut<pass.burn_ut[1]||high_ut>pass.burn_ut[2]||count(mars,"endpoint_count")!=2)
        throw WorkerClientError("evaluation Mars radius diagnostic invalid");
    (void)count(mars,"root_count");(void)count(value,"accepted_steps");(void)count(value,"rejected_steps");
    return pass;
}
void check_evaluation_complete(const json& event,const std::string& origin,const std::string& axes,
    const std::string& handedness,double epoch){
    if(event.contains("ranked_candidates")||event.contains("ranked_routes")||
       event.contains("candidate")||event.contains("route")||event.contains("refinement"))
        throw WorkerClientError("evaluation screened payload invalid for result kind");
    if(text_field(event,"result_label")!="independent_nbody_fixed_impulse_checkpointed_only"||
       text_field(event,"units")!="SI"||text_field(event,"force_model")!="newtonian_point_mass"||
       text_field(event,"frame_origin")!=origin||text_field(event,"frame_axes")!=axes||
       text_field(event,"frame_handedness")!=handedness||finite_field(event,"state_epoch_ut_s")!=epoch)
        throw WorkerClientError("evaluation frame, epoch, units, force model or label mismatch");
    if(!event.contains("route_seed_evidence_revalidated")||
       event.at("route_seed_evidence_revalidated")!=false||
       !event.contains("mars_stay_continuously_verified")||
       event.at("mars_stay_continuously_verified")!=false)
        throw WorkerClientError("evaluation evidence flags invalid");
    const auto& roles=object_field(event,"role_body_ids");
    for(const char* key:{"central","home","mars","venus"})
        if(!roles.contains(key)||!roles.at(key).is_string())
            throw WorkerClientError("evaluation role body IDs invalid");
    const auto central=text_field(roles,"central"),home=text_field(roles,"home");
    const auto mars=text_field(roles,"mars"),venus=text_field(roles,"venus");
    if(central.empty()||home.empty()||mars.empty()||venus.empty()||
       std::set<std::string>{central,home,mars,venus}.size()!=4)
        throw WorkerClientError("evaluation role body IDs invalid");
    const auto coarse=check_evaluation_pass(object_field(event,"coarse"),venus);
    const auto strict=check_evaluation_pass(object_field(event,"strict"),venus);
    if(coarse.burn_ut!=strict.burn_ut)throw WorkerClientError("evaluation coarse/strict burn epochs differ");
    const auto& coarse_burns=event.at("coarse").at("burns");
    const auto& strict_burns=event.at("strict").at("burns");
    for(std::size_t i=0;i<4;++i){
        for(std::size_t axis=0;axis<3;++axis){
            const double a=coarse_burns.at(i).at("delta_v_mps").at(axis).get<double>();
            const double b=strict_burns.at(i).at("delta_v_mps").at(axis).get<double>();
            if(std::abs(a-b)>std::max(1e-9,std::max(std::abs(a),std::abs(b))*1e-9))
                throw WorkerClientError("evaluation coarse/strict same impulse mismatch");
        }
        const double a=finite_field(coarse_burns.at(i),"magnitude_mps");
        const double b=finite_field(strict_burns.at(i),"magnitude_mps");
        if(std::abs(a-b)>std::max(1e-9,std::max(a,b)*1e-9))
            throw WorkerClientError("evaluation coarse/strict same impulse magnitude mismatch");
    }
    const double total=nonnegative_field(event,"total_charged_delta_v_mps");
    if(std::abs(total-coarse.burn_total)>std::max(1e-8,total*1e-9))
        throw WorkerClientError("evaluation charged burn total mismatch");
    const auto& disagreement=object_field(event,"disagreement");
    for(const char* key:{"maximum_checkpoint_position_m","maximum_checkpoint_velocity_mps",
        "venus_event_time_s","venus_radius_m","mars_minimum_radius_m","mars_maximum_radius_m",
        "mars_minimum_time_s","mars_maximum_time_s"})nonnegative_field(disagreement,key);
}
void reject_shooting_claims(const json& value){
    if(value.is_object())for(auto it=value.begin();it!=value.end();++it){
        static const std::set<std::string> forbidden={"feasible","optimized","global_optimum",
            "principia_matched","principia_equivalent","runtime_verified","installed_game_verified"};
        if(forbidden.contains(it.key()))throw WorkerClientError("unsupported shooting verification claim");
        reject_shooting_claims(it.value());
    }else if(value.is_array())for(const auto& item:value)reject_shooting_claims(item);
}
void shooting_keys(const json& value,std::initializer_list<const char*> keys){
    if(!value.is_object())throw WorkerClientError("shooting event object invalid");
    std::set<std::string> names(keys.begin(),keys.end());
    for(auto it=value.begin();it!=value.end();++it)
        if(!names.contains(it.key()))throw WorkerClientError("unsupported shooting event field: "+it.key());
}
void shooting_schema(const json& event,const std::string& type){
    if(type=="started")shooting_keys(event,{"protocol_version","request_id","type","snapshot_hash",
        "source_confidence","source_mode","frame_origin","frame_axes","frame_handedness",
        "state_epoch_ut_s","units","result_label","non_interruptible_stages"});
    else if(type=="progress")shooting_keys(event,{"protocol_version","request_id","type","snapshot_hash",
        "source_confidence","phase","completed_probes","total_probes","frame_origin","frame_axes",
        "frame_handedness","state_epoch_ut_s","units"});
    else if(type=="cancelled")shooting_keys(event,{"protocol_version","request_id","type","snapshot_hash",
        "source_confidence","status","completed_probes"});
    else if(type=="error")shooting_keys(event,{"protocol_version","request_id","type","snapshot_hash",
        "source_confidence","code","detail"});
    else if(type=="complete"){
        shooting_keys(event,{"protocol_version","request_id","type","snapshot_hash","source_confidence",
            "status","result_label","completed_probes","total_probes","iterations","frame_origin",
            "frame_axes","frame_handedness","state_epoch_ut_s","units","role_body_ids",
            "route_seed_evidence_revalidated","mars_stay_continuously_verified","final_trial",
            "coarse_diagnostics","strict"});
        shooting_keys(object_field(event,"role_body_ids"),{"central","home","mars","venus"});
        const auto& trial=object_field(event,"final_trial");
        shooting_keys(trial,{"launch_parking_state","impulses_mps","checkpoint_targets_relative"});
        const auto& probe=object_field(event,"coarse_diagnostics");
        shooting_keys(probe,{"checkpoints","burns","selected_venus","venus_event_count",
            "minimum_observed_venus_boundary_margin_m","mars_radius","total_charged_delta_v_mps"});
        if(!probe.contains("checkpoints")||!probe.at("checkpoints").is_array()||
           !probe.contains("burns")||!probe.at("burns").is_array()||
           !probe.contains("selected_venus"))throw WorkerClientError("shooting coarse arrays missing");
        for(const auto& point:probe.at("checkpoints"))shooting_keys(point,{"name","ut_s",
            "signed_position_residual_m","signed_velocity_residual_mps",
            "signed_parking_radius_residual_m","signed_radial_velocity_mps",
            "signed_tangential_speed_residual_mps"});
        for(const auto& burn:probe.at("burns"))shooting_keys(burn,{"ut_s","delta_v_mps","magnitude_mps"});
        shooting_keys(object_field(probe,"mars_radius"),{"observed_minimum_m","observed_minimum_ut_s",
            "observed_maximum_m","observed_maximum_ut_s","model_interval_lower_m","model_interval_upper_m"});
        if(!probe.at("selected_venus").is_null())
            shooting_keys(object_field(probe,"selected_venus"),{"body_id","ut_s","distance_m","clearance_m"});
        if(event.contains("strict")){
            const auto& strict=object_field(event,"strict");
            shooting_keys(strict,{"coarse","fine","total_charged_delta_v_mps","disagreement"});
            shooting_keys(object_field(strict,"disagreement"),{"maximum_checkpoint_position_m",
                "maximum_checkpoint_velocity_mps","venus_event_time_s","venus_radius_m",
                "mars_minimum_radius_m","mars_maximum_radius_m","mars_minimum_time_s","mars_maximum_time_s"});
            for(const char* key:{"coarse","fine"}){
                const auto& pass=object_field(strict,key);
                shooting_keys(pass,{"burns","checkpoints","accepted_steps","rejected_steps","venus","mars_radius"});
                if(!pass.contains("burns")||!pass.at("burns").is_array()||
                   !pass.contains("checkpoints")||!pass.at("checkpoints").is_array())
                    throw WorkerClientError("shooting strict arrays missing");
                for(const auto& burn:pass.at("burns"))shooting_keys(burn,{"ut_s","delta_v_mps","magnitude_mps"});
                for(const auto& point:pass.at("checkpoints"))shooting_keys(point,{"name","ut_s",
                    "position_error_m","velocity_error_mps","parking_radius_error_m","radial_velocity_mps",
                    "tangential_speed_error_mps"});
                shooting_keys(object_field(pass,"venus"),{"body_id","ut_s","distance_m","clearance_m"});
                shooting_keys(object_field(pass,"mars_radius"),{"observed_minimum_m","observed_minimum_ut_s",
                    "observed_maximum_m","observed_maximum_ut_s","model_interval_lower_m",
                    "model_interval_upper_m","endpoint_count","root_count"});
            }
        }
    }
}
bool close_scalar(double a,double b){return std::abs(a-b)<=std::max(1e-8,std::max(std::abs(a),std::abs(b))*1e-9);}
void check_state(const json& state){
    shooting_keys(state,{"position_m","velocity_mps"});
    (void)vector_norm(state,"position_m");(void)vector_norm(state,"velocity_mps");
}
void check_shooting_complete(const json& event,const std::string& origin,const std::string& axes,
    const std::string& handedness,double epoch,std::size_t cap,std::size_t previous){
    reject_shooting_claims(event);
    if(text_field(event,"frame_origin")!=origin||text_field(event,"frame_axes")!=axes||
       text_field(event,"frame_handedness")!=handedness||finite_field(event,"state_epoch_ut_s")!=epoch||
       text_field(event,"units")!="SI")throw WorkerClientError("shooting frame epoch or units mismatch");
    if(event.value("route_seed_evidence_revalidated",true)!=false||
       event.value("mars_stay_continuously_verified",true)!=false)
        throw WorkerClientError("shooting evidence flags invalid");
    const auto status=text_field(event,"status"),label=text_field(event,"result_label");
    static const std::set<std::string> diagnostics={"strict_rejected","budget_exhausted","missing_venus",
        "venus_boundary_or_radius","mars_stay_failed","coarse_constraints_failed","singular_jacobian",
        "unsafe_difference","line_search_failed","impulse_bound"};
    const bool accepted=status=="checkpointed_accepted";
    if(!accepted&&!diagnostics.contains(status))throw WorkerClientError("shooting completion status invalid");
    if(label!=(accepted?"independent_nbody_fixed_impulse_checkpointed_only":
        "independent_nbody_coarse_trial_diagnostic_only")||event.contains("strict")!=accepted)
        throw WorkerClientError("shooting status label or strict result invalid");
    if(count(event,"total_probes")!=cap||count(event,"completed_probes")!=previous||
       count(event,"iterations")>1000||previous==0)
        throw WorkerClientError("shooting completion probe count invalid");
    const auto& roles=object_field(event,"role_body_ids");std::set<std::string> ids;
    for(const char* key:{"central","home","mars","venus"})ids.insert(text_field(roles,key));
    if(ids.size()!=4||ids.contains(""))throw WorkerClientError("shooting role IDs invalid");
    const auto& trial=object_field(event,"final_trial");check_state(object_field(trial,"launch_parking_state"));
    if(!trial.contains("impulses_mps")||!trial.at("impulses_mps").is_array()||trial.at("impulses_mps").size()!=4||
       !trial.contains("checkpoint_targets_relative")||!trial.at("checkpoint_targets_relative").is_array()||
       trial.at("checkpoint_targets_relative").size()!=4)throw WorkerClientError("shooting four impulses or targets invalid");
    for(const auto& target:trial.at("checkpoint_targets_relative"))check_state(target);
    for(const auto& impulse:trial.at("impulses_mps")){
        json wrapped={{"vector",impulse}};(void)vector_norm(wrapped,"vector");
    }
    const auto& probe=object_field(event,"coarse_diagnostics");
    if(!probe.contains("burns")||!probe.at("burns").is_array()||probe.at("burns").size()!=4||
       !probe.contains("checkpoints")||!probe.at("checkpoints").is_array()||probe.at("checkpoints").size()!=4)
        throw WorkerClientError("shooting four coarse burns and checkpoints required");
    constexpr const char* names[]={"launch","Mars capture","Mars pre-departure","home return capture"};
    std::array<double,4> epochs{};double total=0;
    for(std::size_t i=0;i<4;++i){
        const auto& burn=probe.at("burns").at(i),&point=probe.at("checkpoints").at(i);
        epochs[i]=finite_field(burn,"ut_s");const auto magnitude=nonnegative_field(burn,"magnitude_mps");
        const auto norm=vector_norm(burn,"delta_v_mps");
        if(!close_scalar(magnitude,norm)||text_field(point,"name")!=names[i]||
           finite_field(point,"ut_s")!=epochs[i]||(i&&epochs[i]<=epochs[i-1]))
            throw WorkerClientError("shooting burn or checkpoint order invalid");
        for(std::size_t axis=0;axis<3;++axis){
            const double component=trial.at("impulses_mps").at(i).at(axis).get<double>();
            if(!std::isfinite(component)||!close_scalar(component,burn.at("delta_v_mps").at(axis).get<double>()))
                throw WorkerClientError("shooting final impulse differs from burn");
        }
        (void)vector_norm(point,"signed_position_residual_m");
        (void)vector_norm(point,"signed_velocity_residual_mps");
        for(const char* key:{"signed_parking_radius_residual_m","signed_radial_velocity_mps",
            "signed_tangential_speed_residual_mps"})finite_field(point,key);
        total+=magnitude;
    }
    if(epochs[2]-epochs[1]!=5184000.0||!close_scalar(total,nonnegative_field(probe,"total_charged_delta_v_mps")))
        throw WorkerClientError("shooting fixed stay or charged burn total invalid");
    const auto& radius=object_field(probe,"mars_radius");
    const double low=finite_field(radius,"observed_minimum_m"),high=finite_field(radius,"observed_maximum_m"),
        interval_low=finite_field(radius,"model_interval_lower_m"),interval_high=finite_field(radius,"model_interval_upper_m");
    if(low<=0||high<low||interval_low<=0||interval_low>low||interval_high<high||
       finite_field(radius,"observed_minimum_ut_s")<epochs[1]||
       finite_field(radius,"observed_maximum_ut_s")>epochs[2])
        throw WorkerClientError("shooting Mars radius interval invalid");
    (void)count(probe,"venus_event_count");
    if(!probe.contains("selected_venus")||!probe.contains("minimum_observed_venus_boundary_margin_m"))
        throw WorkerClientError("shooting Venus diagnostics missing");
    if(!probe.at("selected_venus").is_null()){
        const auto& venus=object_field(probe,"selected_venus");
        if(text_field(venus,"body_id")!=text_field(roles,"venus")||
           finite_field(venus,"ut_s")<=epochs[2]||finite_field(venus,"ut_s")>=epochs[3]||
           nonnegative_field(venus,"distance_m")<=0||!std::isfinite(finite_field(venus,"clearance_m")))
            throw WorkerClientError("shooting selected Venus event invalid");
    }
    if(!probe.at("minimum_observed_venus_boundary_margin_m").is_null())
        finite_field(probe,"minimum_observed_venus_boundary_margin_m");
    if(accepted){
        const auto& strict=object_field(event,"strict");
        const auto& coarse=object_field(strict,"coarse"),&fine=object_field(strict,"fine");
        for(const auto* pass:{&coarse,&fine}){
            json checked=*pass;
            checked["venus"]["safety_margin_m"]=finite_field(object_field(*pass,"venus"),"clearance_m");
            (void)check_evaluation_pass(checked,text_field(roles,"venus"));
            if(!pass->contains("burns")||!pass->at("burns").is_array()||pass->at("burns").size()!=4||
               !pass->contains("checkpoints")||!pass->at("checkpoints").is_array()||
               pass->at("checkpoints").size()!=4)
                throw WorkerClientError("shooting strict four burns and checkpoints required");
            const auto& burns=pass->at("burns"),&points=pass->at("checkpoints");
            for(std::size_t i=0;i<4;++i){
                if(finite_field(burns.at(i),"ut_s")!=epochs[i]||
                   !close_scalar(nonnegative_field(burns.at(i),"magnitude_mps"),
                         nonnegative_field(probe.at("burns").at(i),"magnitude_mps")))
                    throw WorkerClientError("shooting strict burn epochs or magnitudes differ");
                if(text_field(points.at(i),"name")!=names[i]||finite_field(points.at(i),"ut_s")!=epochs[i])
                    throw WorkerClientError("shooting strict checkpoint order invalid");
                for(const char* key:{"position_error_m","velocity_error_mps","parking_radius_error_m",
                    "radial_velocity_mps","tangential_speed_error_mps"})nonnegative_field(points.at(i),key);
                (void)vector_norm(burns.at(i),"delta_v_mps");
                for(std::size_t axis=0;axis<3;++axis)
                    if(!close_scalar(burns.at(i).at("delta_v_mps").at(axis).get<double>(),
                             trial.at("impulses_mps").at(i).at(axis).get<double>()))
                        throw WorkerClientError("shooting strict impulse mismatch");
            }
            const auto& venus=object_field(*pass,"venus");
            if(text_field(venus,"body_id")!=text_field(roles,"venus")||
               finite_field(venus,"clearance_m")<=0)throw WorkerClientError("shooting strict Venus clearance invalid");
        }
        if(!close_scalar(nonnegative_field(strict,"total_charged_delta_v_mps"),total))
            throw WorkerClientError("shooting strict charged burn total invalid");
        const auto& disagreement=object_field(strict,"disagreement");
        for(const char* key:{"maximum_checkpoint_position_m","maximum_checkpoint_velocity_mps",
            "venus_event_time_s","venus_radius_m","mars_minimum_radius_m","mars_maximum_radius_m",
            "mars_minimum_time_s","mars_maximum_time_s"})nonnegative_field(disagreement,key);
    }
}
#ifdef _WIN32
std::wstring wide(const std::string& utf8){
    if(utf8.empty())return {};
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),static_cast<int>(utf8.size()),nullptr,0);
    if(n<=0)throw std::runtime_error("invalid UTF-8 path or argument");
    std::wstring output(n,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),static_cast<int>(utf8.size()),output.data(),n);return output;
}
std::wstring quote(const std::wstring& arg){
    std::wstring out=L"\"";std::size_t slashes=0;
    for(wchar_t c:arg){if(c==L'\\'){++slashes;continue;}
        if(c==L'"'){out.append(slashes*2+1,L'\\');out.push_back(c);slashes=0;continue;}
        out.append(slashes,L'\\');slashes=0;out.push_back(c);}
    out.append(slashes*2,L'\\');out.push_back(L'"');return out;
}
struct Child {
    HANDLE in_write=nullptr,out_read=nullptr,err_read=nullptr,process=nullptr;
    ~Child(){if(process){if(WaitForSingleObject(process,0)!=WAIT_OBJECT_0)TerminateProcess(process,1);WaitForSingleObject(process,5000);CloseHandle(process);}
        for(HANDLE handle:{in_write,out_read,err_read})if(handle)CloseHandle(handle);}
    bool launch(const std::string& path,const std::vector<std::string>& args){
        if(!std::filesystem::exists(std::filesystem::path(wide(path))))return false;
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
        HANDLE child_in=nullptr,child_out=nullptr,child_err=nullptr;
        auto clean=[&](){for(HANDLE h:{child_in,child_out,child_err})if(h)CloseHandle(h);};
        if(!CreatePipe(&child_in,&in_write,&sa,0)||!CreatePipe(&out_read,&child_out,&sa,0)||
           !CreatePipe(&err_read,&child_err,&sa,0)){clean();return false;}
        SetHandleInformation(in_write,HANDLE_FLAG_INHERIT,0);SetHandleInformation(out_read,HANDLE_FLAG_INHERIT,0);
        SetHandleInformation(err_read,HANDLE_FLAG_INHERIT,0);
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
        startup.hStdInput=child_in;startup.hStdOutput=child_out;startup.hStdError=child_err;
        PROCESS_INFORMATION info{};const auto exe=wide(path);std::wstring command=quote(exe);
        for(const auto& arg:args)command+=L" "+quote(wide(arg));
        const BOOL okay=CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&info);
        clean();if(!okay)return false;CloseHandle(info.hThread);process=info.hProcess;return true;
    }
    bool write_line(const std::string& line){DWORD written=0;const auto bytes=line+"\n";
        return WriteFile(in_write,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size();}
    void read_available(const std::function<void(const char*,std::size_t,bool)>& receive){
        for(int which=0;which<2;++which){HANDLE h=which?err_read:out_read;DWORD available=0;
            while(PeekNamedPipe(h,nullptr,0,nullptr,&available,nullptr)&&available){char bytes[4096];DWORD got=0;
                if(!ReadFile(h,bytes,std::min<DWORD>(available,sizeof(bytes)),&got,nullptr)||!got)break;
                receive(bytes,got,which!=0);available=0;}
        }
    }
    bool exited(int& code){if(WaitForSingleObject(process,0)!=WAIT_OBJECT_0)return false;DWORD status=0;GetExitCodeProcess(process,&status);code=static_cast<int>(status);return true;}
    void terminate(){if(process)TerminateProcess(process,1);}
};
#else
struct Child {
    int in_write=-1,out_read=-1,err_read=-1;pid_t pid=-1;bool reaped=false;
    ~Child(){if(pid>0&&!reaped){kill(pid,SIGKILL);int status=0;while(waitpid(pid,&status,0)<0&&errno==EINTR){}}
        for(int fd:{in_write,out_read,err_read})if(fd>=0)close(fd);}
    bool launch(const std::string& path,const std::vector<std::string>& args){
        if(access(path.c_str(),X_OK)!=0)return false;
        int input[2]{-1,-1},output[2]{-1,-1},errors[2]{-1,-1};
        auto close_pipes=[&](){for(int fd:{input[0],input[1],output[0],output[1],errors[0],errors[1]})if(fd>=0)close(fd);};
        if(pipe(input)||pipe(output)||pipe(errors)){close_pipes();return false;}
        pid=fork();if(pid<0){close_pipes();return false;}
        if(pid==0){dup2(input[0],STDIN_FILENO);dup2(output[1],STDOUT_FILENO);dup2(errors[1],STDERR_FILENO);
            for(int fd:{input[0],input[1],output[0],output[1],errors[0],errors[1]})close(fd);
            std::vector<char*> argv;argv.push_back(const_cast<char*>(path.c_str()));
            for(const auto& arg:args)argv.push_back(const_cast<char*>(arg.c_str()));argv.push_back(nullptr);
            execv(path.c_str(),argv.data());_exit(127);}
        close(input[0]);close(output[1]);close(errors[1]);in_write=input[1];out_read=output[0];err_read=errors[0];
        fcntl(out_read,F_SETFL,fcntl(out_read,F_GETFL)|O_NONBLOCK);fcntl(err_read,F_SETFL,fcntl(err_read,F_GETFL)|O_NONBLOCK);
        return true;
    }
    bool write_line(const std::string& line){const auto bytes=line+"\n";std::size_t offset=0;
        while(offset<bytes.size()){const auto n=write(in_write,bytes.data()+offset,bytes.size()-offset);
            if(n<0&&errno==EINTR)continue;if(n<=0)return false;offset+=static_cast<std::size_t>(n);}return true;}
    void read_available(const std::function<void(const char*,std::size_t,bool)>& receive){
        for(int which=0;which<2;++which){const int fd=which?err_read:out_read;char bytes[4096];
            for(;;){const auto n=read(fd,bytes,sizeof(bytes));if(n>0){receive(bytes,static_cast<std::size_t>(n),which!=0);continue;}
                if(n<0&&errno==EINTR)continue;break;}}
    }
    bool exited(int& code){if(reaped)return true;int status=0;const auto result=waitpid(pid,&status,WNOHANG);
        if(result==0)return false;if(result<0){code=1;reaped=true;return true;}
        reaped=true;code=WIFEXITED(status)?WEXITSTATUS(status):128+(WIFSIGNALED(status)?WTERMSIG(status):0);return true;}
    void terminate(){if(pid>0&&!reaped)kill(pid,SIGKILL);}
};
#endif
}

WorkerEventValidator::WorkerEventValidator(std::string id,std::string hash,std::string confidence,std::size_t cap,
                                           WorkerResultKind result_kind)
    :request_id_(std::move(id)),snapshot_hash_(std::move(hash)),source_confidence_(std::move(confidence)),
     max_candidates_(cap),result_kind_(result_kind){}
json WorkerEventValidator::accept_line(const std::string& line){
    if(line.size()>line_limit)throw WorkerClientError("event line limit exceeded");
    json event=json::parse(line,nullptr,false);
    if(!event.is_object())throw WorkerClientError("event JSON object invalid");
    if(!event.contains("protocol_version")||!event.at("protocol_version").is_number_integer()||event.at("protocol_version")!=1)
        throw WorkerClientError("protocol_version mismatch");
    if(text_field(event,"request_id")!=request_id_)throw WorkerClientError("request_id mismatch");
    const auto type=text_field(event,"type");
    if(type!="started"&&type!="progress"&&type!="candidate"&&type!="refinement"&&type!="route"&&
       type!="complete"&&type!="cancelled"&&type!="error")
        throw WorkerClientError("event type unsupported");
    if((result_kind_==WorkerResultKind::screened_seed&&type=="route")||
       (result_kind_==WorkerResultKind::screened_route&&(type=="candidate"||type=="refinement"))||
       (result_kind_==WorkerResultKind::fixed_impulse_evaluation&&
        (type=="candidate"||type=="route"||type=="refinement"))||
       (result_kind_==WorkerResultKind::bounded_shooting&&
        (type=="candidate"||type=="route"||type=="refinement")))
        throw WorkerClientError("event type invalid for result kind");
    if(terminal_)throw WorkerClientError("event after terminal");
    const bool identity_present=event.contains("snapshot_hash")||event.contains("source_confidence");
    if(type!="error"||started_||identity_present){
        if(text_field(event,"snapshot_hash")!=snapshot_hash_)throw WorkerClientError("snapshot_hash mismatch");
        if(text_field(event,"source_confidence")!=source_confidence_)throw WorkerClientError("source confidence mismatch");
    }
    if(result_kind_==WorkerResultKind::bounded_shooting)shooting_schema(event,type);
    if(type=="started"){
        if(started_)throw WorkerClientError("duplicate started event");
        if(result_kind_==WorkerResultKind::bounded_shooting){
            reject_shooting_claims(event);
            if(source_confidence_!="runtime_observed_uncompared"||
               text_field(event,"source_mode")!="runtime_snapshot"||text_field(event,"units")!="SI"||
               text_field(event,"result_label")!="independent_nbody_coarse_trial_diagnostic_only")
                throw WorkerClientError("shooting source or diagnostic label invalid");
            frame_origin_=text_field(event,"frame_origin");frame_axes_=text_field(event,"frame_axes");
            frame_handedness_=text_field(event,"frame_handedness");state_epoch_ut_s_=finite_field(event,"state_epoch_ut_s");
            if(frame_origin_!="system_barycenter"||frame_axes_!="principia_alicesun_frozen_at_capture"||
               frame_handedness_!="right")throw WorkerClientError("shooting inertial frame invalid");
        }else if(result_kind_==WorkerResultKind::fixed_impulse_evaluation){
            if(source_confidence_!="runtime_observed_uncompared"||
               text_field(event,"source_mode")!="runtime_snapshot"||text_field(event,"units")!="SI"||
               text_field(event,"result_label")!="independent_nbody_fixed_impulse_checkpointed_only"||
               event.contains("total_cells"))
                throw WorkerClientError("evaluation source, units or label invalid");
            frame_origin_=text_field(event,"frame_origin");frame_axes_=text_field(event,"frame_axes");
            frame_handedness_=text_field(event,"frame_handedness");
            state_epoch_ut_s_=finite_field(event,"state_epoch_ut_s");
            if(frame_origin_!="system_barycenter"||frame_axes_!="principia_alicesun_frozen_at_capture"||
               frame_handedness_!="right")throw WorkerClientError("evaluation inertial frame invalid");
        }else{
            total_cells_=count(event,"total_cells");
            if(total_cells_==0||total_cells_>(result_kind_==WorkerResultKind::screened_route?100000:1000000))
                throw WorkerClientError("total_cells invalid");
        }
        started_=true;
    }else if(type!="error"&&!(result_kind_==WorkerResultKind::fixed_impulse_evaluation&&type=="cancelled")&&
             !(result_kind_==WorkerResultKind::bounded_shooting&&type=="cancelled")&&
             !started_)throw WorkerClientError("event before started");
    if(type=="progress"){
        if(result_kind_==WorkerResultKind::bounded_shooting){
            reject_shooting_claims(event);
            const auto completed=count(event,"completed_probes"),total=count(event,"total_probes");
            if(text_field(event,"phase")!="coarse_probes"||total==0||total>10000||
               (shooting_total_&&total!=shooting_total_)||completed==0||
               completed<last_completed_probes_||completed>total||++shooting_progress_events_>128||
               text_field(event,"frame_origin")!=frame_origin_||
               text_field(event,"frame_axes")!=frame_axes_||
               text_field(event,"frame_handedness")!=frame_handedness_||
               finite_field(event,"state_epoch_ut_s")!=state_epoch_ut_s_||
               text_field(event,"units")!="SI")
                throw WorkerClientError("shooting progress regression, frame or cap invalid");
            shooting_total_=total;last_completed_probes_=completed;
        }else if(result_kind_==WorkerResultKind::fixed_impulse_evaluation){
            const auto completed=count(event,"completed_phases"),total=count(event,"total_phases");
            if(text_field(event,"phase")!="fixed_impulse_evaluation"||total!=1||
               completed<last_completed_phases_||completed>1||
               (!saw_evaluation_phase_zero_&&completed!=0)||
               event.contains("sampled_cells")||event.contains("total_cells"))
                throw WorkerClientError("evaluation progress regression or bound");
            if(completed==0)saw_evaluation_phase_zero_=true;
            last_completed_phases_=completed;
        }else{
            const auto sampled=count(event,"sampled_cells"),total=count(event,"total_cells");
            if(total!=total_cells_||sampled<last_sampled_||sampled>total_cells_)
                throw WorkerClientError("progress regression or bound");
            last_sampled_=sampled;
        }
    }
    if(type=="candidate"&&text_field(event,"status")!="screened_seed")throw WorkerClientError("candidate status invalid");
    if(type=="route")check_route(event,snapshot_hash_,source_confidence_);
    if(type=="complete"||type=="cancelled"){
        if(result_kind_==WorkerResultKind::bounded_shooting){
            reject_shooting_claims(event);
            if(type=="cancelled"){
                const auto status=text_field(event,"status");
                if((status!="before_numerical_work"&&status!="before_coarse_probe"&&
                    status!="during_numerical_work")||
                   (!started_&&status!="before_numerical_work"))
                    throw WorkerClientError("shooting cancellation status invalid");
                if(started_&&(!event.contains("completed_probes")||
                    count(event,"completed_probes")<last_completed_probes_||
                    count(event,"completed_probes")>shooting_total_))
                    throw WorkerClientError("shooting cancelled count outside progress and cap");
            }else{
                if(!started_||!shooting_progress_events_)throw WorkerClientError("shooting completed without progress");
                check_shooting_complete(event,frame_origin_,frame_axes_,frame_handedness_,
                    state_epoch_ut_s_,shooting_total_,last_completed_probes_);
            }
        }else if(result_kind_==WorkerResultKind::fixed_impulse_evaluation){
            if(type=="cancelled"){
                const auto status=text_field(event,"status");
                if((status!="before_numerical_work"&&status!="after_numerical_work")||
                   (!started_&&status!="before_numerical_work"))
                    throw WorkerClientError("evaluation cancellation status invalid");
            }
            if(type=="complete"){
                check_evaluation_complete(event,frame_origin_,frame_axes_,frame_handedness_,state_epoch_ut_s_);
                if(!saw_evaluation_phase_zero_||last_completed_phases_!=1)
                    throw WorkerClientError("evaluation progress incomplete before complete");
            }
        }else if(result_kind_==WorkerResultKind::screened_route)
            check_ranked_routes(event,max_candidates_,snapshot_hash_,source_confidence_);
        else check_ranked(event,max_candidates_,snapshot_hash_,source_confidence_);
    }
    if(type=="complete"||type=="cancelled"||type=="error")terminal_=true;
    return event;
}

WorkerClient::~WorkerClient(){shutdown_.store(true);cancel_requested_.store(true);if(thread_.joinable())thread_.join();}
bool WorkerClient::start(ClientOptions options){
    if(active_.load())return false;
    if(thread_.joinable())thread_.join();
    {std::lock_guard lock(queue_mutex_);queue_.clear();}
    shutdown_.store(false);cancel_requested_.store(false);active_.store(true);
    thread_=std::thread([this,options=std::move(options)]() mutable {
        run(std::move(options));
        // Keep running() true until run's child handles and watchdogs have been destroyed.
        active_.store(false);
    });return true;
}
void WorkerClient::cancel() noexcept {cancel_requested_.store(true);}
bool WorkerClient::wait_for(std::chrono::milliseconds duration){
    const auto until=std::chrono::steady_clock::now()+duration;
    while(active_.load()&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if(active_.load())return false;if(thread_.joinable())thread_.join();return true;
}
std::vector<json> WorkerClient::drain(){std::lock_guard lock(queue_mutex_);std::vector<json> result;
    result.reserve(queue_.size());while(!queue_.empty()){result.push_back(std::move(queue_.front()));queue_.pop_front();}return result;}
bool WorkerClient::push(json value,std::size_t max_count){std::lock_guard lock(queue_mutex_);
    if(queue_.size()>=max_count)return false;queue_.push_back(std::move(value));return true;}
void WorkerClient::failure(const std::string& code,const std::string& detail,std::size_t max_count){
    json event={{"type","client_error"},{"code",code},{"detail",detail.substr(0,stderr_limit)}};
    std::lock_guard lock(queue_mutex_);if(queue_.size()>=max_count&&!queue_.empty())queue_.pop_front();queue_.push_back(std::move(event));
}
void WorkerClient::run(ClientOptions options) noexcept {
    const auto cap=std::max<std::size_t>(1,options.max_retained_events);
    std::string stderr_text;
    try{
#ifndef _WIN32
        sigset_t blocked;sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);pthread_sigmask(SIG_BLOCK,&blocked,nullptr);
#endif
        Child child;
        if(options.executable_path.empty()||!child.launch(options.executable_path,options.arguments)){
            failure("spawn_failed","worker executable missing or could not start",cap);return;}
        const auto begun=std::chrono::steady_clock::now();
        std::atomic<int> write_stop_reason{0}; // 1 owner close, 2 cancel, 3 deadline
        std::jthread write_watchdog([&](std::stop_token stop){
            while(!stop.stop_requested()){
                int reason=0;
                if(shutdown_.load())reason=1;
                else if(cancel_requested_.load())reason=2;
                else if(std::chrono::steady_clock::now()-begun>=options.timeout)reason=3;
                if(reason){write_stop_reason.store(reason);child.terminate();return;}
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
        const bool written=options.request_line.size()<=line_limit&&child.write_line(options.request_line);
        write_watchdog.request_stop();write_watchdog.join();
        if(write_stop_reason.load()==1)return;
        if(write_stop_reason.load()==2){failure("cancelled","request write cancelled before worker accepted it",cap);return;}
        if(write_stop_reason.load()==3){failure("timeout","request write exceeded deadline",cap);return;}
        if(!written){
            failure("request_write_failed","worker request could not be written",cap);return;}
        WorkerEventValidator validator(options.request_id,options.expected_snapshot_hash,options.expected_source_confidence,
                                       options.max_candidates,options.result_kind);
        std::string partial;
        std::chrono::steady_clock::time_point cancel_at{};bool cancel_sent=false;
        auto receive=[&](const char* bytes,std::size_t size,bool from_stderr){
            if(from_stderr){if(stderr_text.size()<stderr_limit)stderr_text.append(bytes,std::min(size,stderr_limit-stderr_text.size()));return;}
            for(std::size_t i=0;i<size;++i){if(bytes[i]=='\n'){
                    if(!partial.empty()&&partial.back()=='\r')partial.pop_back();
                    auto event=validator.accept_line(partial);partial.clear();
                    if(!push(std::move(event),cap))throw WorkerClientError("event queue limit exceeded");
                }else{partial.push_back(bytes[i]);if(partial.size()>line_limit)throw WorkerClientError("event line limit exceeded");}}
        };
        for(;;){
            child.read_available(receive);
            int exit_code=0;
            if(child.exited(exit_code)){
                child.read_available(receive);
                if(!partial.empty())throw WorkerClientError("unterminated event JSON line");
                if(exit_code!=0)failure("nonzero_exit","worker exit status "+std::to_string(exit_code)+"; stderr: "+stderr_text,cap);
                else if(!validator.terminal())failure("premature_eof","worker ended before terminal event; stderr: "+stderr_text,cap);
                return;
            }
            const auto now=std::chrono::steady_clock::now();
            if(shutdown_.load()){child.terminate();return;}
            if(cancel_requested_.load()&&!cancel_sent){
                const json control={{"protocol_version",1},{"command","cancel"},{"request_id",options.request_id}};
                std::atomic<bool> control_timed_out{false};
                std::jthread cancel_watchdog([&](std::stop_token stop){
                    while(!stop.stop_requested()){
                        if(shutdown_.load()||std::chrono::steady_clock::now()-now>options.cancel_grace){
                            control_timed_out.store(true);
                            child.terminate();return;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    }
                });
                const bool control_written=child.write_line(control.dump());
                cancel_watchdog.request_stop();cancel_watchdog.join();
                if(shutdown_.load())return;
                if(control_timed_out.load()){
                    failure("cancel_timeout","worker did not accept cancellation before deadline",cap);
                    return;}
                if(!control_written){failure("cancel_write_failed","worker cancellation control could not be written",cap);
                    return;}
                cancel_sent=true;cancel_at=now;
            }
            if(cancel_sent&&now-cancel_at>options.cancel_grace){
                child.terminate();failure("cancel_timeout","worker ignored cancellation; stderr: "+stderr_text,cap);return;}
            if(now-begun>options.timeout){child.terminate();failure("timeout","worker exceeded deadline; stderr: "+stderr_text,cap);return;}
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }catch(const WorkerClientError& error){failure("invalid_event",std::string(error.what())+"; stderr: "+stderr_text,cap);}
    catch(const std::exception& error){failure("io_failed",std::string(error.what())+"; stderr: "+stderr_text,cap);}
}
}
