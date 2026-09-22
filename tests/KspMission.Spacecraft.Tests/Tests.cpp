#include "Spacecraft.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ksp;
namespace {
int checks=0;
void check(bool x,const char* label){++checks;if(!x)throw std::runtime_error(label);}
template<class F> void fails(F f,const char* field){try{f();}catch(const SpacecraftError& e){check(std::string(e.what()).find(field)!=std::string::npos,"wrong diagnostic");return;}throw std::runtime_error(field);}
Snapshot system(double end){
    Snapshot s;s.analysis_ready=true;s.confidence="synthetic_fixture";s.snapshot_hash="sc-fixture";
    s.state_epoch_ut_s=0;s.frame={"barycenter","X,Y,Z","right",true};
    s.bodies={{"primary",3.986004418e14,1e6,State{{0,0,0},{0,0,0}},0},
              {"remote",1,1e3,State{{1e12,0,0},{0,0,0}},0}};
    return s;
}
Ephemeris planets(const Snapshot& s,double end){
    ksp::Settings p;p.start_ut_s=0;p.end_ut_s=end;p.step_s=100;
    p.max_position_fit_error_m=100;p.max_velocity_fit_error_mps=0.01;return integrate(s,p);
}
SpacecraftSettings options(double end){SpacecraftSettings x;x.start_ut_s=0;x.end_ut_s=end;x.abs_position_tolerance_m=0.01;x.abs_velocity_tolerance_mps=1e-5;
    x.relative_tolerance=1e-11;x.min_step_s=1e-4;x.max_step_s=300;x.safety_margin_m=0;
    x.atmosphere_boundaries={{"primary",0},{"remote",0}};return x;}
State independent_reference(const Snapshot&,const Ephemeris&,State,double,double,double);
void gates(){
    auto s=system(1000);auto e=planets(s,1000);auto q=options(1000);State y{{1e7,0,0},{0,6300,0}};
    s.analysis_ready=false;fails([&]{propagate(s,e,y,q);},"analysis_ready");
    s=system(1000);s.snapshot_hash="wrong";fails([&]{propagate(s,e,y,q);},"snapshot_hash");
    s=system(1000);s.frame.axes="wrong";fails([&]{propagate(s,e,y,q);},"frame");
    s=system(1000);s.state_epoch_ut_s=1;fails([&]{propagate(s,e,y,q);},"epoch");
    s=system(1000);s.bodies[1].state.reset();fails([&]{propagate(s,e,y,q);},"body state");
    s=system(1000);auto bad=e;bad.metadata.force_model="other";fails([&]{propagate(s,bad,y,q);},"force_model");
    bad=e;bad.metadata.end_ut_s=900;fails([&]{propagate(s,bad,y,q);},"coverage");
    bad=e;bad.metadata.measured_max_position_error_m=std::numeric_limits<double>::quiet_NaN();fails([&]{propagate(s,bad,y,q);},"ephemeris fit");
    y.position_m.x=std::numeric_limits<double>::quiet_NaN();fails([&]{propagate(s,e,y,q);},"position_m");
    y={{1e7,0,0},{0,6300,0}};q.safety_margin_m=-1;fails([&]{propagate(s,e,y,q);},"safety_margin_m");
    q=options(1000);q.abs_position_tolerance_m=0;fails([&]{propagate(s,e,y,q);},"tolerance");
    q=options(1000);q.min_step_s=0;fails([&]{propagate(s,e,y,q);},"step");
    q=options(1000);q.end_ut_s=1100;fails([&]{propagate(s,e,y,q);},"coverage");
    q=options(1000);q.burns={{1100,{1,0,0}}};fails([&]{propagate(s,e,y,q);},"burn");
    q=options(1000);q.min_step_s=300;q.max_step_s=300;q.abs_position_tolerance_m=1e-18;q.abs_velocity_tolerance_mps=1e-18;
    try{propagate(s,e,y,q);throw std::runtime_error("minimum step not rejected");}
    catch(const SpacecraftError& error){check(std::string(error.what()).find("minimum step")!=std::string::npos&&error.last_valid_ut_s.has_value()&&error.last_valid_state.has_value(),"minimum step last state");}
    q=options(1000);q.atmosphere_boundaries={{"primary",-1}};fails([&]{propagate(s,e,y,q);},"atmosphere altitude_m");
    q=options(1000);q.atmosphere_boundaries.pop_back();fails([&]{propagate(s,e,y,q);},"missing atmosphere");
}
void circular_and_burn(){
    const double mu=3.986004418e14,r=1e7,v=std::sqrt(mu/r),period=2*std::acos(-1.0)*r/v;
    const double end=std::ceil(period/100)*100;auto s=system(end);auto e=planets(s,end);auto q=options(end);
    State y{{r,0,0},{0,v,0}};auto out=propagate(s,e,y,q);
    check(out.success,"circular success");check(out.accepted_steps>0&&out.rejected_steps>=0,"step stats");
    check(out.max_accepted_normalized_error<=1&&out.max_accepted_position_local_error_m>=0&&out.ephemeris_metadata.snapshot_hash==s.snapshot_hash,"tolerance and ephemeris record");
    auto z=out.final_state;double angle=std::sqrt(mu/(r*r*r))*end;
    Vec3 ep{r*std::cos(angle),r*std::sin(angle),0},ev{-v*std::sin(angle),v*std::cos(angle),0};
    const auto pe=norm(z.position_m-ep),ve=norm(z.velocity_mps-ev);
    check(pe<10,"circular position");check(ve<0.01,"circular velocity");
    std::cout<<"circular error m="<<pe<<" m/s="<<ve<<" accepted="<<out.accepted_steps<<" rejected="<<out.rejected_steps
             <<" max local m="<<out.max_accepted_position_local_error_m<<" m/s="<<out.max_accepted_velocity_local_error_mps
             <<" normalized="<<out.max_accepted_normalized_error<<'\n';
    q=options(end);const double burn=1234.5;Vec3 dv{0,25,3};q.burns={{0,{1,0,0}},{burn,dv},{burn,{2,0,0}},{end,{0,1,0}}};
    out=propagate(s,e,y,q);check(out.success&&out.burns.size()==4,"burn count");
    for(const auto& b:out.burns){check(norm(b.after.position_m-b.before.position_m)<1e-9,"burn position continuity");
        check(norm((b.after.velocity_mps-b.before.velocity_mps)-b.delta_v_mps)<1e-9,"exact impulse");}
    check(out.burns[0].ut_s==0&&out.burns[1].ut_s==burn&&out.burns[2].ut_s==burn&&out.burns[3].ut_s==end,"ordered burn times");
    check(out.final_state.velocity_mps.y==out.burns.back().after.velocity_mps.y,"end burn applied");
    auto reference=y;reference.velocity_mps=reference.velocity_mps+Vec3{1,0,0};
    reference=independent_reference(s,e,reference,0,burn,0.5);
    reference.velocity_mps=reference.velocity_mps+dv+Vec3{2,0,0};
    reference=independent_reference(s,e,reference,burn,end,0.5);
    reference.velocity_mps=reference.velocity_mps+Vec3{0,1,0};
    const double burn_p=norm(reference.position_m-out.final_state.position_m),burn_v=norm(reference.velocity_mps-out.final_state.velocity_mps);
    check(burn_p<0.1&&burn_v<0.0001,"post-burn independent reference");
    std::cout<<"off-grid burn RK4 error m="<<burn_p<<" m/s="<<burn_v<<'\n';
}
void unsafe_and_event(){
    auto s=system(5000);auto e=planets(s,5000);auto q=options(5000);
    State y{{2e6,0,0},{-1000,0,0}};auto out=propagate(s,e,y,q);
    check(!out.success&&out.unsafe.has_value(),"unsafe detected");
    check(out.unsafe->body_id=="primary"&&out.unsafe->ut_s>0&&out.unsafe->ut_s<1000,"unsafe first crossing time");
    y={{5e5,0,0},{0,0,0}};out=propagate(s,e,y,q);check(!out.success&&out.unsafe&&out.unsafe->ut_s==0,"initial unsafe");
    y={{1.2e6,0,0},{0,0,0}};q.atmosphere_boundaries={{"primary",3e5},{"remote",0}};
    out=propagate(s,e,y,q);check(!out.success&&out.unsafe&&out.unsafe->ut_s==0,"atmosphere unsafe");
    q=options(5000);
    y={{2e6,0,0},{-1000,0,0}};q.burns={{2000,{1,0,0}}};out=propagate(s,e,y,q);
    check(!out.success&&out.unsafe&&out.burns.empty(),"burn after unsafe not applied");
    // Straight near-pass near a weak remote body: minimum falls between 300 s grid points.
    s=system(2000);s.bodies[0].mu_m3_s2=1;s.bodies[0].radius_m=100;e=planets(s,2000);q=options(2000);q.max_step_s=300;
    y={{-1000,2000,0},{2,0,0}};out=propagate(s,e,y,q);
    check(out.success&& !out.closest_approaches.empty(),"closest approach detected");
    auto it=std::find_if(out.closest_approaches.begin(),out.closest_approaches.end(),[](auto& x){return x.body_id=="primary";});
    check(it!=out.closest_approaches.end()&&std::abs(it->ut_s-500)<0.1,"off-grid closest time");
    check(it!=out.closest_approaches.end()&&std::abs(it->distance_m-2000)<0.1,"closest distance");
    y={{-1000,0,0},{2,0,0}};out=propagate(s,e,y,q);
    check(!out.success&&out.unsafe&&std::abs(out.unsafe->ut_s-450)<0.3,
          (std::string("between-step unsafe time ")+(out.unsafe?std::to_string(out.unsafe->ut_s):"missing")).c_str());
    s.bodies[0].mu_m3_s2=1e-6;e=planets(s,2000);q=options(2000);
    q.abs_position_tolerance_m=1e6;q.abs_velocity_tolerance_mps=1e3;q.relative_tolerance=0.1;
    y={{-1000,0,0},{100,0,0}};out=propagate(s,e,y,q);
    check(!out.success&&out.unsafe&&std::abs(out.unsafe->ut_s-9)<0.1,"fast between-sample unsafe entry");
}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
void eccentric_and_hyperbolic(){
    constexpr double mu=3.986004418e14,rp=1e7,ecc=0.3;
    const double v=std::sqrt(mu*(1+ecc)/rp),a=rp/(1-ecc);
    const double period=2*std::acos(-1.0)*std::sqrt(a*a*a/mu),coverage=std::ceil(period/100)*100;
    auto s=system(coverage);auto ep=planets(s,coverage);auto q=options(period);State y{{rp,0,0},{0,v,0}};
    auto out=propagate(s,ep,y,q);double pos=norm(out.final_state.position_m-y.position_m),vel=norm(out.final_state.velocity_mps-y.velocity_mps);
    check(out.success&&pos<1&&vel<0.001,"eccentric Kepler period");
    std::cout<<"eccentric period error m="<<pos<<" m/s="<<vel<<'\n';
    s=system(30000);ep=planets(s,30000);q=options(30000);y={{-1e8,1e7,0},{10000,0,0}};
    const auto orbit=[&](State z){auto h=cross(z.position_m,z.velocity_mps);
        auto ex=cross(z.velocity_mps,h)*(1/mu)-z.position_m*(1/norm(z.position_m));
        double energy=dot(z.velocity_mps,z.velocity_mps)/2-mu/norm(z.position_m);
        return std::array<double,3>{std::sqrt(2*energy),norm(ex),std::atan2(ex.y,ex.x)};};
    auto before=orbit(y);out=propagate(s,ep,y,q);auto after=orbit(out.final_state);
    const double turn_before=2*std::asin(1/before[1]),turn_after=2*std::asin(1/after[1]);
    check(out.success,"hyperbolic pass success");
    check(std::abs(after[0]-before[0])<0.001,"hyperbolic excess speed");
    check(std::abs(turn_after-turn_before)<1e-6,"hyperbolic turning angle");
    check(std::abs(after[2]-before[2])<1e-6,"hyperbolic periapsis direction");
    std::cout<<"hyperbolic v-infinity error m/s="<<std::abs(after[0]-before[0])<<" turn error rad="<<std::abs(turn_after-turn_before)<<'\n';
    s.bodies[0].radius_m=8e6;ep=planets(s,30000);
    out=propagate(s,ep,y,q);check(!out.success&&out.unsafe,"hyperbolic clearance rejected");
}
State independent_derivative(const Snapshot& s,const Ephemeris& e,double t,State y){
    State z;z.position_m=y.velocity_mps;
    for(std::size_t j=0;j<s.bodies.size();++j){
        const auto body=e.query(s.bodies[j].id,t);
        const auto d=body.position_m-y.position_m;
        const double squared=dot(d,d);
        z.velocity_mps=z.velocity_mps+d*(s.bodies[j].mu_m3_s2/(squared*std::sqrt(squared)));
    }return z;
}
State sum(State a,State b,double h){return {a.position_m+b.position_m*h,a.velocity_mps+b.velocity_mps*h};}
State independent_reference(const Snapshot& s,const Ephemeris& e,State y,double start,double end,double h){
    for(double t=start;t<end;t+=h){
        const auto k1=independent_derivative(s,e,t,y);
        const auto k2=independent_derivative(s,e,t+h/2,sum(y,k1,h/2));
        const auto k3=independent_derivative(s,e,t+h/2,sum(y,k2,h/2));
        const auto k4=independent_derivative(s,e,t+h,sum(y,k3,h));
        y.position_m=y.position_m+(k1.position_m+k2.position_m*2+k3.position_m*2+k4.position_m)*(h/6);
        y.velocity_mps=y.velocity_mps+(k1.velocity_mps+k2.velocity_mps*2+k3.velocity_mps*2+k4.velocity_mps)*(h/6);
    }return y;
}
void moving_three_body(){
    auto s=system(5000);s.snapshot_hash="moving-3";
    s.bodies[1]={"secondary",1e10,1e5,State{{1e8,0,0},{0,2000,0}},0};
    s.bodies.push_back({"third",2e9,1e5,State{{0,-1.4e8,0},{1500,0,0}},0});
    auto e=planets(s,5000);auto q=options(5000);q.atmosphere_boundaries={{"primary",0},{"secondary",0},{"third",0}};
    State initial{{4e7,2e7,0},{-800,1600,0}};
    auto actual=propagate(s,e,initial,q);auto reference=independent_reference(s,e,initial,0,5000,1);
    const double p=norm(actual.final_state.position_m-reference.position_m),v=norm(actual.final_state.velocity_mps-reference.velocity_mps);
    check(actual.success&&p<0.1&&v<0.0001,"moving three body independent reference");
    std::cout<<"moving three-body RK4 error m="<<p<<" m/s="<<v
             <<" ephemeris fit m="<<e.metadata.measured_max_position_error_m
             <<" m/s="<<e.metadata.measured_max_velocity_error_mps<<'\n';
}
void translated_frame(){
    auto s=system(3000);auto e=planets(s,3000);auto q=options(3000);
    q.abs_position_tolerance_m=1e-4;
    State initial{{1e7,0,0},{0,6300,0}};
    auto baseline=propagate(s,e,initial,q);
    const Vec3 offset{1e11,-2e11,3e11};
    s.snapshot_hash="sc-fixture-translated";
    for(auto& body:s.bodies)body.state->position_m=body.state->position_m+offset;
    initial.position_m=initial.position_m+offset;
    auto moved=propagate(s,planets(s,3000),initial,q);
    check(baseline.success&&moved.success,"translated frame successes");
    check(std::abs(static_cast<long long>(baseline.accepted_steps)-static_cast<long long>(moved.accepted_steps))<=2,
          "adaptive steps should not depend on frame translation");
    check(norm((moved.final_state.position_m-offset)-baseline.final_state.position_m)<0.1,
          "translated frame final position");
}
void curved_between_sample_entry(){
    auto s=system(100);s.snapshot_hash="curved-entry";
    s.bodies[0].mu_m3_s2=1e-9;s.bodies[0].radius_m=10;
    s.bodies[0].state=State{{0,0,0},{67.5,0,0}};
    s.bodies[1].mu_m3_s2=1e-9;
    auto e=planets(s,100);
    // One Hermite ephemeris segment places the body at x=1000 m around UT 33 s.
    // At the quarter probes (UT 25 and 50 s) it is more than 10 m away.
    e.samples[0][0]=State{{0,0,0},{67.5,0,0}};
    e.samples[0][1]=State{{0,0,0},{0,0,0}};
    auto q=options(100);q.max_step_s=100;
    q.abs_position_tolerance_m=0.01;q.abs_velocity_tolerance_mps=1e-5;q.relative_tolerance=0.1;
    State y{{1000,0,0},{0,0,0}};
    auto out=propagate(s,e,y,q);
    check(!out.success&&out.unsafe.has_value(),"curved between-sample entry reported");
    double lo=0,hi=1.0/3;
    for(int k=0;k<60;++k){double u=(lo+hi)/2;double x=6750*u*(1-u)*(1-u);if(x<990)lo=u;else hi=u;}
    check(std::abs(out.unsafe->ut_s-100*hi)<0.1,
          (std::string("curved entry time bracketed ")+std::to_string(out.unsafe->ut_s)).c_str());
    q.abs_position_tolerance_m=1e6;q.abs_velocity_tolerance_mps=1e3;
    fails([&]{propagate(s,e,y,q);},"unsafe interval unresolved");
}
}
int main(){try{gates();circular_and_burn();unsafe_and_event();eccentric_and_hyperbolic();moving_three_body();translated_frame();curved_between_sample_entry();std::cout<<"PASS "<<checks<<" checks\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
