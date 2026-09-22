#include "Search.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ksp;
namespace {
int checks=0;
void check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
template<class F> void fails(F call,const char* fragment){
    try{call();}catch(const SearchError& e){check(std::string(e.what()).find(fragment)!=std::string::npos,"wrong failure diagnostic");return;}
    throw std::runtime_error(fragment);
}
LambertRequest quarter(TransferBranch branch,AngularMomentumDirection direction,double time){
    LambertRequest q;q.departure_relative_position_m={1e7,0,0};q.arrival_relative_position_m={0,1e7,0};
    q.reference_normal={0,0,1};q.mu_m3_s2=3.986004418e14;q.time_of_flight_s=time;
    q.branch=branch;q.direction=direction;q.max_position_residual_m=0.1;q.max_velocity_residual_mps=1e-4;return q;
}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
State independent_derivative(State y,double mu){
    const double squared=dot(y.position_m,y.position_m);
    return {y.velocity_mps,y.position_m*(-mu/(squared*std::sqrt(squared)))};
}
State sum(State y,State derivative,double h){return {y.position_m+derivative.position_m*h,y.velocity_mps+derivative.velocity_mps*h};}
State independent_rk4(State y,double mu,double time){
    const int n=20000;const double h=time/n;
    for(int i=0;i<n;++i){
        auto k1=independent_derivative(y,mu),k2=independent_derivative(sum(y,k1,h/2),mu);
        auto k3=independent_derivative(sum(y,k2,h/2),mu),k4=independent_derivative(sum(y,k3,h),mu);
        y.position_m=y.position_m+(k1.position_m+k2.position_m*2+k3.position_m*2+k4.position_m)*(h/6);
        y.velocity_mps=y.velocity_mps+(k1.velocity_mps+k2.velocity_mps*2+k3.velocity_mps*2+k4.velocity_mps)*(h/6);
    }return y;
}
void lambert_failures(){
    const double period=2*std::acos(-1.0)*std::sqrt(1e21/3.986004418e14);
    auto q=quarter(TransferBranch::short_path,AngularMomentumDirection::positive,period/4);
    q.mu_m3_s2=0;fails([&]{solve_lambert(q);},"mu_m3_s2");
    q=quarter(TransferBranch::short_path,AngularMomentumDirection::positive,0);fails([&]{solve_lambert(q);},"time_of_flight_s");
    q=quarter(TransferBranch::short_path,AngularMomentumDirection::positive,period/4);q.departure_relative_position_m.x=std::numeric_limits<double>::quiet_NaN();
    fails([&]{solve_lambert(q);},"departure_relative_position_m");
    q=quarter(TransferBranch::short_path,AngularMomentumDirection::positive,period/4);q.arrival_relative_position_m=q.departure_relative_position_m;
    fails([&]{solve_lambert(q);},"coincident");
    q=quarter(TransferBranch::short_path,AngularMomentumDirection::positive,period/4);q.arrival_relative_position_m={-1e7,0,0};
    fails([&]{solve_lambert(q);},"collinear");
    q=quarter(TransferBranch::short_path,AngularMomentumDirection::positive,period/4);q.arrival_relative_position_m={-1e7,1e-3,0};
    fails([&]{solve_lambert(q);},"collinear");
    q=quarter(TransferBranch::short_path,AngularMomentumDirection::negative,period/4);
    fails([&]{solve_lambert(q);},"direction");
    q=quarter(TransferBranch::short_path,AngularMomentumDirection::positive,period/4);q.max_position_residual_m=1e-20;
    fails([&]{solve_lambert(q);},"residual");
    q=quarter(TransferBranch::short_path,AngularMomentumDirection::positive,period/4);q.max_iterations=1;
    fails([&]{solve_lambert(q);},"convergence");
}
void lambert_circular(){
    constexpr double mu=3.986004418e14,r=1e7;const double speed=std::sqrt(mu/r);
    const double period=2*std::acos(-1.0)*r/speed;
    auto a=solve_lambert(quarter(TransferBranch::short_path,AngularMomentumDirection::positive,period/4));
    check(norm(a.departure_relative_velocity_mps-Vec3{0,speed,0})<1e-3,"short initial circular velocity");
    check(norm(a.arrival_relative_velocity_mps-Vec3{-speed,0,0})<1e-3,"short final circular velocity");
    check(a.position_residual_m<0.1&&a.velocity_residual_mps<1e-4,"short residual");
    auto b=solve_lambert(quarter(TransferBranch::long_path,AngularMomentumDirection::negative,period*3/4));
    check(norm(b.departure_relative_velocity_mps-Vec3{0,-speed,0})<1e-3,"long initial circular velocity");
    check(norm(b.arrival_relative_velocity_mps-Vec3{speed,0,0})<1e-3,"long final circular velocity");
    check(b.position_residual_m<0.1&&b.velocity_residual_mps<1e-4,"long residual");
    auto short_reference=independent_rk4({{r,0,0},a.departure_relative_velocity_mps},mu,period/4);
    auto long_reference=independent_rk4({{r,0,0},b.departure_relative_velocity_mps},mu,period*3/4);
    check(norm(short_reference.position_m-Vec3{0,r,0})<0.01&&norm(short_reference.velocity_mps-a.arrival_relative_velocity_mps)<1e-5,"short independent RK4");
    check(norm(long_reference.position_m-Vec3{0,r,0})<0.01&&norm(long_reference.velocity_mps-b.arrival_relative_velocity_mps)<1e-5,"long independent RK4");
    auto again=solve_lambert(quarter(TransferBranch::short_path,AngularMomentumDirection::positive,period/4));
    check(norm(again.departure_relative_velocity_mps-a.departure_relative_velocity_mps)==0&&again.iterations==a.iterations,"deterministic solution");
    std::cout<<"short residual m="<<a.position_residual_m<<" m/s="<<a.velocity_residual_mps<<" time s="<<a.time_residual_s<<" iterations="<<a.iterations
             <<" RK4 m="<<norm(short_reference.position_m-Vec3{0,r,0})
             <<" long residual m="<<b.position_residual_m<<" m/s="<<b.velocity_residual_mps<<" time s="<<b.time_residual_s
             <<" iterations="<<b.iterations<<" RK4 m="<<norm(long_reference.position_m-Vec3{0,r,0})<<'\n';
    auto asymmetric=quarter(TransferBranch::short_path,AngularMomentumDirection::positive,4000);
    asymmetric.arrival_relative_position_m={0,1.4e7,0};auto transfer=solve_lambert(asymmetric);
    auto independent=independent_rk4({asymmetric.departure_relative_position_m,transfer.departure_relative_velocity_mps},mu,asymmetric.time_of_flight_s);
    check(norm(independent.position_m-asymmetric.arrival_relative_position_m)<0.01&&norm(independent.velocity_mps-transfer.arrival_relative_velocity_mps)<1e-5,"asymmetric conic RK4");
    std::cout<<"asymmetric RK4 endpoint m="<<norm(independent.position_m-asymmetric.arrival_relative_position_m)<<'\n';
    asymmetric.branch=TransferBranch::long_path;
    asymmetric.direction=AngularMomentumDirection::negative;
    asymmetric.time_of_flight_s=period*0.9;
    auto long_transfer=solve_lambert(asymmetric);
    auto long_independent=independent_rk4({asymmetric.departure_relative_position_m,long_transfer.departure_relative_velocity_mps},mu,asymmetric.time_of_flight_s);
    check(norm(long_independent.position_m-asymmetric.arrival_relative_position_m)<0.01&&norm(long_independent.velocity_mps-long_transfer.arrival_relative_velocity_mps)<1e-5,"asymmetric long conic RK4");
}
void flyby_cases(){
    FlybyRequest q;q.incoming_vinf_mps={3000,0,0};q.outgoing_vinf_mps={3000*std::cos(0.5),3000*std::sin(0.5),0};
    q.mu_planet_m3_s2=3.986004418e14;q.radius_m=6e6;q.atmosphere_altitude_m=1e5;q.safety_margin_m=1e5;q.speed_tolerance_mps=0.01;q.maximum_periapsis_m=2e8;
    auto result=screen_unpowered_flyby(q);
    check(std::abs(result.required_turn_rad-0.5)<1e-12,"required turn");
    check(result.minimum_periapsis_m>q.radius_m+*q.atmosphere_altitude_m+q.safety_margin_m,"periapsis margin");
    check(result.speed_mismatch_mps<1e-9&&result.speed_margin_mps>0,"speed margin");
    auto same=screen_unpowered_flyby(q);check(same.minimum_periapsis_m==result.minimum_periapsis_m,"deterministic flyby");
    q.outgoing_vinf_mps=q.outgoing_vinf_mps*(1+1e-6);
    auto within_tolerance=screen_unpowered_flyby(q);
    check(within_tolerance.speed_mismatch_mps>0&&within_tolerance.speed_margin_mps>0,"named speed tolerance");
    q.outgoing_vinf_mps={3000*std::cos(0.5),3000*std::sin(0.5),0};
    std::cout<<"flyby turn rad="<<result.required_turn_rad<<" max="<<result.maximum_turn_rad
             <<" rp m="<<result.minimum_periapsis_m<<" speed mismatch m/s="<<result.speed_mismatch_mps<<'\n';
    q.atmosphere_altitude_m.reset();fails([&]{screen_unpowered_flyby(q);},"atmosphere");
    q.atmosphere_altitude_m=1e5;q.outgoing_vinf_mps={3010,0,0};fails([&]{screen_unpowered_flyby(q);},"speed mismatch");
    q.outgoing_vinf_mps=q.incoming_vinf_mps;fails([&]{screen_unpowered_flyby(q);},"zero turn");
    q.outgoing_vinf_mps={3000*std::cos(0.2),3000*std::sin(0.2),0};fails([&]{screen_unpowered_flyby(q);},"encounter limit");
    q.outgoing_vinf_mps={3000*std::cos(2.5),3000*std::sin(2.5),0};fails([&]{screen_unpowered_flyby(q);},"periapsis");
    q.outgoing_vinf_mps={0,0,0};fails([&]{screen_unpowered_flyby(q);},"outgoing_vinf_mps");
    q.outgoing_vinf_mps={3000,0,0};q.safety_margin_m=-1;fails([&]{screen_unpowered_flyby(q);},"safety_margin_m");
    q.safety_margin_m=0;q.radius_m=1e308;q.atmosphere_altitude_m=1e308;
    fails([&]{screen_unpowered_flyby(q);},"clearance");
}
}
int main(){try{lambert_failures();lambert_circular();flyby_cases();std::cout<<"PASS "<<checks<<" checks\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
