#include "MissionSearch.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace ksp;
namespace {
int checks=0;
void check(bool value,const char* what){++checks;if(!value)throw std::runtime_error(what);}
template<class F> void fails(F call,const char* expected){
    try{call();}catch(const SearchError& error){
        check(std::string(error.what()).find(expected)!=std::string::npos,"wrong failure diagnostic");return;
    }
    throw std::runtime_error(expected);
}
Snapshot snapshot(){
    Snapshot s;s.analysis_ready=true;s.confidence="synthetic_fixture";s.snapshot_hash="route-search-fixture";s.state_epoch_ut_s=0;
    s.frame={"barycenter","X,Y,Z","right",true};
    const double radius=1e7,angle=2*std::acos(-1.0)/3;
    s.bodies={{"sun",3.986004418e14,1e5,State{{0,0,0},{0,0,0}},0},
        {"home",1e12,1e5,State{{radius,0,0},{0,0,0}},0},
        {"mars",2e12,1e5,State{{radius*std::cos(angle),radius*std::sin(angle),0},{0,0,0}},0},
        {"venus",1e14,1e4,State{{radius*std::cos(angle),-radius*std::sin(angle),0},{0,0,0}},0}};
    return s;
}
Ephemeris ephemeris(const Snapshot& s){
    Ephemeris e;e.metadata.snapshot_hash=s.snapshot_hash;e.metadata.source_confidence=s.confidence;
    e.metadata.frame_origin=s.frame.origin;e.metadata.frame_axes=s.frame.axes;
    e.metadata.frame_handedness=s.frame.handedness;e.metadata.frame_inertial=true;
    e.metadata.state_epoch_ut_s=0;e.metadata.start_ut_s=0;e.metadata.end_ut_s=6e6;e.metadata.step_s=6e6;
    for(const auto& b:s.bodies){e.body_ids.push_back(b.id);e.samples.push_back({*b.state,*b.state});}
    return e;
}
MissionSearchRequest request(){
    MissionSearchRequest q;
    auto& r=q.route;r.central_body_id="sun";r.home_body_id="home";r.mars_body_id="mars";r.venus_body_id="venus";
    r.reference_normal={0,0,1};r.max_lambert_position_residual_m=1;r.max_lambert_velocity_residual_mps=1e-3;
    r.launch_start_ut_s=0;r.launch_end_ut_s=0;r.fixed_stay_s=5184000;r.time_tolerance_s=0;
    r.max_total_duration_s=6e6;r.home_parking_altitude_m=1e5;r.mars_parking_altitude_m=2e5;r.return_capture_altitude_m=1e5;
    r.home_atmosphere_altitude_m=0;r.mars_atmosphere_altitude_m=0;r.venus_atmosphere_altitude_m=1e3;
    r.venus_safety_margin_m=1e3;r.venus_maximum_periapsis_m=1e9;r.venus_speed_tolerance_mps=1e-4;
    r.return_condition=ReturnCondition::parking_capture;
    q.launch_step_s=1;q.legs={FlightGrid{1000,1000,1},FlightGrid{1000,1000,1},FlightGrid{1000,1000,1}};
    q.central_atmosphere_altitude_m=0;q.central_safety_margin_m=0;q.max_cells=100;q.max_routes=5;
    return q;
}
void run(){
    const auto s=snapshot();const auto e=ephemeris(s);auto q=request();
    std::vector<MissionSearchProgress> progress;
    const auto result=search_mission(s,e,q,{},[&](const MissionSearchProgress& p){progress.push_back(p);});
    check(!result.cancelled&&result.routes.size()==1,"one complete screened route");
    check(result.snapshot_hash==s.snapshot_hash&&result.source_confidence==s.confidence,"source identity");
    check(result.sampled_cells==3&&result.total_upper_bound_cells==3,"all three cells sampled");
    check(result.routes[0].result_label=="patched_conic_screened_route","screening label");
    check(std::abs(result.routes[0].total_optimistic_delta_v_mps-58181.2460942699)<1e-6,"known synthetic burn total");
    check(result.routes[0].fixed_stay_s==5184000&&result.routes[0].home_injection_mps>0&&
          result.routes[0].mars_capture_mps>0&&result.routes[0].mars_departure_mps>0&&
          result.routes[0].home_return_capture_mps>0,"separate burn accounting");
    check(!progress.empty()&&progress.back().sampled_cells==3,"progress reaches sampled cells");
    for(std::size_t i=1;i<progress.size();++i)
        check(progress[i].sampled_cells>=progress[i-1].sampled_cells&&
              progress[i].sampled_cells<=progress[i].total_upper_bound_cells,"monotonic bounded progress");
    const auto again=search_mission(s,e,q);
    check(again.routes.size()==1&&again.routes[0].route_id==result.routes[0].route_id&&
          again.routes[0].total_optimistic_delta_v_mps==result.routes[0].total_optimistic_delta_v_mps,"repeatable search");
    q.route.launch_end_ut_s=10;q.launch_step_s=10;
    const auto two=search_mission(s,e,q);
    check(two.routes.size()==2&&two.routes[0].route_id!=two.routes[1].route_id,"distinct route IDs");
    check(two.routes[0].total_optimistic_delta_v_mps<=two.routes[1].total_optimistic_delta_v_mps,"deterministic ranking");
    q.max_routes=1;const auto top=search_mission(s,e,q);
    check(top.routes.size()==1&&top.routes[0].route_id==two.routes[0].route_id&&top.peak_retained_routes==1,"bounded top K");
    const auto cancelled=search_mission(s,e,q,[&]{return false;});
    check(!cancelled.cancelled,"false cancellation callback leaves complete status");
    std::size_t last_sampled=0;
    const auto partial=search_mission(s,e,q,[&]{return last_sampled>=3;},[&](const MissionSearchProgress& p){last_sampled=p.sampled_cells;});
    check(partial.cancelled&&partial.routes.size()==1&&partial.sampled_cells==3,"cancellation keeps accepted route");
    q=request();q.launch_step_s=0;fails([&]{search_mission(s,e,q);},"launch grid");
    q=request();q.legs[1].step_s=0;fails([&]{search_mission(s,e,q);},"flight grid");
    q=request();q.route.launch_end_ut_s=9;q.launch_step_s=5;fails([&]{search_mission(s,e,q);},"integer");
    q=request();q.max_cells=2;fails([&]{search_mission(s,e,q);},"cell limit");
    q=request();q.max_cells=100001;fails([&]{search_mission(s,e,q);},"cell limit");
    q=request();q.legs[2].max_s=1e6;q.legs[2].step_s=999000;fails([&]{search_mission(s,e,q);},"coverage");
    q=request();q.route.home_body_id="unknown";fails([&]{search_mission(s,e,q);},"unknown");
    q=request();q.route.venus_atmosphere_altitude_m.reset();fails([&]{search_mission(s,e,q);},"atmosphere");
    q=request();q.central_atmosphere_altitude_m.reset();fails([&]{search_mission(s,e,q);},"central atmosphere");
    q=request();q.route.fixed_stay_s=5184001;fails([&]{search_mission(s,e,q);},"stay");
    q=request();q.route.home_parking_altitude_m=-1;fails([&]{search_mission(s,e,q);},"altitude");
    q=request();q.route.venus_safety_margin_m=5e7;
    const auto unsafe=search_mission(s,e,q);
    check(unsafe.routes.empty()&&unsafe.sampled_cells==3&&unsafe.rejected_cells==0&&
          unsafe.considered_combinations==1&&unsafe.rejected_route_combinations==1,
          "impossible flyby rejects assembled route, not screened cells");
    q=request();q.route.max_total_duration_s=5186999;
    check(search_mission(s,e,q).routes.empty(),"duration cap leaves no route");
    q=request();q.route.home_mars.push_back({});fails([&]{search_mission(s,e,q);},"seed vectors");
    auto wrong=s;wrong.snapshot_hash="wrong";q=request();fails([&]{search_mission(wrong,e,q);},"snapshot_hash");
}
}
int main(){try{run();std::cout<<"PASS "<<checks<<" bounded mission-search checks\n";return 0;}
catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
