#include "RouteEvaluate.hpp"
#include "RuntimeReader.hpp"
#include <algorithm>
#include <cmath>

namespace ksp {
namespace {
bool finite(double x){return std::isfinite(x);}
bool finite(Vec3 v){return finite(v.x)&&finite(v.y)&&finite(v.z);}
bool finite(State s){return finite(s.position_m)&&finite(s.velocity_mps);}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
const Body& body(const Snapshot& s,const std::string& id){
    auto it=std::find_if(s.bodies.begin(),s.bodies.end(),[&](const Body& b){return b.id==id;});
    if(it==s.bodies.end())throw RouteEvaluationError("missing body: "+id);return *it;
}
double atmosphere(const RouteEvaluationRequest& q,const std::string& id){
    auto matches=std::count_if(q.atmosphere_boundaries.begin(),q.atmosphere_boundaries.end(),
        [&](const AtmosphereBoundary& a){return a.body_id==id;});
    if(matches!=1)throw RouteEvaluationError("missing or conflicting atmosphere: "+id);
    auto it=std::find_if(q.atmosphere_boundaries.begin(),q.atmosphere_boundaries.end(),
        [&](const AtmosphereBoundary& a){return a.body_id==id;});
    if(!finite(it->altitude_m)||it->altitude_m<0)throw RouteEvaluationError("invalid atmosphere: "+id);
    return it->altitude_m;
}
void positive(double v,const char* name){if(!finite(v)||v<=0)throw RouteEvaluationError(std::string(name)+" invalid");}
void nonnegative(double v,const char* name){if(!finite(v)||v<0)throw RouteEvaluationError(std::string(name)+" invalid");}
struct Dates {double launch,arrival,departure,venus,home;std::string home_id,mars_id,venus_id;};
Dates validate(const Snapshot& s,const RouteEvaluationRequest& q){
    if(!s.analysis_ready||s.snapshot_hash.empty()||s.confidence.empty()||
       s.snapshot_hash!=q.expected_snapshot_hash||s.snapshot_hash!=q.route.snapshot_hash||
       s.confidence!=q.route.source_confidence)throw RouteEvaluationError("source hash or confidence mismatch");
    if(!s.frame.inertial||s.frame.handedness!="right"||s.frame.origin!=q.expected_frame_origin||
       s.frame.axes!=q.expected_frame_axes||s.frame.handedness!=q.expected_frame_handedness)
        throw RouteEvaluationError("source frame mismatch");
    if(!s.state_epoch_ut_s||!finite(*s.state_epoch_ut_s)||*s.state_epoch_ut_s!=q.expected_state_epoch_ut_s)
        throw RouteEvaluationError("source epoch mismatch");
    if(q.route.result_label!="patched_conic_screened_route")throw RouteEvaluationError("screened route label required");
    for(const auto& b:s.bodies){
        if(!b.state||!b.epoch_ut_s||*b.epoch_ut_s!=*s.state_epoch_ut_s||!finite(*b.state)||
           !finite(b.mu_m3_s2)||b.mu_m3_s2<=0||!finite(b.radius_m)||b.radius_m<=0)
            throw RouteEvaluationError("missing body state or epoch");
        (void)atmosphere(q,b.id);
    }
    const auto& r=q.route;
    Dates d{r.home_mars.departure_ut_s,r.home_mars.arrival_ut_s,r.mars_venus.departure_ut_s,
        r.mars_venus.arrival_ut_s,r.venus_home.arrival_ut_s,
        r.home_mars.departure_body_id,r.home_mars.arrival_body_id,r.mars_venus.arrival_body_id};
    if(d.home_id.empty()||d.mars_id.empty()||d.venus_id.empty()||d.home_id==d.mars_id||
       d.home_id==d.venus_id||d.mars_id==d.venus_id||r.mars_venus.departure_body_id!=d.mars_id||
       r.venus_home.departure_body_id!=d.venus_id||r.venus_home.arrival_body_id!=d.home_id)
        throw RouteEvaluationError("route body roles invalid");
    for(const auto* leg:{&r.home_mars,&r.mars_venus,&r.venus_home})
        if(leg->snapshot_hash!=s.snapshot_hash||leg->source_confidence!=s.confidence||
           leg->central_body_id.empty()||leg->central_body_id!=r.home_mars.central_body_id)
            throw RouteEvaluationError("leg provenance or central body mismatch");
    (void)body(s,r.home_mars.central_body_id);
    (void)body(s,d.home_id);(void)body(s,d.mars_id);(void)body(s,d.venus_id);
    if(!finite(d.launch)||!finite(d.arrival)||!finite(d.departure)||!finite(d.venus)||!finite(d.home)||
       !(d.launch<d.arrival&&d.arrival<d.departure&&d.departure<d.venus&&d.venus<d.home)||
       d.departure-d.arrival!=5184000||r.fixed_stay_s!=5184000)
        throw RouteEvaluationError("Mars stay or route dates invalid");
    if(r.venus_home.departure_ut_s!=d.venus||r.launch_ut_s!=d.launch||r.return_ut_s!=d.home||
       r.home_mars.flight_time_s!=d.arrival-d.launch||r.mars_venus.flight_time_s!=d.venus-d.departure||
       r.venus_home.flight_time_s!=d.home-d.venus)throw RouteEvaluationError("route date continuity invalid");
    if(!finite(q.launch_parking_state))throw RouteEvaluationError("nonfinite launch state");
    for(auto v:q.impulses_mps)if(!finite(v))throw RouteEvaluationError("nonfinite impulse");
    for(auto target:q.checkpoint_targets_relative)if(!finite(target))throw RouteEvaluationError("nonfinite target");
    for(double v:{q.fixed_position_tolerance_m,q.fixed_velocity_tolerance_mps,q.parking_radius_tolerance_m,
        q.parking_radial_velocity_tolerance_mps,q.parking_tangential_speed_tolerance_mps,
        q.venus_window_halfwidth_s,q.venus_max_encounter_radius_m,q.disagreement_position_m,
        q.disagreement_velocity_mps,q.disagreement_event_time_s})positive(v,"acceptance tolerance");
    nonnegative(q.venus_safety_margin_m,"Venus safety margin");
    for(double v:{q.home_parking_altitude_m,q.mars_parking_altitude_m,q.home_capture_altitude_m})
        nonnegative(v,"parking altitude");
    if(q.venus_window_halfwidth_s>=std::min(d.venus-d.departure,d.home-d.venus))
        throw RouteEvaluationError("Venus event window overlaps burn");
    if(q.home_parking_altitude_m<atmosphere(q,d.home_id)||q.home_capture_altitude_m<atmosphere(q,d.home_id)||
       q.mars_parking_altitude_m<atmosphere(q,d.mars_id))throw RouteEvaluationError("parking below atmosphere");
    if(q.venus_max_encounter_radius_m<=body(s,d.venus_id).radius_m+atmosphere(q,d.venus_id)+q.venus_safety_margin_m)
        throw RouteEvaluationError("Venus unsafe encounter radius");
    for(const auto& e:{q.coarse_ephemeris,q.strict_ephemeris}){
        if(e.start_ut_s>d.launch||e.end_ut_s<d.home||e.max_steps==0||e.max_steps>100000)
            throw RouteEvaluationError("ephemeris coverage or step cap invalid");
        positive(e.step_s,"ephemeris step");positive(e.max_position_fit_error_m,"position fit");
        positive(e.max_velocity_fit_error_mps,"velocity fit");
        const double longest=std::max({d.arrival-d.launch,d.departure-d.arrival,d.home-d.departure});
        if(e.max_position_fit_error_m+e.max_velocity_fit_error_mps*longest>0.1*q.fixed_position_tolerance_m)
            throw RouteEvaluationError("ephemeris fit budget consumed position allowance");
    }
    if(q.strict_ephemeris.start_ut_s!=q.coarse_ephemeris.start_ut_s||
       q.strict_ephemeris.end_ut_s!=q.coarse_ephemeris.end_ut_s||
       q.strict_ephemeris.step_s>=q.coarse_ephemeris.step_s||
       q.strict_ephemeris.max_position_fit_error_m>=q.coarse_ephemeris.max_position_fit_error_m||
       q.strict_ephemeris.max_velocity_fit_error_mps>=q.coarse_ephemeris.max_velocity_fit_error_mps)
        throw RouteEvaluationError("strict ephemeris settings not tighter");
    for(const auto& c:{q.coarse_spacecraft,q.strict_spacecraft}){
        if(c.start_ut_s!=d.launch||c.end_ut_s!=d.home||!c.burns.empty()||
           c.max_accepted_steps==0||c.max_accepted_steps>1000000)
            throw RouteEvaluationError("spacecraft coverage, extra burns or step cap invalid");
        positive(c.abs_position_tolerance_m,"spacecraft position tolerance");
        positive(c.abs_velocity_tolerance_mps,"spacecraft velocity tolerance");
        positive(c.relative_tolerance,"spacecraft relative tolerance");
        positive(c.min_step_s,"spacecraft minimum step");positive(c.max_step_s,"spacecraft maximum step");
        if(c.min_step_s>c.max_step_s)throw RouteEvaluationError("spacecraft step range invalid");
    }
    if(q.strict_spacecraft.max_step_s>=q.coarse_spacecraft.max_step_s||
       q.strict_spacecraft.abs_position_tolerance_m>=q.coarse_spacecraft.abs_position_tolerance_m||
       q.strict_spacecraft.abs_velocity_tolerance_mps>=q.coarse_spacecraft.abs_velocity_tolerance_mps||
       q.strict_spacecraft.relative_tolerance>=q.coarse_spacecraft.relative_tolerance)
        throw RouteEvaluationError("strict spacecraft settings not tighter");
    return d;
}
RouteCheckpoint parking(const Snapshot& s,const Ephemeris& e,const RouteEvaluationRequest& q,
    const std::string& name,const std::string& id,State craft,double ut,std::size_t index,double altitude){
    const auto& b=body(s,id);const auto centre=e.query(id,ut);
    const State relative{craft.position_m-centre.position_m,craft.velocity_mps-centre.velocity_mps};
    const auto& target=q.checkpoint_targets_relative[index];RouteCheckpoint out;
    out.name=name;out.ut_s=ut;out.spacecraft=craft;out.relative=relative;
    out.position_error_m=norm(relative.position_m-target.position_m);
    out.velocity_error_mps=norm(relative.velocity_mps-target.velocity_mps);
    const double radius=norm(relative.position_m);
    if(!finite(radius)||radius<=b.radius_m+atmosphere(q,id))
        throw RouteEvaluationError(name+" unsafe parking position");
    const double radial=dot(relative.position_m,relative.velocity_mps)/radius;
    const double tangential=std::sqrt(std::max(0.0,dot(relative.velocity_mps,relative.velocity_mps)-radial*radial));
    out.parking_radius_error_m=std::abs(radius-(b.radius_m+altitude));
    out.radial_velocity_mps=std::abs(radial);
    out.tangential_speed_error_mps=std::abs(tangential-std::sqrt(b.mu_m3_s2/radius));
    if(out.position_error_m>q.fixed_position_tolerance_m||out.velocity_error_mps>q.fixed_velocity_tolerance_mps)
        throw RouteEvaluationError(name+" fixed event position or velocity failed");
    if(out.parking_radius_error_m>q.parking_radius_tolerance_m||
       out.radial_velocity_mps>q.parking_radial_velocity_tolerance_mps||
       out.tangential_speed_error_mps>q.parking_tangential_speed_tolerance_mps)
        throw RouteEvaluationError(name+" parking radius or circular velocity failed");
    return out;
}
RoutePass pass(const Snapshot& s,const Ephemeris& e,const RouteEvaluationRequest& q,
    const Dates& d,SpacecraftSettings settings){
    settings.burns={{d.launch,q.impulses_mps[0]},{d.arrival,q.impulses_mps[1]},
        {d.departure,q.impulses_mps[2]},{d.home,q.impulses_mps[3]}};
    settings.atmosphere_boundaries=q.atmosphere_boundaries;settings.safety_margin_m=0;
    SpacecraftResult result;
    try{result=propagate(s,e,q.launch_parking_state,settings);}
    catch(const SpacecraftError& error){throw RouteEvaluationError(std::string("unsafe or unresolved propagation: ")+error.what());}
    if(!result.success||result.unsafe)throw RouteEvaluationError("unsafe intermediate encounter or collision");
    if(result.burns.size()!=4)throw RouteEvaluationError("exactly four fixed burns required");
    RoutePass out;out.accepted_steps=result.accepted_steps;out.rejected_steps=result.rejected_steps;
    std::copy(result.burns.begin(),result.burns.end(),out.burns.begin());
    out.checkpoints[0]=parking(s,e,q,"launch",d.home_id,out.burns[0].before,d.launch,0,q.home_parking_altitude_m);
    out.checkpoints[1]=parking(s,e,q,"Mars capture",d.mars_id,out.burns[1].after,d.arrival,1,q.mars_parking_altitude_m);
    out.checkpoints[2]=parking(s,e,q,"Mars pre-departure",d.mars_id,out.burns[2].before,d.departure,2,q.mars_parking_altitude_m);
    out.checkpoints[3]=parking(s,e,q,"home return capture",d.home_id,out.burns[3].after,d.home,3,q.home_capture_altitude_m);
    if(std::abs(norm(out.checkpoints[2].relative.position_m)-norm(out.checkpoints[1].relative.position_m))>
       q.parking_radius_tolerance_m)throw RouteEvaluationError("Mars parking drift excessive without stationkeeping");
    const double boundary=body(s,d.venus_id).radius_m+atmosphere(q,d.venus_id)+q.venus_safety_margin_m;
    out.venus=select_safe_venus_encounter(result.closest_approaches,d.venus_id,d.venus,
        q.venus_window_halfwidth_s,boundary,q.venus_max_encounter_radius_m);
    return out;
}
void measured_fit(const Metadata& metadata,const RouteEvaluationRequest& q,const Dates& d){
    const double longest=std::max({d.arrival-d.launch,d.departure-d.arrival,d.home-d.departure});
    if(metadata.measured_max_position_error_m+metadata.measured_max_velocity_error_mps*longest>
       0.1*q.fixed_position_tolerance_m)
        throw RouteEvaluationError("measured ephemeris fit budget consumed position allowance");
}
State venus_relative(const Ephemeris& e,const EncounterEvent& encounter){
    const auto b=e.query(encounter.body_id,encounter.ut_s);
    return {encounter.spacecraft_state.position_m-b.position_m,encounter.spacecraft_state.velocity_mps-b.velocity_mps};
}
}
EncounterEvent select_safe_venus_encounter(const std::vector<EncounterEvent>& events,
    const std::string& venus_id,double expected_ut_s,double window_halfwidth_s,
    double unsafe_boundary_m,double maximum_encounter_radius_m){
    const EncounterEvent* encounter=nullptr;
    for(const auto& event:events){
        if(event.body_id!=venus_id)continue;
        if(!finite(event.ut_s)||!finite(event.distance_m)||event.distance_m<=unsafe_boundary_m)
            throw RouteEvaluationError("Venus unsafe actual encounter");
        if(std::abs(event.ut_s-expected_ut_s)<=window_halfwidth_s&&
           (!encounter||event.distance_m<encounter->distance_m))encounter=&event;
    }
    if(!encounter)throw RouteEvaluationError("Venus actual closest event missing from time window");
    if(encounter->distance_m>maximum_encounter_radius_m)
        throw RouteEvaluationError("Venus closest event beyond encounter radius");
    return *encounter;
}
RouteEvaluationResult evaluate_core(const Snapshot& s,const RouteEvaluationRequest& q){
    const auto d=validate(s,q);
    Ephemeris coarse,strict;
    try{coarse=integrate(s,q.coarse_ephemeris);strict=integrate(s,q.strict_ephemeris);}
    catch(const EphemerisError& error){throw RouteEvaluationError(std::string("ephemeris integration failed: ")+error.what());}
    measured_fit(coarse.metadata,q,d);measured_fit(strict.metadata,q,d);
    (void)parking(s,coarse,q,"launch",d.home_id,q.launch_parking_state,d.launch,0,q.home_parking_altitude_m);
    (void)parking(s,strict,q,"launch",d.home_id,q.launch_parking_state,d.launch,0,q.home_parking_altitude_m);
    RouteEvaluationResult out;out.snapshot_hash=s.snapshot_hash;out.source_confidence=s.confidence;
    out.coarse=pass(s,coarse,q,d,q.coarse_spacecraft);
    out.strict=pass(s,strict,q,d,q.strict_spacecraft);
    for(std::size_t i=0;i<4;++i){
        out.maximum_checkpoint_position_disagreement_m=std::max(out.maximum_checkpoint_position_disagreement_m,
            norm(out.coarse.checkpoints[i].relative.position_m-out.strict.checkpoints[i].relative.position_m));
        out.maximum_checkpoint_velocity_disagreement_mps=std::max(out.maximum_checkpoint_velocity_disagreement_mps,
            norm(out.coarse.checkpoints[i].relative.velocity_mps-out.strict.checkpoints[i].relative.velocity_mps));
        out.total_charged_delta_v_mps+=out.coarse.burns[i].delta_v_magnitude_mps;
    }
    out.venus_event_time_disagreement_s=std::abs(out.coarse.venus.ut_s-out.strict.venus.ut_s);
    out.venus_radius_disagreement_m=std::abs(out.coarse.venus.distance_m-out.strict.venus.distance_m);
    const auto c_venus=venus_relative(coarse,out.coarse.venus),s_venus=venus_relative(strict,out.strict.venus);
    out.maximum_checkpoint_position_disagreement_m=std::max(out.maximum_checkpoint_position_disagreement_m,
        norm(c_venus.position_m-s_venus.position_m));
    out.maximum_checkpoint_velocity_disagreement_mps=std::max(out.maximum_checkpoint_velocity_disagreement_mps,
        norm(c_venus.velocity_mps-s_venus.velocity_mps));
    const double boundary=body(s,d.venus_id).radius_m+atmosphere(q,d.venus_id)+q.venus_safety_margin_m;
    const double margin=std::min(out.coarse.venus.distance_m-boundary,out.strict.venus.distance_m-boundary);
    if(out.maximum_checkpoint_position_disagreement_m>q.disagreement_position_m||
       out.maximum_checkpoint_velocity_disagreement_mps>q.disagreement_velocity_mps||
       out.venus_event_time_disagreement_s>q.disagreement_event_time_s||
       out.venus_radius_disagreement_m>0.1*margin)
        throw RouteEvaluationError("coarse/strict event disagreement exceeds separate budgets");
    return out;
}
RouteEvaluationResult evaluate_fixed_route_synthetic_fixture(const Snapshot& s,const RouteEvaluationRequest& q){
    if(s.confidence!="synthetic_fixture")
        throw RouteEvaluationError("synthetic fixture entrypoint requires synthetic_fixture confidence");
    return evaluate_core(s,q);
}
RouteEvaluationResult evaluate_fixed_route_runtime(const std::string& json_bytes,
    const std::string& expected_sha256,const RouteEvaluationRequest& request){
    RuntimeLoad loaded;
    try{loaded=read_runtime_snapshot(json_bytes,expected_sha256);}
    catch(const RuntimeReaderError& error){throw RouteEvaluationError(std::string("runtime source: ")+error.what());}
    auto trusted=request;
    trusted.atmosphere_boundaries=loaded.atmosphere_boundaries;
    return evaluate_core(loaded.snapshot,trusted);
}
}
