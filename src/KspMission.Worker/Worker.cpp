#include "Worker.hpp"
#include "SearchGrid.hpp"
#include "Refine.hpp"
#include "RuntimeReader.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace ksp {
namespace {
using json=nlohmann::json;
constexpr const char* fixture_hash="synthetic-worker-v1";
Snapshot fixture(){
    constexpr double mu=3.986004418e14,r=1e7,R=1.2e7,t=2500;
    const double wa=std::sqrt(mu/(r*r*r)),wb=std::sqrt(mu/(R*R*R)),phase=std::acos(-1.0)/2-wb*t;
    Snapshot s;s.analysis_ready=true;s.confidence="synthetic_fixture";s.snapshot_hash=fixture_hash;
    s.state_epoch_ut_s=0;s.frame={"barycenter","X,Y,Z","right",true};
    s.bodies={{"star",mu,1e6,State{{0,0,0},{0,0,0}},0},
        {"home",1,0.01,State{{r,0,0},{0,r*wa,0}},0},
        {"target",1,0.01,State{{R*std::cos(phase),R*std::sin(phase),0},{-R*wb*std::sin(phase),R*wb*std::cos(phase),0}},0}};
    return s;
}
Vec3 vector3(const json& j){if(!j.is_array()||j.size()!=3)throw std::invalid_argument("vector must have three components");return {j.at(0).get<double>(),j.at(1).get<double>(),j.at(2).get<double>()};}
json vector_json(Vec3 v){return json::array({v.x,v.y,v.z});}
GridRequest grid_request(const json& j){
    GridRequest q;
    q.central_body_id=j.at("central_body_id").get<std::string>();q.departure_body_id=j.at("departure_body_id").get<std::string>();q.arrival_body_id=j.at("arrival_body_id").get<std::string>();
    q.launch_start_ut_s=j.at("launch_start_ut_s").get<double>();q.launch_end_ut_s=j.at("launch_end_ut_s").get<double>();q.launch_step_s=j.at("launch_step_s").get<double>();
    q.flight_min_s=j.at("flight_min_s").get<double>();q.flight_max_s=j.at("flight_max_s").get<double>();q.flight_step_s=j.at("flight_step_s").get<double>();
    q.reference_normal=vector3(j.at("reference_normal"));
    const auto branch=j.at("branch").get<std::string>(),direction=j.at("direction").get<std::string>();
    if(branch!="short"&&branch!="long")throw std::invalid_argument("branch");
    if(direction!="positive"&&direction!="negative")throw std::invalid_argument("direction");
    q.branch=branch=="short"?TransferBranch::short_path:TransferBranch::long_path;
    q.direction=direction=="positive"?AngularMomentumDirection::positive:AngularMomentumDirection::negative;
    q.max_position_residual_m=j.at("max_position_residual_m").get<double>();q.max_velocity_residual_mps=j.at("max_velocity_residual_mps").get<double>();
    q.max_candidates=j.at("max_candidates").get<std::size_t>();q.stable_seed=j.value("stable_seed",std::uint64_t{0});
    q.max_grid_cells=j.value("max_grid_cells",std::size_t{100000});
    if(j.contains("central_atmosphere_altitude_m"))q.central_atmosphere_altitude_m=j.at("central_atmosphere_altitude_m").get<double>();
    q.central_safety_margin_m=j.value("central_safety_margin_m",0.0);
    return q;
}
std::size_t cells(double a,double b,double h){
    if(!std::isfinite(a)||!std::isfinite(b)||!std::isfinite(h)||h<=0||b<a)throw std::invalid_argument("invalid grid interval");
    const double quotient=(b-a)/h;
    if(!std::isfinite(quotient)||quotient>100000||std::abs(quotient-std::round(quotient))>1e-9)throw std::invalid_argument("grid interval must contain integer steps");
    return static_cast<std::size_t>(std::round(quotient))+1;
}
bool better(const DatedLegCandidate& a,const DatedLegCandidate& b){
    if(a.screening_score_mps!=b.screening_score_mps)return a.screening_score_mps<b.screening_score_mps;
    if(a.departure_ut_s!=b.departure_ut_s)return a.departure_ut_s<b.departure_ut_s;
    if(a.flight_time_s!=b.flight_time_s)return a.flight_time_s<b.flight_time_s;
    return a.tie_key<b.tie_key;
}
json candidate_json(const GridRequest& q,const DatedLegCandidate& a){return {{"type","candidate"},{"candidate_id",worker_candidate_id(q,a)},{"status","screened_seed"},
    {"snapshot_hash",a.snapshot_hash},{"source_confidence",a.source_confidence},
    {"departure_ut_s",a.departure_ut_s},{"arrival_ut_s",a.arrival_ut_s},{"flight_time_s",a.flight_time_s},
    {"departure_velocity_mps",vector_json(a.departure_barycentric_velocity_mps)},
    {"arrival_velocity_mps",vector_json(a.arrival_barycentric_velocity_mps)},
    {"departure_vinf_mps",a.departure_vinf_mps},{"arrival_vinf_mps",a.arrival_vinf_mps},
    {"screening_score_mps",a.screening_score_mps},{"lambert_position_residual_m",a.lambert.position_residual_m},
    {"lambert_velocity_residual_mps",a.lambert.velocity_residual_mps}};}
json ranked_json(const GridRequest& q,std::vector<DatedLegCandidate> candidates){
    std::sort(candidates.begin(),candidates.end(),better);
    if(candidates.size()>q.max_candidates)candidates.resize(q.max_candidates);
    json ranked=json::array();
    for(std::size_t i=0;i<candidates.size();++i){auto row=candidate_json(q,candidates[i]);row.erase("type");row["rank"]=i+1;ranked.push_back(std::move(row));}
    return ranked;
}
bool lower_sha256(const std::string& hash){
    return hash.size()==64&&std::all_of(hash.begin(),hash.end(),[](unsigned char c){return std::isdigit(c)||(c>='a'&&c<='f');});
}
}

