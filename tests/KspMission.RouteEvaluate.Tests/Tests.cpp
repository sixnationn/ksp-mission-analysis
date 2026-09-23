#include "RouteEvaluate.hpp"
#include "RuntimeReader.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>
using namespace ksp;
namespace {
int checks=0;
void check(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
template<class F> void rejects(F action,const char* text){try{action();}catch(const RouteEvaluationError& error){
    if(std::string(error.what()).find(text)==std::string::npos)
        throw std::runtime_error(std::string("expected '")+text+"', got '"+error.what()+"'");
    ++checks;return;}
    throw std::runtime_error(std::string("missing rejection: ")+text);}
constexpr double stay=5184000,arrival=1000000,departure=arrival+stay,encounter=departure+500000,home_return=departure+1000000;
Snapshot source(){
    Snapshot s;s.analysis_ready=true;s.snapshot_hash="fixed-route-test-v1";s.confidence="synthetic_fixture";
    s.state_epoch_ut_s=0;s.frame={"synthetic_barycenter","X,Y,Z","right",true};
    const double mu=4*std::acos(-1.0)*std::acos(-1.0)*1e18/(stay*stay);
    s.bodies={{"sun",1e6,1000,State{{0,1e10,0},{0,0,0}},0},
        {"home",mu,1000,State{{0,0,0},{0,0,0}},0},
        {"mars",mu,1000,State{{1e8,0,0},{0,0,0}},0},
        {"venus",1e5,1000,State{{5e7,1e6,0},{0,0,0}},0}};
    Vec3 centre{},drift{};double total=0;for(const auto& body:s.bodies){total+=body.mu_m3_s2;
        centre=centre+body.state->position_m*body.mu_m3_s2;drift=drift+body.state->velocity_mps*body.mu_m3_s2;}
    centre=centre*(1/total);drift=drift*(1/total);
    for(auto& body:s.bodies){body.state->position_m=body.state->position_m-centre;body.state->velocity_mps=body.state->velocity_mps-drift;}
    return s;
}
std::string runtime_bytes(const Snapshot& s,double venus_atmosphere_m=0){
    using nlohmann::json;
    json bodies=json::array();
    for(const auto& b:s.bodies){
        const auto p=b.state->position_m,v=b.state->velocity_mps;
        bodies.push_back({{"id",b.id},{"parent_id",b.id=="sun"?json(nullptr):json("sun")},
            {"mu_m3_s2",b.mu_m3_s2},{"radius_m",b.radius_m},
            {"atmosphere_boundary_m",b.id=="venus"?venus_atmosphere_m:0.0},
            {"state_epoch_ut_s",0.0},{"position_m",{p.x,p.y,p.z}},
            {"velocity_mps",{v.x,v.y,v.z}}});
    }
    return json{{"schema_version",1},{"confidence","runtime_observed_uncompared"},
        {"capture",{{"exporter_id","test-runtime-shaped"},{"exporter_version","1"},
            {"game_version","test"},{"save_id","test"},{"capture_ut_s",0.0},
            {"principia_loaded",true},{"state_source","principia_celestial_from_parent"},
            {"mods",json::array({{{"id","Principia"},{"version","test"}}})}}},
        {"frame",{{"origin","system_barycenter"},{"axes","principia_alicesun_frozen_at_capture"},
            {"handedness","right"},{"inertial",true},{"source_frame","Principia/AliceSun"},
            {"transform_method","parent_relative_sum_then_com_translation"},{"transform_version","1"}}},
        {"calendar",{{"day_duration_s",86400},{"display_origin_ut_s",0},
            {"use_leap_years",false},{"month_lengths",{31,28,31,30,31,30,31,31,30,31,30,31}}}},
        {"bodies",bodies}}.dump();
}
SpacecraftSettings spacecraft(double first,double last,bool strict=false){
    SpacecraftSettings q;q.start_ut_s=first;q.end_ut_s=last;
    q.abs_position_tolerance_m=strict?0.1:1;q.abs_velocity_tolerance_mps=strict?1e-5:1e-4;
    q.relative_tolerance=strict?1e-9:1e-8;q.min_step_s=0.01;q.max_step_s=strict?5000:10000;
    q.max_accepted_steps=100000;q.safety_margin_m=0;
    q.atmosphere_boundaries={{"sun",0},{"home",0},{"mars",0},{"venus",0}};return q;
}
State arc(const Snapshot& s,const Ephemeris& e,State initial,double first,double last){
    auto result=propagate(s,e,initial,spacecraft(first,last));
    if(!result.success)throw std::runtime_error("fixture arc unsafe");return result.final_state;
}
Vec3 circle(const Snapshot& s,const Ephemeris& e,const std::string& id,State craft,double ut){
    const auto body=e.query(id,ut);const auto relative=craft.position_m-body.position_m;
    const double r=norm(relative),speed=std::sqrt(std::find_if(s.bodies.begin(),s.bodies.end(),
        [&](const Body& b){return b.id==id;})->mu_m3_s2/r);
    return body.velocity_mps+Vec3{-relative.y/r*speed,relative.x/r*speed,0};
}
struct Shot {Vec3 impulse;State final;};
Shot shoot(const Snapshot& s,const Ephemeris& e,State initial,double first,double last,Vec3 target,Vec3 guess){
    State final{};for(int iteration=0;iteration<10;++iteration){
        auto trial=initial;trial.velocity_mps=trial.velocity_mps+guess;final=arc(s,e,trial,first,last);
        const Vec3 error=final.position_m-target;if(norm(error)<10)return {guess,final};
        const double perturb=0.1;auto x=trial,y=trial;x.velocity_mps.x+=perturb;y.velocity_mps.y+=perturb;
        const auto ex=arc(s,e,x,first,last).position_m,ey=arc(s,e,y,first,last).position_m;
        const double a=(ex.x-final.position_m.x)/perturb,b=(ey.x-final.position_m.x)/perturb;
        const double c=(ex.y-final.position_m.y)/perturb,d=(ey.y-final.position_m.y)/perturb;
        const double det=a*d-b*c;if(std::abs(det)<1)throw std::runtime_error("fixture shooting singular");
        guess.x-=(d*error.x-b*error.y)/det;guess.y-=(-c*error.x+a*error.y)/det;
    }
    throw std::runtime_error("fixture shooting did not converge");
}
State relative(const Ephemeris& e,const std::string& id,State craft,double ut){
    const auto body=e.query(id,ut);return {craft.position_m-body.position_m,craft.velocity_mps-body.velocity_mps};
}
RouteEvaluationRequest fixture(const Snapshot& s){
    RouteEvaluationRequest q;q.expected_snapshot_hash=s.snapshot_hash;q.expected_frame_origin=s.frame.origin;
    q.expected_frame_axes=s.frame.axes;q.expected_frame_handedness=s.frame.handedness;q.expected_state_epoch_ut_s=0;
    auto& r=q.route;r.result_label="patched_conic_screened_route";r.route_id="manufactured-fixed-route";
    r.snapshot_hash=s.snapshot_hash;r.source_confidence=s.confidence;
    r.home_mars.departure_ut_s=0;r.home_mars.arrival_ut_s=arrival;r.home_mars.flight_time_s=arrival;
    r.mars_venus.departure_ut_s=departure;r.mars_venus.arrival_ut_s=encounter;r.mars_venus.flight_time_s=500000;
    r.venus_home.departure_ut_s=encounter;r.venus_home.arrival_ut_s=home_return;r.venus_home.flight_time_s=500000;
    r.home_mars.departure_body_id="home";r.home_mars.arrival_body_id="mars";
    r.mars_venus.departure_body_id="mars";r.mars_venus.arrival_body_id="venus";
    r.venus_home.departure_body_id="venus";r.venus_home.arrival_body_id="home";
    for(auto* leg:{&r.home_mars,&r.mars_venus,&r.venus_home}){
        leg->central_body_id="sun";leg->snapshot_hash=s.snapshot_hash;leg->source_confidence=s.confidence;
    }
    r.launch_ut_s=0;r.return_ut_s=home_return;r.total_duration_s=home_return;r.fixed_stay_s=stay;
    q.home_parking_altitude_m=999000;q.mars_parking_altitude_m=999000;q.home_capture_altitude_m=999000;
    q.fixed_position_tolerance_m=10000;q.fixed_velocity_tolerance_mps=0.1;
    q.parking_radius_tolerance_m=10000;q.parking_radial_velocity_tolerance_mps=0.01;
    q.parking_tangential_speed_tolerance_mps=0.01;
    q.venus_window_halfwidth_s=100000;q.venus_max_encounter_radius_m=2000000;q.venus_safety_margin_m=10000;
    q.disagreement_position_m=1000;q.disagreement_velocity_mps=0.01;q.disagreement_event_time_s=10;
    q.disagreement_mars_extremum_radius_m=1000;q.disagreement_mars_extremum_time_s=stay;
    q.coarse_ephemeris={0,home_return,1000,10,1e-5,100000};
    q.strict_ephemeris={0,home_return,500,1,1e-6,100000};
    q.coarse_spacecraft=spacecraft(0,home_return);q.strict_spacecraft=spacecraft(0,home_return,true);
    q.atmosphere_boundaries=q.coarse_spacecraft.atmosphere_boundaries;
    const auto e=integrate(s,q.coarse_ephemeris);
    const auto home=e.query("home",0);q.launch_parking_state={home.position_m+Vec3{1e6,0,0},
        home.velocity_mps+Vec3{0,std::sqrt(s.bodies[1].mu_m3_s2/1e6),0}};
    q.checkpoint_targets_relative[0]=relative(e,"home",q.launch_parking_state,0);
    const auto mars=e.query("mars",arrival),home_end=e.query("home",home_return);
    const auto first=shoot(s,e,q.launch_parking_state,0,arrival,mars.position_m+Vec3{-1e6,0,0},{98,0,0});
    q.impulses_mps[0]=first.impulse;
    auto post_capture=first.final;const auto circular_mars=circle(s,e,"mars",post_capture,arrival);
    q.impulses_mps[1]=circular_mars-post_capture.velocity_mps;post_capture.velocity_mps=circular_mars;
    q.checkpoint_targets_relative[1]=relative(e,"mars",post_capture,arrival);
    const auto pre_departure=arc(s,e,post_capture,arrival,departure);
    q.checkpoint_targets_relative[2]=relative(e,"mars",pre_departure,departure);
    const auto second=shoot(s,e,pre_departure,departure,home_return,home_end.position_m+Vec3{1e6,0,0},{-98,0,0});
    q.impulses_mps[2]=second.impulse;
    auto post_home=second.final;const auto circular_home=circle(s,e,"home",post_home,home_return);
    q.impulses_mps[3]=circular_home-post_home.velocity_mps;post_home.velocity_mps=circular_home;
    q.checkpoint_targets_relative[3]=relative(e,"home",post_home,home_return);
    return q;
}
void tests(){
    // A safe screened flyby cannot mask a second, unsafe Venus periapsis later in the route.
    std::vector<EncounterEvent> two_venus{{100,"venus","origin","axes",120000,0,{}},
        {1000,"venus","origin","axes",115000,0,{}}};
    check(select_safe_venus_encounter(two_venus,"venus",100,10,110000,200000).ut_s==100,
        "two safe Venus encounters select the screened event");
    two_venus[1].distance_m=109999;
    rejects([&]{select_safe_venus_encounter(two_venus,"venus",100,10,110000,200000);},
        "Venus unsafe actual encounter");
    const auto s=source();auto q=fixture(s);
    auto bad=q;bad.expected_snapshot_hash="stale";rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"hash");
    bad=q;bad.route.mars_venus.snapshot_hash="other";
    rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"leg provenance");
    bad=q;bad.expected_frame_axes="wrong";rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"frame");
    bad=q;bad.expected_state_epoch_ut_s=1;rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"epoch");
    bad=q;bad.atmosphere_boundaries.pop_back();rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"atmosphere");
    bad=q;bad.launch_parking_state.position_m=s.bodies[1].state->position_m;
    rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"parking");
    bad=q;bad.impulses_mps[0].x=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"nonfinite");
    bad=q;bad.route.mars_venus.departure_ut_s+=1;rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"stay");
    bad=q;bad.coarse_ephemeris.end_ut_s=home_return-1;rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"coverage");
    bad=q;bad.coarse_ephemeris.max_velocity_fit_error_mps=1;rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"fit budget");
    bad=q;bad.launch_parking_state.velocity_mps.x+=1;
    bad.checkpoint_targets_relative[0].velocity_mps.x+=1;
    rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"parking");
    bad=q;bad.impulses_mps[1]={0,0,0};rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"unsafe");
    bad=q;bad.venus_max_encounter_radius_m=1000;rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"Venus");
    bad=q;bad.venus_safety_margin_m=2e6;rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"unsafe");
    bad=q;bad.disagreement_position_m=1e-12;rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"disagreement");
    bad=q;bad.disagreement_velocity_mps=1e-12;rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"disagreement");
    bad=q;bad.disagreement_event_time_s=1e-12;rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"disagreement");
    bad=q;bad.disagreement_mars_extremum_radius_m=1e-12;
    rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"Mars extrema disagreement");
    bad=q;bad.coarse_spacecraft.burns.push_back({encounter,{1,0,0}});
    rejects([&]{evaluate_fixed_route_synthetic_fixture(s,bad);},"extra burns");
    auto absent=s;absent.bodies.erase(std::remove_if(absent.bodies.begin(),absent.bodies.end(),
        [](const Body& body){return body.id=="venus";}),absent.bodies.end());
    rejects([&]{evaluate_fixed_route_synthetic_fixture(absent,q);},"missing body");
    const auto result=evaluate_fixed_route_synthetic_fixture(s,q);
    check(result.result_label=="independent_nbody_fixed_impulse_checkpointed_only"&&result.coarse.burns.size()==4,"accepted result and four burns");
    check(result.coarse.venus.body_id=="venus"&&result.coarse.venus.distance_m<q.venus_max_encounter_radius_m,
          "actual Venus event");
    check(result.total_charged_delta_v_mps>0,"charged delta-v");
    auto excursion=q;excursion.impulses_mps[1].x+=0.1;
    excursion.fixed_position_tolerance_m=1e6;excursion.fixed_velocity_tolerance_mps=1;
    excursion.parking_radius_tolerance_m=20000;
    excursion.parking_radial_velocity_tolerance_mps=0.2;
    excursion.parking_tangential_speed_tolerance_mps=0.2;
    const auto excursion_ephemeris=integrate(s,excursion.coarse_ephemeris);
    auto excursion_settings=excursion.coarse_spacecraft;
    excursion_settings.burns={{0,excursion.impulses_mps[0]},{arrival,excursion.impulses_mps[1]},
        {departure,excursion.impulses_mps[2]},{home_return,excursion.impulses_mps[3]}};
    excursion_settings.radius_monitor=RadiusMonitor{"mars",arrival,departure};
    const auto excursion_run=propagate(s,excursion_ephemeris,excursion.launch_parking_state,excursion_settings);
    check(excursion_run.success&&excursion_run.burns.size()==4,"manufactured eccentric Mars coast propagates");
    const double capture_radius=norm(relative(excursion_ephemeris,"mars",excursion_run.burns[1].after,arrival).position_m);
    const double departure_radius=norm(relative(excursion_ephemeris,"mars",excursion_run.burns[2].before,departure).position_m);
    check(std::abs(capture_radius-1e6)<excursion.parking_radius_tolerance_m&&
          std::abs(departure_radius-1e6)<excursion.parking_radius_tolerance_m,
          "eccentric stay endpoints within radius shell");
    check(excursion_run.monitored_radius&&
          excursion_run.monitored_radius->maximum_m>1e6+excursion.parking_radius_tolerance_m,
          "actual interior apoapsis exceeds parking shell");
    rejects([&]{evaluate_fixed_route_synthetic_fixture(s,excursion);},"Mars interior parking radius");
    check(result.coarse.mars_stay_radius.endpoint_count>=2&&
        result.coarse.mars_stay_radius.model_interval_lower_m>0,
        "Mars stay continuous radius enclosure");
    for(const auto& burn:result.coarse.burns)
        check(norm(burn.before.position_m-burn.after.position_m)==0,"position continuous through burn");
    auto tight=s;tight.snapshot_hash="tight-venus-v1";
    auto& venus=*std::find_if(tight.bodies.begin(),tight.bodies.end(),
        [](const Body& body){return body.id=="venus";});
    venus.radius_m=std::min(result.coarse.venus.distance_m,result.strict.venus.distance_m)
        -q.venus_safety_margin_m-15;
    bad=q;bad.expected_snapshot_hash=tight.snapshot_hash;bad.route.snapshot_hash=tight.snapshot_hash;
    for(auto* leg:{&bad.route.home_mars,&bad.route.mars_venus,&bad.route.venus_home})
        leg->snapshot_hash=tight.snapshot_hash;
    rejects([&]{evaluate_fixed_route_synthetic_fixture(tight,bad);},"disagreement");
    const auto bytes=runtime_bytes(s),hash=sha256_hex(bytes);
    const auto loaded=read_runtime_snapshot(bytes,hash);
    auto rq=fixture(loaded.snapshot);
    const auto runtime_result=evaluate_fixed_route_runtime(bytes,hash,rq);
    check(runtime_result.snapshot_hash==hash&&
        runtime_result.result_label=="independent_nbody_fixed_impulse_checkpointed_only"&&
        !runtime_result.route_seed_evidence_revalidated&&!runtime_result.mars_stay_continuously_verified,
        "runtime exact source and bounded label");
    rejects([&]{evaluate_fixed_route_runtime(bytes,std::string(64,'0'),rq);},"hash");
    rejects([&]{evaluate_fixed_route_runtime(bytes+" ",hash,rq);},"hash");
    auto false_provenance=bytes;
    const auto confidence_at=false_provenance.find("runtime_observed_uncompared");
    check(confidence_at!=std::string::npos,"fixture confidence present");
    false_provenance.replace(confidence_at,std::string("runtime_observed_uncompared").size(),
        "raw_config_provisional");
    rejects([&]{evaluate_fixed_route_runtime(false_provenance,sha256_hex(false_provenance),rq);},
        "confidence");
    const auto thick_bytes=runtime_bytes(s,1e6),thick_hash=sha256_hex(thick_bytes);
    auto thick_q=rq;thick_q.expected_snapshot_hash=thick_hash;thick_q.route.snapshot_hash=thick_hash;
    for(auto* leg:{&thick_q.route.home_mars,&thick_q.route.mars_venus,&thick_q.route.venus_home})
        leg->snapshot_hash=thick_hash;
    // Caller-supplied zero atmospheres must not override the byte-backed Venus boundary.
    rejects([&]{evaluate_fixed_route_runtime(thick_bytes,thick_hash,thick_q);},"unsafe");
    rejects([&]{evaluate_fixed_route_synthetic_fixture(loaded.snapshot,rq);},"synthetic_fixture");
    auto untrusted=rq;untrusted.route.home_mars.screening_score_mps=1e99;
    untrusted.route.home_mars.lambert.position_residual_m=1e99;
    untrusted.route.home_mars.lambert.departure_relative_velocity_mps={1e99,1e99,1e99};
    const auto seeded=evaluate_fixed_route_runtime(bytes,hash,untrusted);
    check(seeded.total_charged_delta_v_mps==runtime_result.total_charged_delta_v_mps,
        "screen seed evidence not trusted for fixed impulse result");
    std::cout<<std::setprecision(12)<<"PASS "<<checks<<" route evaluation checks; Venus distance m="<<result.coarse.venus.distance_m
             <<" radius disagreement m="<<result.venus_radius_disagreement_m
             <<" event time disagreement s="<<result.venus_event_time_disagreement_s
             <<" max position disagreement m="<<result.maximum_checkpoint_position_disagreement_m
             <<" max velocity disagreement m/s="<<result.maximum_checkpoint_velocity_disagreement_mps
             <<" Mars observed min/max m="<<result.coarse.mars_stay_radius.minimum_m<<'/'
             <<result.coarse.mars_stay_radius.maximum_m
             <<" Mars model interval lower/upper m="<<result.coarse.mars_stay_radius.model_interval_lower_m<<'/'
             <<result.coarse.mars_stay_radius.model_interval_upper_m
             <<" eccentric interior apo m="<<excursion_run.monitored_radius->maximum_m<<'\n';
}
}
int main(){try{tests();return 0;}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
