#include "Ephemeris.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace ksp {
Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 operator*(Vec3 a,double k){return {a.x*k,a.y*k,a.z*k};}
double norm(Vec3 a){return std::hypot(a.x,a.y,a.z);}
namespace {
bool finite(double v){return std::isfinite(v);}
bool finite(Vec3 v){return finite(v.x)&&finite(v.y)&&finite(v.z);}
bool finite(State s){return finite(s.position_m)&&finite(s.velocity_mps);}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
void validate(const Snapshot& s,const Settings& q){
    if(!s.analysis_ready) throw EphemerisError("analysis_ready is false; complete Cartesian snapshot required");
    if(s.confidence.empty() || s.confidence=="raw_config_provisional" || s.confidence=="requested_override_preview") throw EphemerisError("source confidence is provisional");
    if(s.snapshot_hash.empty()) throw EphemerisError("snapshot_hash missing");
    if(!s.state_epoch_ut_s || !finite(*s.state_epoch_ut_s)) throw EphemerisError("state_epoch_ut_s missing or nonfinite");
    if(s.frame.origin.empty()) throw EphemerisError("frame origin missing");
    if(s.frame.axes.empty()) throw EphemerisError("frame axes missing");
    if(s.frame.handedness!="right") throw EphemerisError("frame handedness must be right");
    if(!s.frame.inertial) throw EphemerisError("frame must be inertial");
    if(s.bodies.size()<2) throw EphemerisError("at least two bodies required");
    std::set<std::string> ids;
    for(const auto& b:s.bodies){
        if(b.id.empty()) throw EphemerisError("body id missing");
        if(!ids.insert(b.id).second) throw EphemerisError("duplicate body id: "+b.id);
        if(!finite(b.mu_m3_s2)||b.mu_m3_s2<=0) throw EphemerisError(b.id+" mu_m3_s2 must be positive finite");
        if(!finite(b.radius_m)||b.radius_m<=0) throw EphemerisError(b.id+" radius_m must be positive finite");
        if(!b.state) throw EphemerisError(b.id+" state missing");
        if(!finite(b.state->position_m)) throw EphemerisError(b.id+" position_m nonfinite");
        if(!finite(b.state->velocity_mps)) throw EphemerisError(b.id+" velocity_mps nonfinite");
        if(!b.epoch_ut_s || !finite(*b.epoch_ut_s) || *b.epoch_ut_s!=*s.state_epoch_ut_s) throw EphemerisError(b.id+" epoch mismatch");
    }
    if(!finite(q.step_s)||q.step_s<=0) throw EphemerisError("step_s must be positive finite");
    if(!finite(q.start_ut_s)||!finite(q.end_ut_s)||q.end_ut_s<=q.start_ut_s) throw EphemerisError("coverage interval invalid");
    if(q.start_ut_s!=*s.state_epoch_ut_s) throw EphemerisError("coverage start must equal state_epoch_ut_s");
    const double n=(q.end_ut_s-q.start_ut_s)/q.step_s;
    if(!finite(n)||n>static_cast<double>(q.max_steps)||q.max_steps==0) throw EphemerisError("step count excessive");
    if(std::abs(n-std::round(n))>1e-9) throw EphemerisError("interval must be integer number of steps");
    if(!finite(q.max_position_fit_error_m)||q.max_position_fit_error_m<=0 || !finite(q.max_velocity_fit_error_mps)||q.max_velocity_fit_error_mps<=0) throw EphemerisError("fit tolerances must be positive finite");
    if(q.result_label!="independent_newtonian_nbody") throw EphemerisError("principia_matched requires installed-version comparison evidence");
}
std::vector<Vec3> acceleration(const Snapshot& s,const std::vector<State>& y){
    std::vector<Vec3> a(y.size());
    for(std::size_t i=0;i<y.size();++i) for(std::size_t j=i+1;j<y.size();++j){
        Vec3 d=y[j].position_m-y[i].position_m; const double r=norm(d);
        if(!finite(d)||!finite(r)) throw EphemerisError("nonfinite pair separation");
        if(r<=s.bodies[i].radius_m+s.bodies[j].radius_m) throw EphemerisError("collision "+s.bodies[i].id+"/"+s.bodies[j].id);
        const double inv=1.0/(r*r*r);
        a[i]=a[i]+d*(s.bodies[j].mu_m3_s2*inv);
        a[j]=a[j]-d*(s.bodies[i].mu_m3_s2*inv);
        if(!finite(a[i])||!finite(a[j])) throw EphemerisError("nonfinite acceleration");
    }
    return a;
}
std::vector<State> advance_raw(const Snapshot& s,const std::vector<State>& y,double dt){
    const auto a=acceleration(s,y); auto z=y;
    for(std::size_t i=0;i<y.size();++i){
        z[i].position_m=y[i].position_m+y[i].velocity_mps*dt+a[i]*(0.5*dt*dt);
        if(!finite(z[i].position_m)) throw EphemerisError("nonfinite integrated position");
    }
    const auto b=acceleration(s,z);
    for(std::size_t i=0;i<y.size();++i){
        z[i].velocity_mps=y[i].velocity_mps+(a[i]+b[i])*(0.5*dt);
        if(!finite(z[i])) throw EphemerisError("nonfinite integrated state");
    }
    return z;
}
// Bootstrap a bound on each body's displacement. If every pair remains at
// least half its starting distance apart, its acceleration is at most the
// initial pairwise sum with each inverse-square term multiplied by four.
// The resulting displacement bound then proves no surface crossing over dt.
bool certified_clear(const Snapshot& s,const std::vector<State>& y,double dt){
    std::vector<double> max_acc(y.size()),travel(y.size());
    for(std::size_t i=0;i<y.size();++i) for(std::size_t j=i+1;j<y.size();++j){
        const double d=norm(y[j].position_m-y[i].position_m);
        if(!finite(d)) throw EphemerisError("nonfinite pair separation");
        if(d<=s.bodies[i].radius_m+s.bodies[j].radius_m) return false;
        max_acc[i]+=4*s.bodies[j].mu_m3_s2/(d*d);
        max_acc[j]+=4*s.bodies[i].mu_m3_s2/(d*d);
    }
    for(std::size_t i=0;i<y.size();++i){
        travel[i]=norm(y[i].velocity_mps)*dt+0.5*max_acc[i]*dt*dt;
        if(!finite(travel[i])) throw EphemerisError("nonfinite displacement bound");
    }
    for(std::size_t i=0;i<y.size();++i) for(std::size_t j=i+1;j<y.size();++j){
        const double d=norm(y[j].position_m-y[i].position_m);
        const double movement=travel[i]+travel[j];
        if(!finite(movement)||movement>=d/2||d-movement<=s.bodies[i].radius_m+s.bodies[j].radius_m) return false;
    }
    return true;
}
// At the final resolution, locate the earliest numerical boundary crossing
// on the local constant-acceleration position curve, or fail as unresolved.
[[noreturn]] void unresolved_or_collision(const Snapshot& s,const std::vector<State>& y,double t,double dt){
    const auto a=acceleration(s,y);
    double first=std::numeric_limits<double>::infinity();
    std::size_t first_i=0,first_j=0;
    auto at=[&](std::size_t i,double u){return y[i].position_m+y[i].velocity_mps*u+a[i]*(0.5*u*u);};
    for(std::size_t i=0;i<y.size();++i) for(std::size_t j=i+1;j<y.size();++j){
        const double radius=s.bodies[i].radius_m+s.bodies[j].radius_m;
        double previous=0;
        for(int sample=1;sample<=16;++sample){
            const double now=dt*sample/16;
            const double distance=norm(at(j,now)-at(i,now));
            if(!finite(distance)) throw EphemerisError("nonfinite close-approach state");
            if(distance<=radius){
                double lo=previous,hi=now;
                for(int k=0;k<48;++k){
                    const double mid=(lo+hi)/2;
                    if(norm(at(j,mid)-at(i,mid))<=radius)hi=mid;else lo=mid;
                }
                if(hi<first){first=hi;first_i=i;first_j=j;}
                break;
            }
            previous=now;
        }
    }
    if(finite(first)) throw EphemerisError("collision "+s.bodies[first_i].id+"/"+s.bodies[first_j].id+" at UT "+std::to_string(t+first));
    throw EphemerisError("unresolved close approach at UT "+std::to_string(t)+"; reduce step_s");
}
std::vector<State> step(const Snapshot& s,const std::vector<State>& y,double t,double dt,int depth=0,std::size_t* split_count=nullptr){
    if(certified_clear(s,y,dt)) return advance_raw(s,y,dt);
    if(depth>=30||dt<=1e-6) unresolved_or_collision(s,y,t,dt);
    if(split_count) ++*split_count;
    auto mid=step(s,y,t,dt/2,depth+1,split_count);
    return step(s,mid,t+dt/2,dt/2,depth+1,split_count);
}
State hermite(const State& a,const State& b,double h,double u){
    const double u2=u*u,u3=u2*u;
    const double h00=2*u3-3*u2+1,h10=u3-2*u2+u,h01=-2*u3+3*u2,h11=u3-u2;
    const double d00=(6*u2-6*u)/h,d10=3*u2-4*u+1,d01=(-6*u2+6*u)/h,d11=3*u2-2*u;
    return {a.position_m*h00+a.velocity_mps*(h*h10)+b.position_m*h01+b.velocity_mps*(h*h11),
            a.position_m*d00+a.velocity_mps*d10+b.position_m*d01+b.velocity_mps*d11};
}
}
State Ephemeris::query(const std::string& id,double t) const{
    if(!finite(t)) throw EphemerisError("query time nonfinite");
    if(t<metadata.start_ut_s||t>metadata.end_ut_s) throw EphemerisError("query outside coverage");
    auto it=std::find(body_ids.begin(),body_ids.end(),id);
    if(it==body_ids.end()) throw EphemerisError("body id missing: "+id);
    const auto& v=samples[static_cast<std::size_t>(it-body_ids.begin())];
    const double x=(t-metadata.start_ut_s)/metadata.step_s;
    const std::size_t k=std::min(static_cast<std::size_t>(std::floor(x)),v.size()-2);
    const double u=(t-(metadata.start_ut_s+metadata.step_s*k))/metadata.step_s;
    return hermite(v[k],v[k+1],metadata.step_s,u);
}
Ephemeris integrate(const Snapshot& s,const Settings& q){
    validate(s,q);
    const auto n=static_cast<std::size_t>(std::llround((q.end_ut_s-q.start_ut_s)/q.step_s));
    Ephemeris e;
    e.metadata={s.snapshot_hash,s.confidence,s.frame.origin,s.frame.axes,s.frame.handedness,s.frame.inertial,
        "newtonian_point_mass","velocity_verlet_with_certified_safety_substeps","2",q.result_label,"cubic_hermite_position_velocity",
        *s.state_epoch_ut_s,q.start_ut_s,q.end_ut_s,q.step_s,q.step_s,q.max_position_fit_error_m,q.max_velocity_fit_error_mps,0,0};
    std::vector<State> current;
    for(const auto& b:s.bodies){e.body_ids.push_back(b.id);e.samples.emplace_back();e.samples.back().reserve(n+1);current.push_back(*b.state);e.samples.back().push_back(*b.state);}
    for(std::size_t i=0;i<current.size();++i) for(std::size_t j=i+1;j<current.size();++j)
        if(norm(current[j].position_m-current[i].position_m)<=s.bodies[i].radius_m+s.bodies[j].radius_m)
            throw EphemerisError("collision "+s.bodies[i].id+"/"+s.bodies[j].id+" at UT "+std::to_string(q.start_ut_s));
    for(std::size_t k=0;k<n;++k){
        current=step(s,current,q.start_ut_s+k*q.step_s,q.step_s,0,&e.metadata.safety_split_count);
        for(std::size_t i=0;i<current.size();++i)e.samples[i].push_back(current[i]);
    }
    // Carry one sixfold finer reference from the initial state over the full
    // coverage. This measures the coarse ephemeris disagreement, including
    // accumulated phase error, rather than restarting at each coarse sample.
    const double fine_h=q.step_s/6;
    current.clear(); for(const auto& b:s.bodies)current.push_back(*b.state);
    for(std::size_t k=0;k<n;++k){
        for(int sub=1;sub<=6;++sub){
            current=step(s,current,q.start_ut_s+(6*k+sub-1)*fine_h,fine_h);
            if(sub!=2&&sub!=3&&sub!=6)continue;
            const double u=static_cast<double>(sub)/6;
            for(std::size_t i=0;i<current.size();++i){
                auto fit=hermite(e.samples[i][k],e.samples[i][k+1],q.step_s,u);
                const double position_error=norm(fit.position_m-current[i].position_m);
                const double velocity_error=norm(fit.velocity_mps-current[i].velocity_mps);
                if(!finite(position_error)||!finite(velocity_error)) throw EphemerisError("nonfinite fit error");
                e.metadata.measured_max_position_error_m=std::max(e.metadata.measured_max_position_error_m,position_error);
                e.metadata.measured_max_velocity_error_mps=std::max(e.metadata.measured_max_velocity_error_mps,velocity_error);
            }
        }
    }
    if(e.metadata.measured_max_position_error_m>q.max_position_fit_error_m || e.metadata.measured_max_velocity_error_mps>q.max_velocity_fit_error_mps)
        throw EphemerisError("fit tolerance exceeded at independent off-grid samples");
    return e;
}
bool cache_compatible(const Metadata& m,const Snapshot& s,const Settings& q){
    if(!finite(m.measured_max_position_error_m)||!finite(m.measured_max_velocity_error_mps)||m.measured_max_position_error_m<0||m.measured_max_velocity_error_mps<0) return false;
    return s.analysis_ready && m.snapshot_hash==s.snapshot_hash && m.source_confidence==s.confidence &&
        m.frame_origin==s.frame.origin && m.frame_axes==s.frame.axes && m.frame_handedness==s.frame.handedness && m.frame_inertial==s.frame.inertial &&
        s.state_epoch_ut_s && m.state_epoch_ut_s==*s.state_epoch_ut_s && m.force_model=="newtonian_point_mass" &&
        m.integrator=="velocity_verlet_with_certified_safety_substeps" && m.integrator_version=="2" && m.result_label==q.result_label &&
        m.interpolation=="cubic_hermite_position_velocity" && m.step_s==q.step_s && m.segment_spacing_s==q.step_s &&
        m.start_ut_s==q.start_ut_s && m.end_ut_s==q.end_ut_s && m.max_position_fit_error_m==q.max_position_fit_error_m &&
        m.max_velocity_fit_error_mps==q.max_velocity_fit_error_mps &&
        m.measured_max_position_error_m<=q.max_position_fit_error_m && m.measured_max_velocity_error_mps<=q.max_velocity_fit_error_mps;
}
}
