#include "Refine.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ksp;
namespace {
int checks=0;
void check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
template<class F> void fails(F call,const char* fragment){
    try{call();}catch(const SpacecraftError& e){check(std::string(e.what()).find(fragment)!=std::string::npos,"wrong diagnostic");return;}
    throw std::runtime_error(fragment);
}
Snapshot system(){Snapshot s;s.analysis_ready=true;s.confidence="synthetic_fixture";s.snapshot_hash="refine-fixture";
    s.state_epoch_ut_s=0;s.frame={"barycenter","X,Y,Z","right",true};
    s.bodies={{"primary",3.986004418e14,1e6,State{{0,0,0},{0,0,0}},0},
              {"remote",1,1e3,State{{1e12,0,0},{0,0,0}},0}};return s;}
Ephemeris planets(const Snapshot& s){Settings p;p.start_ut_s=0;p.end_ut_s=3000;p.step_s=100;
    p.max_position_fit_error_m=0.002;p.max_velocity_fit_error_mps=2e-6;return integrate(s,p);}
RefineRequest request(){const double mu=3.986004418e14,r=1e7,t=2500,v=std::sqrt(mu/r),a=v*t/r;
    RefineRequest q;q.initial_state={{r,0,0},{0,v,0}};
    q.target_position_m={r*std::cos(a),r*std::sin(a),0};q.seed_impulse_mps={0.5,-0.3,0.2};
    q.propagation.start_ut_s=0;q.propagation.end_ut_s=t;
    q.propagation.abs_position_tolerance_m=0.01;q.propagation.abs_velocity_tolerance_mps=1e-5;
    q.propagation.relative_tolerance=1e-11;q.propagation.min_step_s=1e-4;q.propagation.max_step_s=300;
    q.propagation.atmosphere_boundaries={{"primary",0},{"remote",0}};
    q.target_position_tolerance_m=0.1;q.finite_difference_step_mps=0.001;
    q.maximum_impulse_mps=10;q.strict_disagreement_limit_m=0.1;q.maximum_iterations=8;return q;}
State rk4(State y,double mu,double duration){const int steps=20000;const double h=duration/steps;
    auto derivative=[&](State z){const double r=norm(z.position_m);return State{z.velocity_mps,z.position_m*(-mu/(r*r*r))};};
    auto add=[](State a,State b,double scale){return State{a.position_m+b.position_m*scale,a.velocity_mps+b.velocity_mps*scale};};
    for(int i=0;i<steps;++i){auto a=derivative(y),b=derivative(add(y,a,h/2)),c=derivative(add(y,b,h/2)),d=derivative(add(y,c,h));
        y.position_m=y.position_m+(a.position_m+b.position_m*2+c.position_m*2+d.position_m)*(h/6);
        y.velocity_mps=y.velocity_mps+(a.velocity_mps+b.velocity_mps*2+c.velocity_mps*2+d.velocity_mps)*(h/6);
    }return y;}
void failures(){auto s=system();auto e=planets(s);auto q=request();
    auto degraded=e;degraded.metadata.max_position_fit_error_m=100;
    fails([&]{refine_terminal_position(s,degraded,q);},"ephemeris accuracy budget");
    degraded=e;for(auto& state:degraded.samples[0])state.position_m.x+=1000;
    fails([&]{refine_terminal_position(s,degraded,q);},"ephemeris convergence");
    q.target_position_m.x=std::numeric_limits<double>::quiet_NaN();fails([&]{refine_terminal_position(s,e,q);},"target_position_m");
    q=request();q.maximum_iterations=0;fails([&]{refine_terminal_position(s,e,q);},"maximum_iterations");
    q=request();q.maximum_impulse_mps=0.1;fails([&]{refine_terminal_position(s,e,q);},"maximum_impulse_mps");
    q=request();q.propagation.burns={{100,{1,0,0}}};fails([&]{refine_terminal_position(s,e,q);},"preexisting burns");
    q=request();q.initial_state.position_m={5e5,0,0};fails([&]{refine_terminal_position(s,e,q);},"unsafe seed");
    q=request();q.maximum_iterations=1;q.seed_impulse_mps={5,-3,2};
    fails([&]{refine_terminal_position(s,e,q);},"did not converge");
    q=request();q.strict_disagreement_limit_m=1e-5;
    fails([&]{refine_terminal_position(s,e,q);},"ephemeris accuracy budget");
    q=request();s.snapshot_hash="wrong";fails([&]{refine_terminal_position(s,e,q);},"snapshot_hash");
}
void known_transfer(){auto s=system();auto e=planets(s);auto q=request();
    auto result=refine_terminal_position(s,e,q);
    check(result.trajectory.success&&result.strict_trajectory.success,"both propagations succeed");
    check(result.position_residual_m<q.target_position_tolerance_m,"terminal target reached");
    check(result.strict_disagreement_m<q.strict_disagreement_limit_m,"tighter repropagation agrees");
    check(result.ephemeris_step_disagreement_m<0.01,"planetary step convergence");
    check(result.result_label=="terminal_position_targeted_only","position-only result label");
    check(norm(result.impulse_mps)<0.02,"known zero-burn solution recovered");
    check(result.trajectory.burns.size()==1&&result.strict_trajectory.burns.size()==1,"one explicit impulse");
    auto initial=q.initial_state;initial.velocity_mps=initial.velocity_mps+result.impulse_mps;
    auto independent=rk4(initial,3.986004418e14,q.propagation.end_ut_s);
    check(norm(independent.position_m-q.target_position_m)<0.1,"independent RK4 target comparison");
    std::cout<<"refined miss m="<<result.position_residual_m<<" strict disagreement m="<<result.strict_disagreement_m
             <<" impulse m/s="<<result.impulse_magnitude_mps<<" iterations="<<result.iterations<<" evaluations="<<result.evaluations
             <<" independent RK4 miss m="<<norm(independent.position_m-q.target_position_m)<<'\n';
}
}
int main(){try{failures();known_transfer();std::cout<<"PASS "<<checks<<" checks\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
