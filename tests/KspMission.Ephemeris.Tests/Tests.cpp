#include "Ephemeris.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <array>

using namespace ksp;
namespace {
int checks = 0;
void check(bool ok, const std::string& label) { ++checks; if (!ok) throw std::runtime_error(label); }
template<class F> void fails(F f, const std::string& field) {
    try { f(); } catch (const EphemerisError& e) {
        check(std::string(e.what()).find(field) != std::string::npos, std::string("wrong error: ") + e.what()); return;
    }
    throw std::runtime_error("expected failure: " + field);
}
Snapshot pair_snapshot() {
    constexpr double a=1.495978707e11, m1=1.32712440018e20, m2=3.986004418e14;
    const double omega=std::sqrt((m1+m2)/(a*a*a));
    Snapshot s;
    s.analysis_ready=true; s.confidence="synthetic_fixture"; s.snapshot_hash="pair-v1";
    s.state_epoch_ut_s=0; s.frame={"barycenter", "X,Y,Z", "right", true};
    s.bodies={{"star",m1,1e8,State{{-a*m2/(m1+m2),0,0},{0,-a*omega*m2/(m1+m2),0}},0},
              {"planet",m2,1e6,State{{a*m1/(m1+m2),0,0},{0,a*omega*m1/(m1+m2),0}},0}};
    return s;
}
Settings settings(double end=31557600, double step=600) {
    Settings x; x.start_ut_s=0; x.end_ut_s=end; x.step_s=step;
    x.max_position_fit_error_m=100; x.max_velocity_fit_error_mps=0.01; return x;
}
void gate_failures() {
    auto s=pair_snapshot(); auto q=settings(1200);
    s.analysis_ready=false; s.confidence="raw_config_provisional";
    fails([&]{ integrate(s,q); }, "analysis_ready");
    s=pair_snapshot(); s.bodies[1].state.reset(); fails([&]{ integrate(s,q); }, "state");
    s=pair_snapshot(); s.state_epoch_ut_s.reset(); fails([&]{ integrate(s,q); }, "state_epoch_ut_s");
    s=pair_snapshot(); s.frame.axes=""; fails([&]{ integrate(s,q); }, "axes");
    s=pair_snapshot(); s.frame.handedness="left"; fails([&]{ integrate(s,q); }, "handedness");
    s=pair_snapshot(); s.frame.inertial=false; fails([&]{ integrate(s,q); }, "inertial");
    s=pair_snapshot(); s.bodies[1].epoch_ut_s=1; fails([&]{ integrate(s,q); }, "epoch");
    s=pair_snapshot(); s.bodies[1].mu_m3_s2=0; fails([&]{ integrate(s,q); }, "mu");
    s=pair_snapshot(); s.bodies[1].id="star"; fails([&]{ integrate(s,q); }, "duplicate");
    s=pair_snapshot(); s.bodies[1].state->position_m.x=std::numeric_limits<double>::quiet_NaN(); fails([&]{ integrate(s,q); }, "position_m");
    s=pair_snapshot(); q.step_s=0; fails([&]{ integrate(s,q); }, "step_s");
    q=settings(1000); fails([&]{ integrate(s,q); }, "integer");
    q=settings(1200); q.result_label="principia_matched"; fails([&]{ integrate(s,q); }, "principia_matched");
}
void collision_failures() {
    auto s=pair_snapshot(); auto q=settings(1200);
    s.bodies[1].state->position_m=s.bodies[0].state->position_m;
    fails([&]{ integrate(s,q); }, "collision");
    s=pair_snapshot(); s.bodies[0].state->position_m={-1000,0,0}; s.bodies[1].state->position_m={1000,0,0};
    s.bodies[0].radius_m=10; s.bodies[1].radius_m=10; s.bodies[0].mu_m3_s2=1; s.bodies[1].mu_m3_s2=1;
    s.bodies[0].state->velocity_mps={2,0,0}; s.bodies[1].state->velocity_mps={-2,0,0};
    try { integrate(s,q); throw std::runtime_error("missed between-step collision"); }
    catch(const EphemerisError& e){const std::string message=e.what(),prefix="collision star/planet at UT ";
        check(message.rfind(prefix,0)==0 && std::stod(message.substr(prefix.size()))>490 && std::stod(message.substr(prefix.size()))<500,
              std::string("collision pair and time: ")+message);}

    // A wide chord cuts through the central sphere, while the numerically
    // propagated circular arc stays outside it. A chord is not a collision.
    Snapshot curved;
    curved.analysis_ready=true; curved.confidence="synthetic_fixture"; curved.snapshot_hash="curved-clear-v1";
    curved.state_epoch_ut_s=0; curved.frame={"barycenter","X,Y,Z","right",true};
    const double orbital_speed=std::sqrt(1000001.0/1000.0);
    curved.bodies={{"center",1000000,970,State{{0,0,0},{0,0,0}},0},
                   {"satellite",1,1,State{{1000,0,0},{0,orbital_speed,0}},0}};
    auto clear_settings=settings(20,20);
    clear_settings.max_position_fit_error_m=1e5;
    clear_settings.max_velocity_fit_error_mps=1e3;
    auto clear=integrate(curved,clear_settings);
    check(clear.query("satellite",20).position_m.x>0,"curved clear pass");
}
void generated_nonfinite_and_long_drift_failures(){
    auto s=pair_snapshot(); auto q=settings(1200);
    s.bodies[0].state->position_m.x=-1e308;
    s.bodies[1].state->position_m.x=1e308;
    fails([&]{integrate(s,q);},"nonfinite");

    s=pair_snapshot();
    const double r=1.495978707e11;
    const double omega=std::sqrt((s.bodies[0].mu_m3_s2+s.bodies[1].mu_m3_s2)/(r*r*r));
    const double end=std::floor((2*std::acos(-1.0)/omega)/600)*600;
    q=settings(end,600);
    fails([&]{integrate(s,q);},"fit tolerance");
}
void queries_and_cache() {
    auto e=integrate(pair_snapshot(),settings(1200));
    fails([&]{ e.query("planet",-1); }, "coverage");
    fails([&]{ e.query("planet",1201); }, "coverage");
    fails([&]{ e.query("planet",std::numeric_limits<double>::quiet_NaN()); }, "time");
    fails([&]{ e.query("missing",0); }, "body");
    check(cache_compatible(e.metadata,pair_snapshot(),settings(1200)),"cache hit");
    auto s=pair_snapshot(); s.snapshot_hash="changed"; check(!cache_compatible(e.metadata,s,settings(1200)),"hash miss");
    s=pair_snapshot(); s.frame.axes="A,B,C"; check(!cache_compatible(e.metadata,s,settings(1200)),"frame miss");
    auto q=settings(1200); q.step_s=300; check(!cache_compatible(e.metadata,pair_snapshot(),q),"step miss");
    q=settings(1200); q.max_position_fit_error_m=0.1; check(!cache_compatible(e.metadata,pair_snapshot(),q),"fit miss");
    auto m=e.metadata; m.measured_max_position_error_m=std::numeric_limits<double>::quiet_NaN();
    check(!cache_compatible(m,pair_snapshot(),settings(1200)),"invalid error miss");
    m=e.metadata; m.force_model="different"; check(!cache_compatible(m,pair_snapshot(),settings(1200)),"force miss");
    m=e.metadata; m.integrator_version="different"; check(!cache_compatible(m,pair_snapshot(),settings(1200)),"integrator miss");
    m=e.metadata; m.interpolation="different"; check(!cache_compatible(m,pair_snapshot(),settings(1200)),"interpolation miss");
    q=settings(1200);q.end_ut_s=1800;check(!cache_compatible(e.metadata,pair_snapshot(),q),"coverage miss");
    q=settings(1200);q.max_position_fit_error_m=1e-20;fails([&]{integrate(pair_snapshot(),q);},"fit tolerance");
    for(const auto& id:e.body_ids){
        const auto a=e.query(id,0),b=e.query(id,1200);
        check(norm(a.position_m-e.samples[&id-&e.body_ids[0]][0].position_m)<1e-6,"start endpoint");
        check(norm(b.position_m-e.samples[&id-&e.body_ids[0]][2].position_m)<1e-6,"end endpoint");
        const auto l=e.query(id,600-1e-3),c=e.query(id,600),r=e.query(id,600+1e-3);
        check(norm((c.position_m-l.position_m)-(r.position_m-c.position_m))<1e-6,"position continuity");
        check(norm((c.velocity_mps-l.velocity_mps)-(r.velocity_mps-c.velocity_mps))<1e-6,"velocity continuity");
    }
}
void two_body_and_fit() {
    auto s=pair_snapshot(); const double r=1.495978707e11;
    const double w=std::sqrt((s.bodies[0].mu_m3_s2+s.bodies[1].mu_m3_s2)/(r*r*r));
    const double period=2*std::acos(-1.0)/w;
    const double end=std::floor(period/600)*600;
    auto coarse_settings=settings(end);
    coarse_settings.max_position_fit_error_m=10000;
    auto fine_settings=settings(end,300);
    fine_settings.max_position_fit_error_m=10000;
    auto e=integrate(s,coarse_settings); auto fine=integrate(s,fine_settings);
    double pos=0,vel=0,fitp=0,fitv=0,ratio_error=0;
    const auto rel=[&](const Ephemeris& x,double t){ auto a=x.query("star",t),b=x.query("planet",t); return State{b.position_m-a.position_m,b.velocity_mps-a.velocity_mps}; };
    for(int i=0;i<=12;++i) {
        double t=end*i/12.0; auto z=rel(e,t); Vec3 ep{r*std::cos(w*t),r*std::sin(w*t),0},ev{-r*w*std::sin(w*t),r*w*std::cos(w*t),0};
        pos=std::max(pos,norm(z.position_m-ep)); vel=std::max(vel,norm(z.velocity_mps-ev));
        auto f=rel(fine,t); ratio_error=std::max(ratio_error,norm(f.position_m-ep));
    }
    const int coarse_steps=static_cast<int>(end/600);
    for(int i=0;i<20;++i) for(double f : {1.0/3,0.5}) {
        double t=(std::min(coarse_steps-1,coarse_steps*i/19)+f)*600; auto a=rel(e,t),b=rel(fine,t);
        fitp=std::max(fitp,norm(a.position_m-b.position_m)); fitv=std::max(fitv,norm(a.velocity_mps-b.velocity_mps));
    }
    check(pos<10000,"analytic position"); check(vel<0.01,"analytic velocity");
    check(pos/ratio_error>3 && pos/ratio_error<5,"step halving");
    check(fitp<10000 && fitv<0.01,"off grid full-interval comparison");
    check(e.metadata.measured_max_position_error_m<=10000 && e.metadata.measured_max_velocity_error_mps<=0.01,"fit metadata");
    std::cout<<"two-body max m="<<pos<<" m/s="<<vel<<" ratio="<<pos/ratio_error<<" off-grid m="<<fitp<<" m/s="<<fitv<<'\n';
}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
struct Conserved {Vec3 center,velocity,momentum,angular; double energy=0,scale=0;};
Conserved conserved(const Snapshot& s,const Ephemeris& e,double t){
    Conserved c; double mass=0;
    for(const auto& b:s.bodies){
        auto z=e.query(b.id,t); auto m=b.mu_m3_s2; mass+=m;
        c.center=c.center+z.position_m*m; c.velocity=c.velocity+z.velocity_mps*m;
        c.momentum=c.momentum+z.velocity_mps*m; c.angular=c.angular+cross(z.position_m,z.velocity_mps)*m;
        c.energy+=0.5*m*dot(z.velocity_mps,z.velocity_mps); c.scale+=m*norm(z.velocity_mps);
    }
    for(std::size_t i=0;i<s.bodies.size();++i) for(std::size_t j=i+1;j<s.bodies.size();++j){
        auto a=e.query(s.bodies[i].id,t),b=e.query(s.bodies[j].id,t);
        c.energy-=s.bodies[i].mu_m3_s2*s.bodies[j].mu_m3_s2/norm(b.position_m-a.position_m);
    }
    c.center=c.center*(1/mass);c.velocity=c.velocity*(1/mass);return c;
}
Snapshot triple_snapshot(){
    auto s=pair_snapshot(); s.snapshot_hash="triple-v1";
    const double r=2.3e11,mu=s.bodies[0].mu_m3_s2;
    s.bodies.push_back({"outer",4.0e14,1e6,State{{0,r,0},{-std::sqrt(mu/r),0,0}},0});
    return s;
}
// Separate acceleration loop and RK4 advance, sharing only fixture values and the State type.
std::vector<State> independent_acceleration(const Snapshot& s,const std::vector<State>& y){
    std::vector<State> derivative(y.size());
    for(std::size_t target=0;target<y.size();++target){
        derivative[target].position_m=y[target].velocity_mps;
        for(std::size_t source=0;source<y.size();++source){
            if(source==target)continue;
            const auto delta=y[source].position_m-y[target].position_m;
            const auto dist=std::sqrt(dot(delta,delta));
            derivative[target].velocity_mps=derivative[target].velocity_mps+delta*(s.bodies[source].mu_m3_s2/(dist*dist*dist));
        }
    }
    return derivative;
}
std::vector<State> add_scaled(const std::vector<State>& y,const std::vector<State>& k,double h){
    auto z=y;for(std::size_t i=0;i<y.size();++i){z[i].position_m=z[i].position_m+k[i].position_m*h;z[i].velocity_mps=z[i].velocity_mps+k[i].velocity_mps*h;}return z;
}
std::vector<State> independent_rk4(const Snapshot& s,std::vector<State> y,double h,int count){
    for(int n=0;n<count;++n){
        auto k1=independent_acceleration(s,y);auto k2=independent_acceleration(s,add_scaled(y,k1,h/2));
        auto k3=independent_acceleration(s,add_scaled(y,k2,h/2));auto k4=independent_acceleration(s,add_scaled(y,k3,h));
        for(std::size_t i=0;i<y.size();++i){
            y[i].position_m=y[i].position_m+(k1[i].position_m+k2[i].position_m*2+k3[i].position_m*2+k4[i].position_m)*(h/6);
            y[i].velocity_mps=y[i].velocity_mps+(k1[i].velocity_mps+k2[i].velocity_mps*2+k3[i].velocity_mps*2+k4[i].velocity_mps)*(h/6);
        }
    }return y;
}
void triple_and_independent(){
    auto s=triple_snapshot();constexpr double end=30*86400.0;
    auto q=settings(end);q.max_position_fit_error_m=500;
    auto e=integrate(s,q);
    const auto a=conserved(s,e,0),b=conserved(s,e,end);
    const double com=norm(b.center-a.center-a.velocity*end);
    const double momentum=norm(b.momentum-a.momentum)/a.scale;
    const double angular=norm(b.angular-a.angular)/norm(a.angular);
    const double energy=std::abs(b.energy-a.energy)/std::abs(a.energy);
    check(com<1,"COM drift");check(momentum<1e-10,"momentum drift");
    check(angular<1e-9,"angular momentum drift");check(energy<1e-6,"energy drift");
    std::vector<State> y;for(const auto& x:s.bodies)y.push_back(*x.state);
    double p=0,v=0;
    for(int day=5;day<=30;day+=5){
        y=independent_rk4(s,y,60,5*86400/60);
        for(std::size_t i=0;i<y.size();++i){auto z=e.query(s.bodies[i].id,day*86400.0);
            p=std::max(p,norm(z.position_m-y[i].position_m));v=std::max(v,norm(z.velocity_mps-y[i].velocity_mps));}
    }
    std::cout<<"three-body COM m="<<com<<" momentum rel="<<momentum<<" angular rel="<<angular<<" energy rel="<<energy
             <<" RK4 max m="<<p<<" m/s="<<v<<'\n';
    check(p<250 && v<0.001,"independent RK4 comparison");
}
}
int main(){ try { gate_failures(); collision_failures(); generated_nonfinite_and_long_drift_failures(); queries_and_cache(); two_body_and_fit(); triple_and_independent(); std::cout<<"PASS "<<checks<<" checks\n"; } catch(const std::exception& e){ std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; } }
