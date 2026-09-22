#include "MissionRoute.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace ksp;
namespace {
int checks=0;
void check(bool x,const char* what){++checks;if(!x)throw std::runtime_error(what);}
template<class F> void fails(F call,const char* fragment){try{call();}catch(const SearchError& e){if(std::string(e.what()).find(fragment)==std::string::npos)throw std::runtime_error(std::string("expected ")+fragment+", got "+e.what());++checks;return;}throw std::runtime_error(fragment);}
Snapshot snapshot(){Snapshot s;s.analysis_ready=true;s.confidence="synthetic_fixture";s.snapshot_hash="route-fixture";s.state_epoch_ut_s=0;
    s.frame={"barycenter","X,Y,Z","right",true};
    const double r=1e7,c=std::cos(2*std::acos(-1.0)/3),t=std::sin(2*std::acos(-1.0)/3);
    s.bodies={{"sun",3.986004418e14,1e5,State{{0,0,0},{0,0,0}},0},
        {"home",1e12,1e5,State{{r,0,0},{0,0,0}},0},
        {"mars",2e12,1e5,State{{r*c,r*t,0},{0,0,0}},0},
        {"venus",1e14,1e4,State{{r*c,-r*t,0},{0,0,0}},0}};return s;}
Ephemeris ephemeris(const Snapshot& s){Ephemeris e;e.metadata.snapshot_hash=s.snapshot_hash;e.metadata.source_confidence=s.confidence;
    e.metadata.frame_origin=s.frame.origin;e.metadata.frame_axes=s.frame.axes;e.metadata.frame_handedness=s.frame.handedness;e.metadata.frame_inertial=true;
    e.metadata.state_epoch_ut_s=0;e.metadata.start_ut_s=0;e.metadata.end_ut_s=6e6;e.metadata.step_s=6e6;
    for(const auto& b:s.bodies){e.body_ids.push_back(b.id);e.samples.push_back({*b.state,*b.state});}return e;}
DatedLegCandidate leg(const Snapshot& s,const Ephemeris& e,std::string from,std::string to,double depart,double arrive){
    DatedLegCandidate c;c.departure_body_id=from;c.arrival_body_id=to;c.central_body_id="sun";
    c.departure_ut_s=depart;c.arrival_ut_s=arrive;c.flight_time_s=arrive-depart;
    c.snapshot_hash=s.snapshot_hash;c.source_confidence=s.confidence;c.frame_origin=s.frame.origin;c.frame_axes=s.frame.axes;c.ephemeris_metadata=e.metadata;
    LambertRequest request;request.departure_relative_position_m=e.query(from,depart).position_m-e.query("sun",depart).position_m;
    request.arrival_relative_position_m=e.query(to,arrive).position_m-e.query("sun",arrive).position_m;
    request.reference_normal={0,0,1};request.mu_m3_s2=s.bodies[0].mu_m3_s2;request.time_of_flight_s=c.flight_time_s;
    request.branch=TransferBranch::short_path;request.direction=AngularMomentumDirection::positive;
    request.max_position_residual_m=1;request.max_velocity_residual_mps=1e-3;
    c.lambert=solve_lambert(request);c.branch=request.branch;c.direction=request.direction;
    c.departure_barycentric_velocity_mps=c.lambert.departure_relative_velocity_mps+e.query("sun",depart).velocity_mps;
    c.arrival_barycentric_velocity_mps=c.lambert.arrival_relative_velocity_mps+e.query("sun",arrive).velocity_mps;
    c.departure_vinf_mps=norm(c.departure_barycentric_velocity_mps-e.query(from,depart).velocity_mps);
    c.arrival_vinf_mps=norm(c.arrival_barycentric_velocity_mps-e.query(to,arrive).velocity_mps);
    c.screening_score_mps=c.departure_vinf_mps+c.arrival_vinf_mps;return c;}
struct Fixture {Snapshot s=snapshot();Ephemeris e=ephemeris(s);RouteRequest q;Fixture(){
    q.central_body_id="sun";q.home_body_id="home";q.mars_body_id="mars";q.venus_body_id="venus";
    q.reference_normal={0,0,1};q.max_lambert_position_residual_m=1;q.max_lambert_velocity_residual_mps=1e-3;
    q.launch_start_ut_s=0;q.launch_end_ut_s=100;q.fixed_stay_s=5184000;q.time_tolerance_s=1e-6;q.max_total_duration_s=6e6;
    q.home_parking_altitude_m=1e5;q.mars_parking_altitude_m=2e5;q.return_capture_altitude_m=1e5;
    q.home_atmosphere_altitude_m=0;q.mars_atmosphere_altitude_m=0;q.venus_atmosphere_altitude_m=1e3;q.venus_safety_margin_m=1e3;q.venus_maximum_periapsis_m=1e9;q.venus_speed_tolerance_mps=1e-4;
    q.max_combinations=100;q.max_routes=5;q.return_condition=ReturnCondition::parking_capture;
    q.home_mars={leg(s,e,"home","mars",0,1000)};
    q.mars_venus={leg(s,e,"mars","venus",5185000,5186000)};
    q.venus_home={leg(s,e,"venus","home",5186000,5187000)};
}};
void failures(){Fixture f;auto q=f.q;
    q.home_mars[0].arrival_body_id="venus";fails([&]{assemble_routes(f.s,f.e,q);},"role order");q=f.q;
    q.mars_venus[0]=leg(f.s,f.e,"mars","venus",5185001,5186001);fails([&]{assemble_routes(f.s,f.e,q);},"stay");q=f.q;
    q.time_tolerance_s=1;q.fixed_stay_s=5184000.75;q.mars_venus[0]=leg(f.s,f.e,"mars","venus",5185001.5,5186001.5);
    fails([&]{assemble_routes(f.s,f.e,q);},"stay");q=f.q;
    q.venus_home[0]=leg(f.s,f.e,"venus","home",5186001,5187001);fails([&]{assemble_routes(f.s,f.e,q);},"flyby time");q=f.q;
    q.venus_home[0]=leg(f.s,f.e,"venus","home",5186000,5187100);auto no_speed=assemble_routes(f.s,f.e,q);check(no_speed.routes.empty(),"speed mismatch rejects");q=f.q;
    q.venus_safety_margin_m=5e7;q.venus_maximum_periapsis_m=1e9;auto unsafe=assemble_routes(f.s,f.e,q);check(unsafe.routes.empty(),"unsafe flyby rejects");q=f.q;
    q.venus_atmosphere_altitude_m.reset();fails([&]{assemble_routes(f.s,f.e,q);},"atmosphere");q=f.q;
    q.home_parking_altitude_m=-1;fails([&]{assemble_routes(f.s,f.e,q);},"altitude");q=f.q;
    q.home_mars[0].snapshot_hash="other";fails([&]{assemble_routes(f.s,f.e,q);},"snapshot_hash");q=f.q;
    q.max_combinations=0;fails([&]{assemble_routes(f.s,f.e,q);},"combination");q=f.q;
    q.max_routes=1001;fails([&]{assemble_routes(f.s,f.e,q);},"route limit");q=f.q;
    q.launch_start_ut_s=5;q.launch_end_ut_s=10;auto none=assemble_routes(f.s,f.e,q);check(none.routes.empty()&&none.diagnostic=="no screened route in window","no route window");q=f.q;
    q.return_condition=ReturnCondition::entry;fails([&]{assemble_routes(f.s,f.e,q);},"entry");
    q=f.q;q.home_mars[0].departure_barycentric_velocity_mps.x+=1000;fails([&]{assemble_routes(f.s,f.e,q);},"Lambert");
    q=f.q;q.home_mars[0].flight_time_s=-1000;fails([&]{assemble_routes(f.s,f.e,q);},"leg time");
    q=f.q;q.time_tolerance_s=1;q.venus_home[0]=leg(f.s,f.e,"venus","home",5186000.5,5187000.5);
    fails([&]{assemble_routes(f.s,f.e,q);},"flyby time");
    q=f.q;q.home_mars[0].ephemeris_metadata.step_s=1;fails([&]{assemble_routes(f.s,f.e,q);},"ephemeris identity");
}
void positive(){Fixture f;auto result=assemble_routes(f.s,f.e,f.q),again=assemble_routes(f.s,f.e,f.q);
    check(result.routes.size()==1&&again.routes.size()==1,"one route");
    const auto& route=result.routes[0];check(route.result_label=="patched_conic_screened_route","screen label");
    const auto burn=[](double vinf,double mu,double radius){return std::sqrt(vinf*vinf+2*mu/radius)-std::sqrt(mu/radius);};
    const double v0=norm(f.q.home_mars[0].departure_barycentric_velocity_mps),v1=norm(f.q.home_mars[0].arrival_barycentric_velocity_mps);
    const double v2=norm(f.q.mars_venus[0].departure_barycentric_velocity_mps),v3=norm(f.q.venus_home[0].arrival_barycentric_velocity_mps);
    check(std::abs(route.home_injection_mps-burn(v0,1e12,2e5))<1e-9,"home injection");
    check(std::abs(route.mars_capture_mps-burn(v1,2e12,3e5))<1e-9,"mars capture");
    check(std::abs(route.mars_departure_mps-burn(v2,2e12,3e5))<1e-9,"mars departure");
    check(std::abs(route.home_return_capture_mps-burn(v3,1e12,2e5))<1e-9,"home return capture");
    check(std::abs(route.departure_c3_m2_s2-v0*v0)<1e-6&&std::abs(route.return_c3_m2_s2-v3*v3)<1e-6,"C3 convention");
    check(route.total_optimistic_delta_v_mps==route.home_injection_mps+route.mars_capture_mps+route.mars_departure_mps+route.home_return_capture_mps,"burn sum");
    check(route.flyby.required_turn_rad>0&&route.flyby.turn_margin_rad>=0,"flyby geometry");
    check(route.route_id==again.routes[0].route_id&&route.total_optimistic_delta_v_mps==again.routes[0].total_optimistic_delta_v_mps,"repeatability");
    check(route.stay_type=="parking_orbit"&&route.fixed_stay_s==5184000,"parking stay explicit");
    auto perturbed=f.q;
    perturbed.home_mars[0].departure_barycentric_velocity_mps.x+=5e-4;
    perturbed.venus_home[0].departure_barycentric_velocity_mps.x+=5e-4;
    const auto canonical=assemble_routes(f.s,f.e,perturbed);
    check(canonical.routes.size()==1,"within-tolerance perturbation retains verified route");
    check(canonical.routes[0].home_mars.departure_barycentric_velocity_mps.x==route.home_mars.departure_barycentric_velocity_mps.x&&
          canonical.routes[0].venus_home.departure_barycentric_velocity_mps.x==route.venus_home.departure_barycentric_velocity_mps.x,
          "retained legs use re-solved Lambert velocities");
    check(canonical.routes[0].home_mars.departure_vinf_mps==route.home_mars.departure_vinf_mps&&
          canonical.routes[0].home_mars.screening_score_mps==route.home_mars.screening_score_mps&&
          canonical.routes[0].leg_verification.find("canonicalized")!=std::string::npos,
          "verified v-infinity, score and label retained");
    check(canonical.routes[0].home_injection_mps==route.home_injection_mps&&
          canonical.routes[0].flyby.minimum_periapsis_m==route.flyby.minimum_periapsis_m,"burn and flyby use verified values");
    auto long_leg=f.q;
    long_leg.home_mars[0]=leg(f.s,f.e,"home","mars",0,5000);
    long_leg.mars_venus[0]=leg(f.s,f.e,"mars","venus",5189000,5190000);
    long_leg.venus_home[0]=leg(f.s,f.e,"venus","home",5190000,5191000);
    const auto long_reference=assemble_routes(f.s,f.e,long_leg);
    check(long_reference.routes.size()==1,"long dated leg route exists");
    long_leg.home_mars[0].departure_barycentric_velocity_mps.x+=5e-4;
    const auto long_canonical=assemble_routes(f.s,f.e,long_leg);
    check(long_canonical.routes.size()==1&&
          long_canonical.routes[0].home_mars.departure_barycentric_velocity_mps.x==long_reference.routes[0].home_mars.departure_barycentric_velocity_mps.x&&
          long_canonical.routes[0].home_injection_mps==long_reference.routes[0].home_injection_mps,
          "five-thousand-second leg uses canonical departure velocity");
    auto q=f.q;
    q.home_mars.push_back(leg(f.s,f.e,"home","mars",10,1010));
    q.mars_venus.push_back(leg(f.s,f.e,"mars","venus",5185010,5186010));
    q.venus_home.push_back(leg(f.s,f.e,"venus","home",5186010,5187010));
    const auto all=assemble_routes(f.s,f.e,q);check(all.routes.size()==2&&all.routes[0].total_optimistic_delta_v_mps<=all.routes[1].total_optimistic_delta_v_mps,"objective order");
    q.max_routes=1;const auto capped=assemble_routes(f.s,f.e,q);
    check(capped.routes.size()==1&&capped.routes[0].route_id==all.routes[0].route_id,"bounded top K retains best");
    check(capped.peak_retained_routes<=1&&all.peak_retained_routes<=5,"bounded storage during enumeration");
    q=f.q;q.max_combinations=1000;q.max_routes=1;
    for(int i=1;i<10;++i){q.home_mars.push_back(q.home_mars[0]);q.mars_venus.push_back(q.mars_venus[0]);q.venus_home.push_back(q.venus_home[0]);}
    const auto stress=assemble_routes(f.s,f.e,q);
    check(stress.considered_combinations==1000&&stress.routes.size()==1&&stress.peak_retained_routes==1,"bounded thousand-combination retention");
    std::cout<<std::setprecision(15)<<"home injection m/s="<<route.home_injection_mps<<" Mars capture m/s="<<route.mars_capture_mps
             <<" Mars departure m/s="<<route.mars_departure_mps<<" home return capture m/s="<<route.home_return_capture_mps
             <<" optimistic total m/s="<<route.total_optimistic_delta_v_mps
             <<" departure C3 m2/s2="<<route.departure_c3_m2_s2<<" return C3 m2/s2="<<route.return_c3_m2_s2
             <<" flyby periapsis m="<<route.flyby.minimum_periapsis_m<<'\n';
}
}
int main(){try{failures();positive();std::cout<<"PASS "<<checks<<" route checks\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
