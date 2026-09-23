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
        q.disagreement_velocity_mps,q.disagreement_event_time_s,
        q.disagreement_mars_extremum_radius_m,q.disagreement_mars_extremum_time_s})positive(v,"acceptance tolerance");
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
    settings.radius_monitor=RadiusMonitor{d.mars_id,d.arrival,d.departure};
    SpacecraftResult result;
    try{result=propagate(s,e,q.launch_parking_state,settings);}
    catch(const SpacecraftError& error){throw RouteEvaluationError(std::string("unsafe or unresolved propagation: ")+error.what());}
    if(!result.success||result.unsafe)throw RouteEvaluationError("unsafe intermediate encounter or collision");
    if(result.burns.size()!=4)throw RouteEvaluationError("exactly four fixed burns required");
    if(!result.monitored_radius||result.monitored_radius->endpoint_count!=2)
        throw RouteEvaluationError("Mars radius monitor unresolved");
    RoutePass out;out.accepted_steps=result.accepted_steps;out.rejected_steps=result.rejected_steps;
    out.mars_stay_radius=*result.monitored_radius;
    std::copy(result.burns.begin(),result.burns.end(),out.burns.begin());
    out.checkpoints[0]=parking(s,e,q,"launch",d.home_id,out.burns[0].before,d.launch,0,q.home_parking_altitude_m);
    out.checkpoints[1]=parking(s,e,q,"Mars capture",d.mars_id,out.burns[1].after,d.arrival,1,q.mars_parking_altitude_m);
    out.checkpoints[2]=parking(s,e,q,"Mars pre-departure",d.mars_id,out.burns[2].before,d.departure,2,q.mars_parking_altitude_m);
    if(std::abs(norm(out.checkpoints[2].relative.position_m)-norm(out.checkpoints[1].relative.position_m))>
       q.parking_radius_tolerance_m)throw RouteEvaluationError("Mars parking drift excessive without stationkeeping");
    const auto& mars=body(s,d.mars_id);
    const double shell=mars.radius_m+q.mars_parking_altitude_m;
    if(out.mars_stay_radius.model_interval_lower_m<=mars.radius_m+atmosphere(q,d.mars_id)||
       out.mars_stay_radius.model_interval_lower_m<shell-q.parking_radius_tolerance_m||
       out.mars_stay_radius.model_interval_upper_m>shell+q.parking_radius_tolerance_m)
        throw RouteEvaluationError("Mars interior parking radius bound failed");
    out.checkpoints[3]=parking(s,e,q,"home return capture",d.home_id,out.burns[3].after,d.home,3,q.home_capture_altitude_m);
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
    out.mars_minimum_radius_disagreement_m=std::abs(out.coarse.mars_stay_radius.minimum_m-
        out.strict.mars_stay_radius.minimum_m);
    out.mars_maximum_radius_disagreement_m=std::abs(out.coarse.mars_stay_radius.maximum_m-
        out.strict.mars_stay_radius.maximum_m);
    out.mars_minimum_time_disagreement_s=std::abs(out.coarse.mars_stay_radius.minimum_ut_s-
        out.strict.mars_stay_radius.minimum_ut_s);
    out.mars_maximum_time_disagreement_s=std::abs(out.coarse.mars_stay_radius.maximum_ut_s-
        out.strict.mars_stay_radius.maximum_ut_s);
    if(out.mars_minimum_radius_disagreement_m>q.disagreement_mars_extremum_radius_m||
       out.mars_maximum_radius_disagreement_m>q.disagreement_mars_extremum_radius_m||
       out.mars_minimum_time_disagreement_s>q.disagreement_mars_extremum_time_s||
       out.mars_maximum_time_disagreement_s>q.disagreement_mars_extremum_time_s)
        throw RouteEvaluationError("Mars extrema disagreement exceeds separate budgets");
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

