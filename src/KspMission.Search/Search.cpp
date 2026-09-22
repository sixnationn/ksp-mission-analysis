#include "Search.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace ksp {
namespace {
bool finite(double v){return std::isfinite(v);}bool finite(Vec3 v){return finite(v.x)&&finite(v.y)&&finite(v.z);}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
struct Stumpff {double c,s;};
Stumpff stumpff(double z){
    if(std::abs(z)<1e-5){
        const double c=0.5-z/24+z*z/720-z*z*z/40320;
        const double s=1.0/6-z/120+z*z/5040-z*z*z/362880;
        return {c,s};
    }
    if(z>0){const double root=std::sqrt(z);return {(1-std::cos(root))/z,(root-std::sin(root))/(root*root*root)};}
    const double root=std::sqrt(-z);return {(std::cosh(root)-1)/(-z),(std::sinh(root)-root)/(root*root*root)};
}
struct LambertTime {double seconds,y;bool valid;};
LambertTime time_at(double z,double r1,double r2,double A,double mu){
    const auto cs=stumpff(z);
    if(!finite(cs.c)||!finite(cs.s)||cs.c<=0)return {0,0,false};
    const double y=r1+r2+A*(z*cs.s-1)/std::sqrt(cs.c);
    if(!finite(y)||y<=0)return {0,y,false};
    const double x=std::sqrt(y/cs.c);
    const double time=(x*x*x*cs.s+A*std::sqrt(y))/std::sqrt(mu);
    return {time,y,finite(time)&&time>0};
}
State propagate_conic(Vec3 r0,Vec3 v0,double mu,double dt){
    const double radius=norm(r0),speed2=dot(v0,v0),alpha=2/radius-speed2/mu;
    const double rv=dot(r0,v0)/std::sqrt(mu);
    auto equation=[&](double chi){const auto cs=stumpff(alpha*chi*chi);
        return rv*chi*chi*cs.c+(1-alpha*radius)*chi*chi*chi*cs.s+radius*chi-std::sqrt(mu)*dt;};
    double high=std::max(1.0,std::sqrt(mu)*dt/radius);
    for(int i=0;i<64&&equation(high)<0;++i)high*=2;
    if(!finite(equation(high))||equation(high)<0)throw SearchError("conic residual propagation unbracketed");
    double low=0;
    for(int i=0;i<128;++i){double mid=(low+high)/2;if(equation(mid)<0)low=mid;else high=mid;}
    const double chi=(low+high)/2;const auto cs=stumpff(alpha*chi*chi);
    const double f=1-chi*chi*cs.c/radius;
    const double g=dt-chi*chi*chi*cs.s/std::sqrt(mu);
    const Vec3 position=r0*f+v0*g;const double end_radius=norm(position);
    const double fdot=std::sqrt(mu)/(end_radius*radius)*(alpha*chi*chi*chi*cs.s-chi);
    const double gdot=1-chi*chi*cs.c/end_radius;
    return {position,r0*fdot+v0*gdot};
}
}
LambertResult solve_lambert(const LambertRequest& q){
    if(!finite(q.departure_relative_position_m)||norm(q.departure_relative_position_m)==0)throw SearchError("departure_relative_position_m invalid");
    if(!finite(q.arrival_relative_position_m)||norm(q.arrival_relative_position_m)==0)throw SearchError("arrival_relative_position_m invalid");
    if(!finite(q.reference_normal)||norm(q.reference_normal)==0)throw SearchError("reference_normal invalid");
    if(!finite(q.mu_m3_s2)||q.mu_m3_s2<=0)throw SearchError("mu_m3_s2 invalid");
    if(!finite(q.time_of_flight_s)||q.time_of_flight_s<=0)throw SearchError("time_of_flight_s invalid");
    if(!finite(q.max_position_residual_m)||q.max_position_residual_m<=0||!finite(q.max_velocity_residual_mps)||q.max_velocity_residual_mps<=0)throw SearchError("residual bounds invalid");
    if(q.max_iterations==0)throw SearchError("convergence iteration limit invalid");
    const double r1=norm(q.departure_relative_position_m),r2=norm(q.arrival_relative_position_m);
    if(norm(q.arrival_relative_position_m-q.departure_relative_position_m)<=1e-12*std::max(r1,r2))throw SearchError("coincident endpoints");
    const Vec3 h=cross(q.departure_relative_position_m,q.arrival_relative_position_m);
    const double sine=norm(h)/(r1*r2);
    if(sine<1e-8)throw SearchError("collinear or near-antipodal geometry");
    const double alignment=dot(h,q.reference_normal)/(norm(h)*norm(q.reference_normal));
    if(std::abs(alignment)<1e-8)throw SearchError("reference_normal not aligned with transfer plane");
    const int path_sign=q.branch==TransferBranch::short_path?1:-1;
    const int direction_sign=q.direction==AngularMomentumDirection::positive?1:-1;
    if((alignment>0?1:-1)*path_sign!=direction_sign)throw SearchError("branch and angular momentum direction disagree");
    const double cosine=std::clamp(dot(q.departure_relative_position_m,q.arrival_relative_position_m)/(r1*r2),-1.0,1.0);
    const double A=path_sign*std::sqrt(r1*r2*(1+cosine));
    double upper=4*std::numbers::pi*std::numbers::pi*(1-1e-8);
    double lower=-100;
    for(int i=0;i<16;++i){auto v=time_at(lower,r1,r2,A,q.mu_m3_s2);if(!v.valid||v.seconds<=q.time_of_flight_s)break;lower*=2;}
    auto lo=time_at(lower,r1,r2,A,q.mu_m3_s2),hi=time_at(upper,r1,r2,A,q.mu_m3_s2);
    if((lo.valid&&lo.seconds>q.time_of_flight_s)||!hi.valid||hi.seconds<q.time_of_flight_s)throw SearchError("unbracketed Lambert solution");
    double z=0;LambertTime value{};std::size_t iteration=0;
    for(;iteration<q.max_iterations;++iteration){
        z=(lower+upper)/2;value=time_at(z,r1,r2,A,q.mu_m3_s2);
        if(value.valid&&std::abs(value.seconds-q.time_of_flight_s)<=std::max(1e-10,q.time_of_flight_s*1e-13))break;
        if(!value.valid||value.seconds<q.time_of_flight_s)lower=z;else upper=z;
    }
    if(iteration==q.max_iterations||!value.valid)throw SearchError("Lambert convergence failure");
    const double f=1-value.y/r1,g=A*std::sqrt(value.y/q.mu_m3_s2),gdot=1-value.y/r2;
    if(!finite(g)||std::abs(g)<1e-15)throw SearchError("Lambert singular velocity reconstruction");
    const Vec3 depart=(q.arrival_relative_position_m-q.departure_relative_position_m*f)*(1/g);
    const Vec3 arrive=(q.arrival_relative_position_m*gdot-q.departure_relative_position_m)*(1/g);
    if(!finite(depart)||!finite(arrive))throw SearchError("Lambert velocity nonfinite");
    const auto independent=propagate_conic(q.departure_relative_position_m,depart,q.mu_m3_s2,q.time_of_flight_s);
    const double position_error=norm(independent.position_m-q.arrival_relative_position_m);
    const double velocity_error=norm(independent.velocity_mps-arrive);
    if(!finite(position_error)||!finite(velocity_error)||position_error>q.max_position_residual_m||velocity_error>q.max_velocity_residual_mps)
        throw SearchError("Lambert residual exceeds declared bounds");
    return {depart,arrive,q.branch,q.direction,position_error,velocity_error,std::abs(value.seconds-q.time_of_flight_s),iteration+1};
}
FlybyResult screen_unpowered_flyby(const FlybyRequest& q){
    if(!finite(q.incoming_vinf_mps)||norm(q.incoming_vinf_mps)<=0)throw SearchError("incoming_vinf_mps invalid");
    if(!finite(q.outgoing_vinf_mps)||norm(q.outgoing_vinf_mps)<=0)throw SearchError("outgoing_vinf_mps invalid");
    if(!finite(q.mu_planet_m3_s2)||q.mu_planet_m3_s2<=0)throw SearchError("mu_planet_m3_s2 invalid");
    if(!finite(q.radius_m)||q.radius_m<=0)throw SearchError("radius_m invalid");
    if(!q.atmosphere_altitude_m||!finite(*q.atmosphere_altitude_m)||*q.atmosphere_altitude_m<0)throw SearchError("atmosphere altitude unknown or invalid");
    if(!finite(q.safety_margin_m)||q.safety_margin_m<0)throw SearchError("safety_margin_m invalid");
    if(!finite(q.speed_tolerance_mps)||q.speed_tolerance_mps<0)throw SearchError("speed_tolerance_mps invalid");
    const double incoming=norm(q.incoming_vinf_mps),outgoing=norm(q.outgoing_vinf_mps);
    const double mismatch=std::abs(incoming-outgoing);
    if(mismatch>q.speed_tolerance_mps)throw SearchError("unpowered flyby speed mismatch");
    const double speed=std::max(incoming,outgoing); // conservative when mismatch is within tolerance
    const double clearance=q.radius_m+*q.atmosphere_altitude_m+q.safety_margin_m;
    if(!finite(clearance)||!finite(speed*speed)||!finite(clearance*speed*speed/q.mu_planet_m3_s2))throw SearchError("flyby clearance or speed scale nonfinite");
    if(!finite(q.maximum_periapsis_m)||q.maximum_periapsis_m<=clearance)throw SearchError("maximum_periapsis_m invalid");
    const double required=std::acos(std::clamp(dot(q.incoming_vinf_mps,q.outgoing_vinf_mps)/(incoming*outgoing),-1.0,1.0));
    if(required<=1e-12)throw SearchError("zero turn is not a finite assist");
    const double maximum=2*std::asin(1/(1+clearance*speed*speed/q.mu_planet_m3_s2));
    if(required>maximum)throw SearchError("flyby periapsis below clearance");
    const double periapsis=q.mu_planet_m3_s2/(speed*speed)*(1/std::sin(required/2)-1);
    if(!finite(periapsis)||periapsis>q.maximum_periapsis_m)throw SearchError("flyby periapsis exceeds encounter limit");
    return {required,maximum,periapsis,mismatch,q.speed_tolerance_mps-mismatch,maximum-required,periapsis-clearance};
}
}