std::string worker_candidate_id(const GridRequest& q,const DatedLegCandidate& a){
    const auto launch=static_cast<std::uint64_t>(std::llround((a.departure_ut_s-q.launch_start_ut_s)/q.launch_step_s));
    const auto flight=static_cast<std::uint64_t>(std::llround((a.flight_time_s-q.flight_min_s)/q.flight_step_s));
    const auto& m=a.ephemeris_metadata;
    const json key={{"snapshot_hash",a.snapshot_hash},{"central_body_id",q.central_body_id},
        {"departure_body_id",q.departure_body_id},{"arrival_body_id",q.arrival_body_id},
        {"branch",q.branch==TransferBranch::short_path?"short":"long"},
        {"direction",q.direction==AngularMomentumDirection::positive?"positive":"negative"},
        {"launch_start_ut_s",q.launch_start_ut_s},{"launch_end_ut_s",q.launch_end_ut_s},{"launch_step_s",q.launch_step_s},
        {"flight_min_s",q.flight_min_s},{"flight_max_s",q.flight_max_s},{"flight_step_s",q.flight_step_s},
        {"reference_normal",vector_json(q.reference_normal)},
        {"max_position_residual_m",q.max_position_residual_m},{"max_velocity_residual_mps",q.max_velocity_residual_mps},
        {"central_atmosphere_altitude_m",q.central_atmosphere_altitude_m?json(*q.central_atmosphere_altitude_m):json(nullptr)},
        {"central_safety_margin_m",q.central_safety_margin_m},{"max_candidates",q.max_candidates},
        {"max_grid_cells",q.max_grid_cells},{"stable_seed",q.stable_seed},
        {"ephemeris_start_ut_s",m.start_ut_s},{"ephemeris_end_ut_s",m.end_ut_s},{"ephemeris_step_s",m.step_s},
        {"ephemeris_fit_position_m",m.max_position_fit_error_m},{"ephemeris_fit_velocity_mps",m.max_velocity_fit_error_mps},
        {"ephemeris_integrator",m.integrator},{"ephemeris_integrator_version",m.integrator_version},
        {"ephemeris_force_model",m.force_model},{"ephemeris_result_label",m.result_label}};
    return a.snapshot_hash+":"+sha256_hex(key.dump())+":"+std::to_string(launch)+":"+std::to_string(flight);
}

bool is_cancel_line(const std::string& line,const std::string& request_id){
    const json message=json::parse(line,nullptr,false);
    return message.is_object()&&message.contains("protocol_version")&&message["protocol_version"].is_number_integer()&&message["protocol_version"]==1&&
        message.contains("command")&&message["command"].is_string()&&message["command"]=="cancel"&&
        message.contains("request_id")&&message["request_id"].is_string()&&message["request_id"]==request_id;
}