namespace {
Ephemeris prepare_probe_ephemeris(const Snapshot& snapshot,const RouteEvaluationRequest& baseline){
    const auto dates=validate(snapshot,baseline);
    Ephemeris ephemeris;
    try{ephemeris=integrate(snapshot,baseline.coarse_ephemeris);}
    catch(const EphemerisError& error){throw RouteEvaluationError(std::string("probe ephemeris integration failed: ")+error.what());}
    measured_fit(ephemeris.metadata,baseline,dates);
    if(!cache_compatible(ephemeris.metadata,snapshot,baseline.coarse_ephemeris)||
       ephemeris.body_ids.size()!=snapshot.bodies.size()||ephemeris.samples.size()!=snapshot.bodies.size())
        throw RouteEvaluationError("probe ephemeris source or body dimensions mismatch");
    for(std::size_t i=0;i<snapshot.bodies.size();++i)
        if(ephemeris.body_ids[i]!=snapshot.bodies[i].id||ephemeris.samples[i].empty()||
           !finite(ephemeris.samples[i].front())||
           norm(ephemeris.samples[i].front().position_m-snapshot.bodies[i].state->position_m)!=0||
           norm(ephemeris.samples[i].front().velocity_mps-snapshot.bodies[i].state->velocity_mps)!=0)
            throw RouteEvaluationError("probe ephemeris body order or first sample mismatch");
    return ephemeris;
}
RouteProbeCheckpoint probe_checkpoint(const Snapshot& snapshot,const Ephemeris& ephemeris,
    const RouteEvaluationRequest& baseline,const RouteProbeTrial& trial,const std::string& name,
    const std::string& id,State spacecraft,double ut,std::size_t index,double altitude){
    const auto& b=body(snapshot,id);const auto centre=ephemeris.query(id,ut);
    const State relative{spacecraft.position_m-centre.position_m,spacecraft.velocity_mps-centre.velocity_mps};
    const double radius=norm(relative.position_m);
    if(!finite(radius)||radius<=b.radius_m+atmosphere(baseline,id))
        throw RouteEvaluationError(name+" unsafe parking position");
    const double radial=dot(relative.position_m,relative.velocity_mps)/radius;
    const double speed2=dot(relative.velocity_mps,relative.velocity_mps);
    const double tangential=std::sqrt(std::max(0.0,speed2-radial*radial));
    RouteProbeCheckpoint out;out.name=name;out.ut_s=ut;out.spacecraft=spacecraft;out.relative=relative;
    out.signed_position_residual_m=relative.position_m-trial.checkpoint_targets_relative[index].position_m;
    out.signed_velocity_residual_mps=relative.velocity_mps-trial.checkpoint_targets_relative[index].velocity_mps;
    out.signed_parking_radius_residual_m=radius-(b.radius_m+altitude);
    out.signed_radial_velocity_mps=radial;
    out.signed_tangential_speed_residual_mps=tangential-std::sqrt(b.mu_m3_s2/radius);
    if(!finite(relative)||!finite(out.signed_position_residual_m)||!finite(out.signed_velocity_residual_mps)||
       !finite(out.signed_parking_radius_residual_m)||!finite(out.signed_radial_velocity_mps)||
       !finite(out.signed_tangential_speed_residual_mps))
        throw RouteEvaluationError(name+" nonfinite probe checkpoint diagnostic");
    return out;
}
}
RouteProbeContext prepare_route_probe_synthetic_fixture(const Snapshot& snapshot,const RouteEvaluationRequest& baseline){
    if(snapshot.confidence!="synthetic_fixture")throw RouteEvaluationError("synthetic fixture probe requires synthetic_fixture confidence");
    auto ephemeris=prepare_probe_ephemeris(snapshot,baseline);
    return RouteProbeContext(snapshot,baseline,std::move(ephemeris));
}
RouteProbeContext prepare_route_probe_runtime(const std::string& json_bytes,const std::string& expected_sha256,
    const RouteEvaluationRequest& baseline){
    RuntimeLoad loaded;
    try{loaded=read_runtime_snapshot(json_bytes,expected_sha256);}
    catch(const RuntimeReaderError& error){throw RouteEvaluationError(std::string("runtime source: ")+error.what());}
    auto trusted=baseline;trusted.atmosphere_boundaries=loaded.atmosphere_boundaries;
    auto ephemeris=prepare_probe_ephemeris(loaded.snapshot,trusted);
    return RouteProbeContext(std::move(loaded.snapshot),std::move(trusted),std::move(ephemeris),json_bytes,expected_sha256);
}
RouteProbeResult probe_route_trial(const RouteProbeContext& context,const RouteProbeTrial& trial){
    const auto& s=context.snapshot_;const auto& q=context.baseline_;const auto& ephemeris=context.ephemeris_;
    if(!finite(trial.launch_parking_state))throw RouteEvaluationError("nonfinite probe launch state");
    for(const auto& impulse:trial.impulses_mps)if(!finite(impulse))throw RouteEvaluationError("nonfinite probe impulse");
    for(const auto& target:trial.checkpoint_targets_relative)if(!finite(target))throw RouteEvaluationError("nonfinite probe target");
    const auto dates=validate(s,q);
    const auto launch_centre=ephemeris.query(dates.home_id,dates.launch);
    if(norm(trial.launch_parking_state.position_m-launch_centre.position_m)<=
       body(s,dates.home_id).radius_m+atmosphere(q,dates.home_id))
        throw RouteEvaluationError("unsafe probe launch parking position");
    auto settings=q.coarse_spacecraft;
    settings.burns={{dates.launch,trial.impulses_mps[0]},{dates.arrival,trial.impulses_mps[1]},
        {dates.departure,trial.impulses_mps[2]},{dates.home,trial.impulses_mps[3]}};
    settings.atmosphere_boundaries=q.atmosphere_boundaries;settings.safety_margin_m=0;
    settings.radius_monitor=RadiusMonitor{dates.mars_id,dates.arrival,dates.departure};
    SpacecraftResult propagation;
    try{propagation=propagate(s,ephemeris,trial.launch_parking_state,settings);}
    catch(const SpacecraftError& error){throw RouteEvaluationError(std::string("unsafe or unresolved probe propagation: ")+error.what());}
    if(!propagation.success||propagation.unsafe||propagation.burns.size()!=4||
       !propagation.monitored_radius||propagation.monitored_radius->endpoint_count!=2)
        throw RouteEvaluationError("unsafe or unresolved probe propagation");
    RouteProbeResult out;out.snapshot_hash=s.snapshot_hash;out.source_confidence=s.confidence;
    out.frame_origin=s.frame.origin;out.frame_axes=s.frame.axes;out.frame_handedness=s.frame.handedness;
    out.frame_inertial=s.frame.inertial;
    out.state_epoch_ut_s=*s.state_epoch_ut_s;out.ephemeris_metadata=ephemeris.metadata;out.trial=trial;
    out.accepted_steps=propagation.accepted_steps;out.rejected_steps=propagation.rejected_steps;
    out.mars_stay_radius=*propagation.monitored_radius;
    const auto& mars=out.mars_stay_radius;
    for(double value:{mars.minimum_m,mars.maximum_m,mars.minimum_ut_s,mars.maximum_ut_s,
        mars.model_interval_lower_m,mars.model_interval_upper_m})
        if(!finite(value))throw RouteEvaluationError("nonfinite Mars probe radius diagnostic");
    std::copy(propagation.burns.begin(),propagation.burns.end(),out.burns.begin());
    out.checkpoints[0]=probe_checkpoint(s,ephemeris,q,trial,"launch",dates.home_id,out.burns[0].before,dates.launch,0,q.home_parking_altitude_m);
    out.checkpoints[1]=probe_checkpoint(s,ephemeris,q,trial,"Mars capture",dates.mars_id,out.burns[1].after,dates.arrival,1,q.mars_parking_altitude_m);
    out.checkpoints[2]=probe_checkpoint(s,ephemeris,q,trial,"Mars pre-departure",dates.mars_id,out.burns[2].before,dates.departure,2,q.mars_parking_altitude_m);
    out.checkpoints[3]=probe_checkpoint(s,ephemeris,q,trial,"home return capture",dates.home_id,out.burns[3].after,dates.home,3,q.home_capture_altitude_m);
    for(const auto& burn:out.burns){
        if(!finite(burn.delta_v_magnitude_mps)||!finite(burn.delta_v_mps)||
           !finite(burn.before)||!finite(burn.after))throw RouteEvaluationError("nonfinite probe burn diagnostic");
        out.total_charged_delta_v_mps+=burn.delta_v_magnitude_mps;
    }
    if(!finite(out.total_charged_delta_v_mps))throw RouteEvaluationError("nonfinite probe charged delta-v");
    const double boundary=body(s,dates.venus_id).radius_m+atmosphere(q,dates.venus_id)+q.venus_safety_margin_m;
    for(const auto& event:propagation.closest_approaches){
        if(event.body_id!=dates.venus_id)continue;
        if(!finite(event.ut_s)||!finite(event.distance_m)||!finite(event.clearance_m)||
           !finite(event.spacecraft_state))throw RouteEvaluationError("nonfinite Venus event diagnostic");
        out.venus_events.push_back(event);
        const double margin=event.distance_m-boundary;
        if(!finite(margin))throw RouteEvaluationError("nonfinite Venus boundary margin");
        if(!out.minimum_observed_venus_boundary_margin_m||margin<*out.minimum_observed_venus_boundary_margin_m)
            out.minimum_observed_venus_boundary_margin_m=margin;
        if(std::abs(event.ut_s-dates.venus)<=q.venus_window_halfwidth_s&&
           (!out.selected_venus||event.distance_m<out.selected_venus->distance_m))out.selected_venus=event;
    }
    return out;
}
namespace {
double axis(Vec3 v,int i){return i==0?v.x:(i==1?v.y:v.z);}
void add_axis(Vec3& v,int i,double delta){if(i==0)v.x+=delta;else if(i==1)v.y+=delta;else v.z+=delta;}
bool same_vec(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool same_state(State a,State b){return same_vec(a.position_m,b.position_m)&&same_vec(a.velocity_mps,b.velocity_mps);}
Vec3 block_residual(const RouteProbeResult& result,int block){
    const auto& checkpoint=result.checkpoints[block<2?1:3];
    return (block==0||block==2)?checkpoint.signed_position_residual_m:checkpoint.signed_velocity_residual_mps;
}
std::optional<Vec3> solve_three(double matrix[3][3],Vec3 residual){
    double a[3][4]{};double scale=0;
    for(int row=0;row<3;++row){for(int col=0;col<3;++col){a[row][col]=matrix[row][col];scale=std::max(scale,std::abs(a[row][col]));}a[row][3]=-axis(residual,row);}
    if(!finite(scale)||scale==0)return std::nullopt;
    for(int col=0;col<3;++col){int pivot=col;
        for(int row=col+1;row<3;++row)if(std::abs(a[row][col])>std::abs(a[pivot][col]))pivot=row;
        if(!finite(a[pivot][col])||std::abs(a[pivot][col])<=1e-12*scale)return std::nullopt;
        for(int k=col;k<4;++k)std::swap(a[col][k],a[pivot][k]);
        const double divisor=a[col][col];for(int k=col;k<4;++k)a[col][k]/=divisor;
        for(int row=0;row<3;++row)if(row!=col){const double factor=a[row][col];for(int k=col;k<4;++k)a[row][k]-=factor*a[col][k];}
    }
    Vec3 change{a[0][3],a[1][3],a[2][3]};if(!finite(change))return std::nullopt;return change;
}
}
RouteShootingResult shoot_fixed_route(const RouteProbeContext& context,const RouteProbeTrial& seed,
    const RouteShootingLimits& limits){
    const auto& q=context.baseline_;const auto& s=context.snapshot_;
    if(!finite(limits.finite_difference_impulse_mps)||limits.finite_difference_impulse_mps<=0||
       !finite(limits.max_impulse_mps)||limits.max_impulse_mps<=0)
        throw RouteEvaluationError("shooting limit invalid");
    if(limits.max_iterations==0||limits.max_iterations>1000||limits.max_probe_evaluations==0||
       limits.max_probe_evaluations>10000)throw RouteEvaluationError("shooting budget invalid");
    if(!same_state(seed.launch_parking_state,q.launch_parking_state))
        throw RouteEvaluationError("shooting fixed launch state differs from context");
    for(std::size_t i=0;i<4;++i){
        if(!same_state(seed.checkpoint_targets_relative[i],q.checkpoint_targets_relative[i]))
            throw RouteEvaluationError("shooting fixed target differs from context");
        if(!finite(seed.impulses_mps[i])||!finite(norm(seed.impulses_mps[i]))||
           norm(seed.impulses_mps[i])>limits.max_impulse_mps)
            throw RouteEvaluationError("shooting seed impulse exceeds bound");
    }
    const auto dates=validate(s,q);
    RouteShootingResult out;RouteProbeTrial current_trial=seed;
    auto evaluate=[&](const RouteProbeTrial& trial)->std::optional<RouteProbeResult>{
        if(out.probe_evaluations>=limits.max_probe_evaluations)return std::nullopt;
        ++out.probe_evaluations;return probe_route_trial(context,trial);
    };
    out.final_probe=*evaluate(current_trial);
    auto coarse_reason=[&](const RouteProbeResult& probe)->std::string{
        if(!probe.selected_venus)return "missing_venus";
        if(!probe.minimum_observed_venus_boundary_margin_m||
           *probe.minimum_observed_venus_boundary_margin_m<=0||
           probe.selected_venus->distance_m>q.venus_max_encounter_radius_m)return "venus_boundary_or_radius";
        const auto& mars=body(s,dates.mars_id);const double shell=mars.radius_m+q.mars_parking_altitude_m;
        if(probe.mars_stay_radius.model_interval_lower_m<=mars.radius_m+atmosphere(q,dates.mars_id)||
           probe.mars_stay_radius.model_interval_lower_m<shell-q.parking_radius_tolerance_m||
           probe.mars_stay_radius.model_interval_upper_m>shell+q.parking_radius_tolerance_m)
            return "mars_stay_failed";
        for(const auto& checkpoint:probe.checkpoints)
            if(norm(checkpoint.signed_position_residual_m)>q.fixed_position_tolerance_m||
               norm(checkpoint.signed_velocity_residual_mps)>q.fixed_velocity_tolerance_mps||
               std::abs(checkpoint.signed_parking_radius_residual_m)>q.parking_radius_tolerance_m||
               std::abs(checkpoint.signed_radial_velocity_mps)>q.parking_radial_velocity_tolerance_mps||
               std::abs(checkpoint.signed_tangential_speed_residual_mps)>q.parking_tangential_speed_tolerance_mps)
                return "coarse_constraints_failed";
        return {};
    };
    auto strict_gate=[&](){auto request=q;request.impulses_mps=current_trial.impulses_mps;
        try{
            if(context.runtime_bytes_.empty())out.strict_result=evaluate_fixed_route_synthetic_fixture(s,request);
            else out.strict_result=evaluate_fixed_route_runtime(context.runtime_bytes_,context.runtime_hash_,request);
            out.status="checkpointed_accepted";
        }catch(const RouteEvaluationError&){out.status="strict_rejected";}
    };
    if(coarse_reason(out.final_probe).empty()){strict_gate();return out;}
    for(std::size_t iteration=0;iteration<limits.max_iterations;++iteration){
        bool changed=false;out.iterations=iteration+1;
        for(int block=0;block<4;++block){
            const Vec3 residual=block_residual(out.final_probe,block);
            // Leave slack for the downstream Mars stay and parking checks, which can
            // amplify small capture/departure velocity errors over long arcs.
            const double tolerance=(block==0||block==2)?
                std::min(q.fixed_position_tolerance_m,1.0):std::min(q.fixed_velocity_tolerance_mps,1e-6);
            if(norm(residual)<=tolerance)continue;
            Vec3 correction{};
            if(block==0||block==2){
                double jacobian[3][3]{};
                for(int col=0;col<3;++col){auto difference=current_trial;
                    double h=limits.finite_difference_impulse_mps;
                    add_axis(difference.impulses_mps[block],col,h);
                    if(norm(difference.impulses_mps[block])>limits.max_impulse_mps){h=-h;difference=current_trial;add_axis(difference.impulses_mps[block],col,h);}
                    if(!finite(norm(difference.impulses_mps[block]))||norm(difference.impulses_mps[block])>limits.max_impulse_mps){out.status="impulse_bound";return out;}
                    std::optional<RouteProbeResult> sample;
                    try{sample=evaluate(difference);}catch(const RouteEvaluationError&){out.status="unsafe_difference";return out;}
                    if(!sample){out.status="budget_exhausted";return out;}
                    const auto derivative=(block_residual(*sample,block)-residual)*(1.0/h);
                    for(int row=0;row<3;++row)jacobian[row][col]=axis(derivative,row);
                }
                const auto solved=solve_three(jacobian,residual);
                if(!solved){out.status="singular_jacobian";return out;}correction=*solved;
            }else correction=residual*(-1.0);
            bool accepted=false;
            for(int attempt=0;attempt<12;++attempt){const double alpha=std::ldexp(1.0,-attempt);
                auto candidate=current_trial;candidate.impulses_mps[block]=candidate.impulses_mps[block]+correction*alpha;
                if(!finite(candidate.impulses_mps[block])||!finite(norm(candidate.impulses_mps[block]))||
                   norm(candidate.impulses_mps[block])>limits.max_impulse_mps)continue;
                std::optional<RouteProbeResult> sample;
                try{sample=evaluate(candidate);}catch(const RouteEvaluationError&){continue;}
                if(!sample){out.status="budget_exhausted";return out;}
                if(norm(block_residual(*sample,block))<norm(residual)){
                    current_trial=std::move(candidate);out.final_probe=std::move(*sample);accepted=true;changed=true;break;
                }
            }
            if(!accepted){out.status="line_search_failed";return out;}
        }
        const auto reason=coarse_reason(out.final_probe);
        if(reason.empty()){strict_gate();return out;}
        if(!changed){out.status=reason;return out;}
    }
    out.status=coarse_reason(out.final_probe);if(out.status.empty())out.status="budget_exhausted";
    return out;
}
}
