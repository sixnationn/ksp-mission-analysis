#include "SearchGrid.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ksp;
namespace {
int checks=0;
void check(bool x,const char* label){++checks;if(!x)throw std::runtime_error(label);}
template<class F> void fails(F f,const char* fragment){try{f();}catch(const SearchError& e){check(std::string(e.what()).find(fragment)!=std::string::npos,"wrong error");return;}throw std::runtime_error(fragment);}
Snapshot system(){
    constexpr double mu=3.986004418e14,r=1e7,R=1.2e7,t=2500;
    const double wa=std::sqrt(mu/(r*r*r)),wb=std::sqrt(mu/(R*R*R)),phase=std::acos(-1.0)/2-wb*t;
    Snapshot s;s.analysis_ready=true;s.confidence="synthetic_fixture";s.snapshot_hash="grid-fixture";
    s.state_epoch_ut_s=0;s.frame={"barycenter","X,Y,Z","right",true};
    s.bodies={{"star",mu,1e6,State{{0,0,0},{0,0,0}},0},
              {"home",1,100,State{{r,0,0},{0,r*wa,0}},0},
              {"target",1,100,State{{R*std::cos(phase),R*std::sin(phase),0},{-R*wb*std::sin(phase),R*wb*std::cos(phase),0}},0}};
    return s;
}
Ephemeris planets(const Snapshot& s){Settings x;x.start_ut_s=0;x.end_ut_s=5000;x.step_s=1;x.max_position_fit_error_m=100;x.max_velocity_fit_error_mps=0.01;return integrate(s,x);}
GridRequest base(){GridRequest q;q.central_body_id="star";q.departure_body_id="home";q.arrival_body_id="target";
    q.launch_start_ut_s=0;q.launch_end_ut_s=0;q.launch_step_s=100;
    q.flight_min_s=2500;q.flight_max_s=2500;q.flight_step_s=100;
    q.reference_normal={0,0,1};q.branch=TransferBranch::short_path;q.direction=AngularMomentumDirection::positive;
    q.max_position_residual_m=0.1;q.max_velocity_residual_mps=1e-4;q.max_candidates=10;q.stable_seed=42;
    q.central_atmosphere_altitude_m=0;return q;}
void failures(){
    auto s=system();auto e=planets(s);auto q=base();
    q.launch_step_s=0;fails([&]{screen_single_leg(s,e,q);},"launch_step_s");
    q=base();q.flight_step_s=-1;fails([&]{screen_single_leg(s,e,q);},"flight_step_s");
    q=base();q.launch_end_ut_s=5500;fails([&]{screen_single_leg(s,e,q);},"coverage");
    q=base();q.flight_max_s=5100;fails([&]{screen_single_leg(s,e,q);},"coverage");
    q=base();q.max_candidates=0;fails([&]{screen_single_leg(s,e,q);},"max_candidates");
    q=base();q.max_grid_cells=1000001;fails([&]{screen_single_leg(s,e,q);},"grid cell limit");
    q=base();q.launch_end_ut_s=55;fails([&]{screen_single_leg(s,e,q);},"integer");
    s.snapshot_hash="changed";fails([&]{screen_single_leg(s,e,q);},"snapshot_hash");
    s=system();s.frame.axes="changed";fails([&]{screen_single_leg(s,e,q);},"frame");
    s=system();s.analysis_ready=false;fails([&]{screen_single_leg(s,e,q);},"analysis_ready");
    s=system();s.bodies[1].state.reset();fails([&]{screen_single_leg(s,e,q);},"body state");
    s=system();s.bodies[2].epoch_ut_s=1;fails([&]{screen_single_leg(s,e,q);},"body state");
    s=system();q=base();q.central_body_id="missing";fails([&]{screen_single_leg(s,e,q);},"central_body_id");
    q=base();q.central_atmosphere_altitude_m.reset();fails([&]{screen_single_leg(s,e,q);},"central atmosphere");
    q=base();q.central_safety_margin_m=9e6;auto collision=screen_single_leg(s,e,q);
    check(collision.candidates.empty()&&collision.diagnostic=="no feasible screened seed","central clearance rejects intersecting conic");
}
void result_cases(){
    auto s=system();auto e=planets(s);auto q=base();auto result=screen_single_leg(s,e,q);
    check(result.candidates.size()==1&&result.diagnostic=="screened candidates","single screenable seed");
    const auto& a=result.candidates[0];
    check(a.departure_ut_s==0&&a.arrival_ut_s==2500&&a.flight_time_s==2500,"dated endpoints");
    check(a.snapshot_hash==s.snapshot_hash&&a.frame_origin==s.frame.origin&&a.central_body_id=="star","identity");
    check(a.lambert.position_residual_m<0.1&&a.lambert.velocity_residual_mps<1e-4,"Lambert bounds");
    auto central0=e.query("star",0),central1=e.query("star",2500);
    check(norm(a.departure_barycentric_velocity_mps-(a.lambert.departure_relative_velocity_mps+central0.velocity_mps))<1e-12,"departure barycentric velocity");
    check(norm(a.arrival_barycentric_velocity_mps-(a.lambert.arrival_relative_velocity_mps+central1.velocity_mps))<1e-12,"arrival barycentric velocity");
    q.launch_end_ut_s=400;q.flight_min_s=2100;q.flight_max_s=2900;q.max_candidates=8;
    auto many=screen_single_leg(s,e,q),again=screen_single_leg(s,e,q);
    q.max_candidates=100;
    auto all=screen_single_leg(s,e,q);
    check(!many.candidates.empty()&&many.candidates.size()<=8,"candidate cap");
    check(all.candidates.size()>many.candidates.size(),"cap exercised");
    check(many.candidates.size()==again.candidates.size(),"repeatable count");
    for(std::size_t i=0;i<many.candidates.size();++i){
        check(many.candidates[i].departure_ut_s==again.candidates[i].departure_ut_s&&many.candidates[i].flight_time_s==again.candidates[i].flight_time_s,"repeatable order");
        check(many.candidates[i].departure_ut_s==all.candidates[i].departure_ut_s&&many.candidates[i].flight_time_s==all.candidates[i].flight_time_s,"bounded top K matches full order");
        if(i)check(many.candidates[i-1].screening_score_mps<=many.candidates[i].screening_score_mps,"objective order");
    }
    // A common inertial translation leaves central-relative Lambert velocities unchanged.
    Vec3 shift{3e8,-4e8,2e8};auto shifted_s=s;auto shifted_e=e;
    for(auto& body:shifted_s.bodies)body.state->position_m=body.state->position_m+shift;
    for(auto& body_samples:shifted_e.samples)for(auto& state:body_samples)state.position_m=state.position_m+shift;
    auto shifted=screen_single_leg(shifted_s,shifted_e,base());
    check(shifted.candidates.size()==1&&norm(shifted.candidates[0].departure_barycentric_velocity_mps-a.departure_barycentric_velocity_mps)<1e-6,"translation invariant velocity");
    q=base();q.max_position_residual_m=1e-20;auto none=screen_single_leg(s,e,q);
    check(none.candidates.empty()&&none.diagnostic=="no feasible screened seed","no solution diagnostic");
    std::cout<<"known leg departure vinf m/s="<<a.departure_vinf_mps<<" arrival vinf m/s="<<a.arrival_vinf_mps
             <<" screen score m/s="<<a.screening_score_mps<<" Lambert position residual m="<<a.lambert.position_residual_m
             <<" velocity residual m/s="<<a.lambert.velocity_residual_mps<<" grid candidates="<<many.candidates.size()
             <<" ephemeris fit m="<<e.metadata.measured_max_position_error_m<<" m/s="<<e.metadata.measured_max_velocity_error_mps<<'\n';
}
}
int main(){try{failures();result_cases();std::cout<<"PASS "<<checks<<" checks\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