void process_start_line(const std::string& line,const WorkerEmit& emit,const WorkerCancel& cancelled,
                        const WorkerAfterRead& after_runtime_bytes_read_for_test){
    json input=json::parse(line,nullptr,false);std::string id,source_hash,source_confidence;
    auto send=[&](json event){event["protocol_version"]=1;event["request_id"]=id;
        if(!source_hash.empty()){event["snapshot_hash"]=source_hash;event["source_confidence"]=source_confidence;}
        emit(event);};
    auto error=[&](const char* code,const std::string& detail){send({{"type","error"},{"code",code},{"detail",detail}});};
    if(input.is_discarded()){error("invalid_json","malformed JSON line");return;}
    if(!input.is_object()){error("invalid_request","request must be an object");return;}
    if(input.contains("request_id")&&input["request_id"].is_string())id=input["request_id"].get<std::string>();
    if(!input.contains("protocol_version")||!input["protocol_version"].is_number_integer()||input["protocol_version"]!=1){error("unsupported_version","protocol_version must be 1");return;}
    if(!input.contains("command")||input["command"]!="start"||id.empty()){error("invalid_request","start command and nonempty request_id required");return;}
    bool work_started=false;
    try{
        const auto& source=input.at("source");
        const auto mode=source.at("mode").get<std::string>();
        Snapshot snapshot;Settings settings;std::optional<RuntimeLoad> runtime;
        if(mode=="synthetic_fixture"){
            if(source.at("expected_snapshot_hash")!=fixture_hash){error("source_mismatch","expected_snapshot_hash differs from fixture");return;}
            snapshot=fixture();source_hash=snapshot.snapshot_hash;source_confidence=snapshot.confidence;
            settings.start_ut_s=0;settings.end_ut_s=5000;settings.step_s=1;
            settings.max_position_fit_error_m=10;settings.max_velocity_fit_error_mps=0.003;
        }else if(mode=="runtime_snapshot"){
            const auto path=source.at("path").get<std::string>();
            const auto expected=source.at("expected_snapshot_hash").get<std::string>();
            if(path.empty()||!lower_sha256(expected))throw std::invalid_argument("runtime path and 64 lowercase SHA-256 required");
            std::ifstream file(path,std::ios::binary|std::ios::ate);
            if(!file){error("source_read_failed","cannot open runtime snapshot");return;}
            const auto length=file.tellg();
            if(length<0){error("source_read_failed","cannot size runtime snapshot");return;}
            constexpr std::streamoff max_bytes=16*1024*1024;
            if(length>max_bytes){error("source_too_large","runtime snapshot exceeds 16 MiB");return;}
            std::string bytes(static_cast<std::size_t>(length),'\0');file.seekg(0);
            if(!file.read(bytes.data(),length)){error("source_read_failed","cannot read complete runtime snapshot");return;}
            if(after_runtime_bytes_read_for_test)after_runtime_bytes_read_for_test();
            file.clear();file.seekg(0,std::ios::end);
            if(!file||file.tellg()!=length){error("source_changed","runtime snapshot size changed during read");return;}
            if(sha256_hex(bytes)!=expected){error("source_mismatch","source hash mismatch for exact snapshot bytes");return;}
            try{runtime=read_runtime_snapshot(bytes,expected);}catch(const RuntimeReaderError& issue){error("invalid_source",issue.what());return;}
            file.clear();file.seekg(0,std::ios::end);
            if(!file||file.tellg()!=length){error("source_changed","runtime snapshot size changed before source acceptance");return;}
            snapshot=runtime->snapshot;source_hash=snapshot.snapshot_hash;source_confidence=snapshot.confidence;
            const auto& ep=input.at("ephemeris");
            settings.start_ut_s=runtime->capture_ut_s;
            settings.end_ut_s=ep.at("end_ut_s").get<double>();settings.step_s=ep.at("step_s").get<double>();
            settings.max_position_fit_error_m=ep.at("max_position_fit_error_m").get<double>();
            settings.max_velocity_fit_error_mps=ep.at("max_velocity_fit_error_mps").get<double>();
            const double steps=(settings.end_ut_s-settings.start_ut_s)/settings.step_s;
            if(!std::isfinite(settings.end_ut_s)||!std::isfinite(settings.step_s)||settings.step_s<=0||
               !std::isfinite(settings.max_position_fit_error_m)||settings.max_position_fit_error_m<=0||
               !std::isfinite(settings.max_velocity_fit_error_mps)||settings.max_velocity_fit_error_mps<=0||
               settings.end_ut_s<=settings.start_ut_s||!std::isfinite(steps)||steps>100000||steps<1||
               std::abs(steps-std::round(steps))>1e-9)throw std::invalid_argument("runtime ephemeris settings or step limit invalid");
            settings.max_steps=100000;
        }else throw std::invalid_argument("unsupported source mode");
        auto q=grid_request(input.at("grid"));
        const auto nlaunch=cells(q.launch_start_ut_s,q.launch_end_ut_s,q.launch_step_s),nflight=cells(q.flight_min_s,q.flight_max_s,q.flight_step_s);
        if(nlaunch>100000/nflight||nlaunch*nflight>q.max_grid_cells||q.max_grid_cells>1000000)throw std::invalid_argument("grid cell limit");
        if(q.launch_start_ut_s<settings.start_ut_s||q.launch_end_ut_s+q.flight_max_s>settings.end_ut_s||q.flight_min_s<=0)
            throw std::invalid_argument("ephemeris coverage");
        if(runtime){
            auto found=std::find_if(runtime->atmosphere_boundaries.begin(),runtime->atmosphere_boundaries.end(),
                [&](const AtmosphereBoundary& b){return b.body_id==q.central_body_id;});
            if(found==runtime->atmosphere_boundaries.end())throw std::invalid_argument("central atmosphere missing from runtime snapshot");
            if(q.central_atmosphere_altitude_m&&*q.central_atmosphere_altitude_m!=found->altitude_m)
                throw std::invalid_argument("central atmosphere disagrees with runtime snapshot");
            q.central_atmosphere_altitude_m=found->altitude_m;
            for(const auto& body_id:{q.central_body_id,q.departure_body_id,q.arrival_body_id})
                if(std::none_of(snapshot.bodies.begin(),snapshot.bodies.end(),[&](const Body& b){return b.id==body_id;}))
                    throw std::invalid_argument("unknown runtime body ID: "+body_id);
        }
        if(!q.max_candidates||!q.central_atmosphere_altitude_m)throw std::invalid_argument("candidate cap and central atmosphere required");
        bool refine_enabled=false;
        if(input.contains("refine"))refine_enabled=input.at("refine").at("enabled").get<bool>();
        if(runtime&&refine_enabled)throw std::invalid_argument("runtime refinement is not enabled in this screening slice");
        send({{"type","started"},{"total_cells",nlaunch*nflight},{"frame_origin",snapshot.frame.origin},{"frame_axes",snapshot.frame.axes},
            {"frame_handedness",snapshot.frame.handedness},{"frame_inertial",snapshot.frame.inertial},
            {"coverage_start_ut_s",settings.start_ut_s},{"coverage_end_ut_s",settings.end_ut_s},
            {"force_model","newtonian_point_mass"},{"source_mode",mode},
            {"units","SI"},{"result_label",settings.result_label},{"cancellation_guarantee","checkpoint_only"},
            {"non_interruptible_stages",json::array({"ephemeris_integration","individual_grid_cell","terminal_position_refinement"})}});
        work_started=true;
        std::vector<DatedLegCandidate> retained;std::size_t sampled=0,rejected=0,emitted=0;
        auto cancel_event=[&](){send({{"type","cancelled"},{"sampled_cells",sampled},{"retained_candidates",retained.size()},
            {"ranked_candidates",ranked_json(q,retained)}});};
        if(cancelled()){cancel_event();return;}
        send({{"type","progress"},{"phase","ephemeris_precompute"},{"sampled_cells",0},{"total_cells",nlaunch*nflight}});
        if(cancelled()){cancel_event();return;}
        const auto eph=integrate(snapshot,settings);
        if(cancelled()){cancel_event();return;}
        // Validate remaining search-specific options against the computed ephemeris.
        auto preflight=q;preflight.launch_end_ut_s=preflight.launch_start_ut_s;preflight.flight_max_s=preflight.flight_min_s;
        (void)screen_single_leg(snapshot,eph,preflight);
        if(cancelled()){cancel_event();return;}
        for(std::size_t i=0;i<nlaunch;++i)for(std::size_t j=0;j<nflight;++j){
            if(cancelled()){cancel_event();return;}
            auto cell=q;cell.launch_start_ut_s=cell.launch_end_ut_s=q.launch_start_ut_s+i*q.launch_step_s;
            cell.flight_min_s=cell.flight_max_s=q.flight_min_s+j*q.flight_step_s;
            auto result=screen_single_leg(snapshot,eph,cell);++sampled;
            if(result.candidates.empty())++rejected;
            for(const auto& candidate:result.candidates){retained.push_back(candidate);if(emitted<q.max_candidates){send(candidate_json(q,candidate));++emitted;}}
            if(sampled==nlaunch*nflight||sampled==1||sampled%(std::max<std::size_t>(1,nlaunch*nflight/20))==0)
                send({{"type","progress"},{"sampled_cells",sampled},{"total_cells",nlaunch*nflight},{"rejected_cells",rejected},{"retained_candidates",retained.size()}});
        }
        std::sort(retained.begin(),retained.end(),better);if(retained.size()>q.max_candidates)retained.resize(q.max_candidates);
        if(cancelled()){cancel_event();return;}
        if(refine_enabled&&!retained.empty()){
            const auto& best=retained.front();
            try{
                const State departing=eph.query(q.departure_body_id,best.departure_ut_s);
                const State arriving=eph.query(q.arrival_body_id,best.arrival_ut_s);
                RefineRequest r;r.initial_state=departing;
                const Vec3 departure_vinf=best.departure_barycentric_velocity_mps-departing.velocity_mps;
                const Vec3 arrival_vinf=best.arrival_barycentric_velocity_mps-arriving.velocity_mps;
                r.initial_state.position_m=r.initial_state.position_m+departure_vinf*(1000/norm(departure_vinf));
                r.target_position_m=arriving.position_m-arrival_vinf*(1000/norm(arrival_vinf));
                r.seed_impulse_mps=best.departure_barycentric_velocity_mps-departing.velocity_mps;
                r.propagation.start_ut_s=best.departure_ut_s;r.propagation.end_ut_s=best.arrival_ut_s;
                r.propagation.abs_position_tolerance_m=1;r.propagation.abs_velocity_tolerance_mps=1e-4;r.propagation.relative_tolerance=1e-10;
                r.propagation.min_step_s=1e-5;r.propagation.max_step_s=30;r.propagation.safety_margin_m=0;
                for(const auto& body:snapshot.bodies)r.propagation.atmosphere_boundaries.push_back({body.id,0});
                r.target_position_tolerance_m=200;r.finite_difference_step_mps=0.1;r.maximum_impulse_mps=20000;r.strict_disagreement_limit_m=200;r.maximum_iterations=8;
                send({{"type","progress"},{"phase","refinement"},{"sampled_cells",sampled},{"total_cells",nlaunch*nflight},
                    {"initial_offset_m",1000},{"target_offset_m",1000},
                    {"actual_initial_position_m",vector_json(r.initial_state.position_m)},
                    {"actual_target_position_m",vector_json(r.target_position_m)}});
                if(cancelled()){cancel_event();return;}
                auto result=refine_terminal_position(snapshot,eph,r);
                if(cancelled()){cancel_event();return;}
                send({{"type","refinement"},{"status","terminal_position_targeted"},{"position_residual_m",result.position_residual_m},
                    {"strict_disagreement_m",result.strict_disagreement_m},{"impulse_mps",vector_json(result.impulse_mps)},
                    {"iterations",result.iterations},{"evaluations",result.evaluations},
                    {"initial_offset_m",1000},{"target_offset_m",1000},
                    {"actual_initial_position_m",vector_json(r.initial_state.position_m)},
                    {"actual_target_position_m",vector_json(r.target_position_m)},
                    {"target_position_tolerance_m",r.target_position_tolerance_m},
                    {"strict_disagreement_limit_m",r.strict_disagreement_limit_m}});
            }catch(const std::exception& e){
                if(cancelled()){cancel_event();return;}
                send({{"type","refinement"},{"status","terminal_position_failed"},{"diagnostic",e.what()}});
            }
        }
        send({{"type","complete"},{"sampled_cells",sampled},{"rejected_cells",rejected},{"retained_candidates",retained.size()},
            {"ranked_candidates",ranked_json(q,retained)},
            {"best_screening_score_mps",retained.empty()?json(nullptr):json(retained.front().screening_score_mps)},
            {"status",retained.empty()?"no_screened_seed":"screened_seeds_only"}});
    }catch(const SearchError& e){error("invalid_request",e.what());}
    catch(const std::exception& e){error(work_started?"work_failed":"invalid_request",e.what());}
}
}
