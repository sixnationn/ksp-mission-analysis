#include "Spacecraft.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace ksp {
namespace {
bool finite(double x){return std::isfinite(x);} bool finite(Vec3 x){return finite(x.x)&&finite(x.y)&&finite(x.z);}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
State add(State x,State y,double factor){return {x.position_m+y.position_m*factor,x.velocity_mps+y.velocity_mps*factor};}
State derivative(const Snapshot& s,const Ephemeris& e,double t,State y){
    Vec3 acceleration{};
    for(const auto& b:s.bodies){
        const auto planet=e.query(b.id,t);const Vec3 delta=planet.position_m-y.position_m;
        const double distance=norm(delta);
        if(distance==0)throw SpacecraftError("zero body separation at UT "+std::to_string(t),t,y);
        acceleration=acceleration+delta*(b.mu_m3_s2/(distance*distance*distance));
    }
    return {y.velocity_mps,acceleration};
}
struct Trial {State high,low;};
// Dormand-Prince 5(4), with seven evaluations and the embedded fourth-order estimate.
Trial trial(const Snapshot& s,const Ephemeris& e,double t,State y,double h){
    const auto k1=derivative(s,e,t,y);
    const auto k2=derivative(s,e,t+h/5,add(y,k1,h/5));
    auto z=add(add(y,k1,h*3/40),k2,h*9/40);
    const auto k3=derivative(s,e,t+h*3/10,z);
    z=add(add(add(y,k1,h*44/45),k2,-h*56/15),k3,h*32/9);
    const auto k4=derivative(s,e,t+h*4/5,z);
    z=add(add(add(add(y,k1,h*19372/6561),k2,-h*25360/2187),k3,h*64448/6561),k4,-h*212/729);
    const auto k5=derivative(s,e,t+h*8/9,z);
    z=add(add(add(add(add(y,k1,h*9017/3168),k2,-h*355/33),k3,h*46732/5247),k4,h*49/176),k5,-h*5103/18656);
    const auto k6=derivative(s,e,t+h,z);
    z=add(add(add(add(add(y,k1,h*35/384),k3,h*500/1113),k4,h*125/192),k5,-h*2187/6784),k6,h*11/84);
    const auto k7=derivative(s,e,t+h,z);
    auto low=add(add(add(add(add(add(y,k1,h*5179/57600),k3,h*7571/16695),k4,h*393/640),k5,-h*92097/339200),k6,h*187/2100),k7,h/40);
    return {z,low};
}
void validate(const Snapshot& s,const Ephemeris& e,State y,const SpacecraftSettings& q){
    if(!s.analysis_ready)throw SpacecraftError("analysis_ready is false");
    if(s.snapshot_hash.empty()||e.metadata.snapshot_hash!=s.snapshot_hash)throw SpacecraftError("snapshot_hash mismatch");
    if(e.metadata.source_confidence!=s.confidence||s.confidence=="raw_config_provisional")throw SpacecraftError("source confidence mismatch");
    if(!s.state_epoch_ut_s||e.metadata.state_epoch_ut_s!=*s.state_epoch_ut_s)throw SpacecraftError("epoch mismatch");
    if(!s.frame.inertial||!e.metadata.frame_inertial||s.frame.handedness!="right"||e.metadata.frame_origin!=s.frame.origin||e.metadata.frame_axes!=s.frame.axes||e.metadata.frame_handedness!=s.frame.handedness)throw SpacecraftError("frame mismatch");
    if(e.metadata.force_model!="newtonian_point_mass"||e.metadata.result_label!="independent_newtonian_nbody")throw SpacecraftError("force_model mismatch");
    if(e.body_ids.size()!=s.bodies.size())throw SpacecraftError("body identity mismatch");
    for(const auto& b:s.bodies){
        if(std::find(e.body_ids.begin(),e.body_ids.end(),b.id)==e.body_ids.end()||!finite(b.mu_m3_s2)||b.mu_m3_s2<=0||!finite(b.radius_m)||b.radius_m<=0)throw SpacecraftError("body identity or clearance invalid");
        if(!b.state||!finite(b.state->position_m)||!finite(b.state->velocity_mps)||!b.epoch_ut_s||*b.epoch_ut_s!=*s.state_epoch_ut_s)throw SpacecraftError("body state or epoch invalid");
    }
    if(!finite(e.metadata.max_position_fit_error_m)||!finite(e.metadata.max_velocity_fit_error_mps)||!finite(e.metadata.measured_max_position_error_m)||!finite(e.metadata.measured_max_velocity_error_mps)||
        e.metadata.max_position_fit_error_m<=0||e.metadata.max_velocity_fit_error_mps<=0||e.metadata.measured_max_position_error_m<0||e.metadata.measured_max_velocity_error_mps<0||
        e.metadata.measured_max_position_error_m>e.metadata.max_position_fit_error_m||e.metadata.measured_max_velocity_error_mps>e.metadata.max_velocity_fit_error_mps)
        throw SpacecraftError("ephemeris fit bounds invalid");
    if(!finite(y.position_m))throw SpacecraftError("position_m nonfinite");
    if(!finite(y.velocity_mps))throw SpacecraftError("velocity_mps nonfinite");
    if(!finite(q.start_ut_s)||!finite(q.end_ut_s)||q.end_ut_s<q.start_ut_s||q.start_ut_s<e.metadata.start_ut_s||q.end_ut_s>e.metadata.end_ut_s)throw SpacecraftError("coverage invalid");
    if(!finite(q.abs_position_tolerance_m)||q.abs_position_tolerance_m<=0||!finite(q.abs_velocity_tolerance_mps)||q.abs_velocity_tolerance_mps<=0||!finite(q.relative_tolerance)||q.relative_tolerance<=0)throw SpacecraftError("tolerance must be positive finite");
    if(!finite(q.min_step_s)||!finite(q.max_step_s)||q.min_step_s<=0||q.max_step_s<q.min_step_s||q.max_accepted_steps==0)throw SpacecraftError("step bounds invalid");
    if(!finite(q.safety_margin_m)||q.safety_margin_m<0)throw SpacecraftError("safety_margin_m invalid");
    for(const auto& burn:q.burns)if(!finite(burn.ut_s)||burn.ut_s<q.start_ut_s||burn.ut_s>q.end_ut_s||!finite(burn.delta_v_mps))throw SpacecraftError("burn time or vector invalid");
    std::vector<std::string> atmosphere_ids;
    for(const auto& boundary:q.atmosphere_boundaries){
        if(std::find_if(s.bodies.begin(),s.bodies.end(),[&](const Body& b){return b.id==boundary.body_id;})==s.bodies.end())throw SpacecraftError("atmosphere body_id missing");
        if(!finite(boundary.altitude_m)||boundary.altitude_m<0)throw SpacecraftError("atmosphere altitude_m invalid");
        if(std::find(atmosphere_ids.begin(),atmosphere_ids.end(),boundary.body_id)!=atmosphere_ids.end())throw SpacecraftError("duplicate atmosphere body_id");
        atmosphere_ids.push_back(boundary.body_id);
    }
    for(const auto& body:s.bodies)
        if(std::find(atmosphere_ids.begin(),atmosphere_ids.end(),body.id)==atmosphere_ids.end())
            throw SpacecraftError("missing atmosphere boundary for body "+body.id+" (use explicit zero for none)");
}
double atmosphere(const SpacecraftSettings& q,const std::string& id){
    auto it=std::find_if(q.atmosphere_boundaries.begin(),q.atmosphere_boundaries.end(),[&](const AtmosphereBoundary& x){return x.body_id==id;});
    if(it==q.atmosphere_boundaries.end())throw SpacecraftError("missing atmosphere boundary for body "+id);
    return it->altitude_m;
}
double clearance(const Snapshot& s,const Ephemeris& e,State y,double t,std::size_t i,const SpacecraftSettings& q){
    return norm(y.position_m-e.query(s.bodies[i].id,t).position_m)-(s.bodies[i].radius_m+atmosphere(q,s.bodies[i].id)+q.safety_margin_m);
}
double radial(const Snapshot& s,const Ephemeris& e,State y,double t,std::size_t i){
    auto p=e.query(s.bodies[i].id,t);return dot(y.position_m-p.position_m,y.velocity_mps-p.velocity_mps);
}
std::pair<double,double> nearest_relative_scales(const Snapshot& s,const Ephemeris& e,State y,double t){
    double distance=std::numeric_limits<double>::infinity(),speed=std::numeric_limits<double>::infinity();
    for(const auto& body:s.bodies){
        const auto state=e.query(body.id,t);
        distance=std::min(distance,norm(y.position_m-state.position_m));
        speed=std::min(speed,norm(y.velocity_mps-state.velocity_mps));
    }
    return {distance,speed};
}
// For cubic Hermite, |h01'| <= 1.5 and |h10'|, |h11'| <= 1 on [0,1].
// This bounds the speed of the fitted body path, including between queries.
double body_speed_bound(const Ephemeris& e,const std::string& id,double t,double h){
    const auto it=std::find(e.body_ids.begin(),e.body_ids.end(),id);
    if(it==e.body_ids.end())throw SpacecraftError("body speed bound id missing");
    const auto& samples=e.samples[static_cast<std::size_t>(it-e.body_ids.begin())];
    if(samples.size()<2||!finite(e.metadata.step_s)||e.metadata.step_s<=0)throw SpacecraftError("body speed bound metadata invalid");
    const auto first=std::min(static_cast<std::size_t>(std::floor((t-e.metadata.start_ut_s)/e.metadata.step_s)),samples.size()-2);
    const auto last=std::min(static_cast<std::size_t>(std::floor((t+h-e.metadata.start_ut_s)/e.metadata.step_s)),samples.size()-2);
    double bound=0;
    for(std::size_t k=first;k<=last;++k){
        const auto& a=samples[k];const auto& b=samples[k+1];
        bound=std::max(bound,1.5*norm(b.position_m-a.position_m)/e.metadata.step_s+
                             norm(a.velocity_mps)+norm(b.velocity_mps));
    }
    if(!finite(bound))throw SpacecraftError("body speed bound nonfinite");
    return bound;
}
// A bootstrap bound for the model ODE. If each separation remains above half
// its starting value, spacecraft acceleration is at most sum 4*mu/d^2.
// The displacement bound then proves that assumption and clearance together.
bool clear_interval_bound(const Snapshot& s,const Ephemeris& e,const SpacecraftSettings& q,State y,double t,double h){
    std::vector<double> distance,speed;
    double acceleration=0;
    for(const auto& body:s.bodies){
        const double d=norm(y.position_m-e.query(body.id,t).position_m);
        if(!finite(d)||d<=0)return false;
        distance.push_back(d);
        speed.push_back(body_speed_bound(e,body.id,t,h));
        acceleration+=4*body.mu_m3_s2/(d*d);
    }
    if(!finite(acceleration))return false;
    for(std::size_t i=0;i<s.bodies.size();++i){
        const double travel=(norm(y.velocity_mps)+speed[i])*h+0.5*acceleration*h*h;
        const double limit=s.bodies[i].radius_m+atmosphere(q,s.bodies[i].id)+q.safety_margin_m;
        const double guard=e.metadata.max_position_fit_error_m+q.abs_position_tolerance_m;
        if(!finite(travel)||travel>=0.5*distance[i]||distance[i]-limit<=travel+guard)return false;
    }
    return true;
}
EncounterEvent event(const Snapshot& s,const Ephemeris& e,State y,double t,std::size_t i,const SpacecraftSettings& q){
    double distance=norm(y.position_m-e.query(s.bodies[i].id,t).position_m);
    return {t,s.bodies[i].id,s.frame.origin,s.frame.axes,distance,distance-(s.bodies[i].radius_m+atmosphere(q,s.bodies[i].id)+q.safety_margin_m),y};
}
template<class F> double root(const Snapshot& s,const Ephemeris& e,double t,State y,double h,double lo,double hi,F function){
    double flo=function(y,t);
    if(lo>0){auto z=trial(s,e,t,y,lo*h).high;flo=function(z,t+lo*h);}
    for(int iteration=0;iteration<45;++iteration){
        const double mid=(lo+hi)/2;auto z=trial(s,e,t,y,mid*h).high;double value=function(z,t+mid*h);
        if((flo>0&&value>0)||(flo<0&&value<0)){lo=mid;flo=value;}else hi=mid;
    }
    return t+hi*h;
}
void events(const Snapshot& s,const Ephemeris& e,const SpacecraftSettings& q,SpacecraftResult& result,double t,State y,double h,State endpoint){
    std::array<State,5> points{y,trial(s,e,t,y,h*0.25).high,trial(s,e,t,y,h*0.5).high,trial(s,e,t,y,h*0.75).high,endpoint};
    std::optional<EncounterEvent> first_unsafe;
    for(std::size_t i=0;i<s.bodies.size();++i){
        for(int part=0;part<4;++part){
            double u=part*0.25,v=(part+1)*0.25;
            const double c0=clearance(s,e,points[part],t+u*h,i,q);
            const double c1=clearance(s,e,points[part+1],t+v*h,i,q);
            if(c0>0&&c1<=0){
                double time=root(s,e,t,y,h,u,v,[&](State z,double at){return clearance(s,e,z,at,i,q);});
                auto found=event(s,e,trial(s,e,t,y,time-t).high,time,i,q);
                if(!first_unsafe||found.ut_s<first_unsafe->ut_s)first_unsafe=found;
            }
            if(c0>0&&c1>0){
                const auto body0=e.query(s.bodies[i].id,t+u*h),body1=e.query(s.bodies[i].id,t+v*h);
                const Vec3 r0=points[part].position_m-body0.position_m;
                const Vec3 displacement=(points[part+1].position_m-body1.position_m)-r0;
                const double length2=dot(displacement,displacement);
                const double closest=length2>0?std::clamp(-dot(r0,displacement)/length2,0.0,1.0):0.0;
                const double limit=s.bodies[i].radius_m+atmosphere(q,s.bodies[i].id)+q.safety_margin_m;
                if(closest>0&&closest<1&&norm(r0+displacement*closest)<limit){
                    const double b=2*dot(r0,displacement),c=dot(r0,r0)-limit*limit;
                    const double entry=(-b-std::sqrt(std::max(0.0,b*b-4*length2*c)))/(2*length2);
                    const double probe=u+(v-u)*(entry+0.25*(closest-entry));
                    auto probe_state=trial(s,e,t,y,probe*h).high;
                    if(clearance(s,e,probe_state,t+probe*h,i,q)<=0){
                        double time=root(s,e,t,y,h,u,probe,[&](State z,double at){return clearance(s,e,z,at,i,q);});
                        auto found=event(s,e,trial(s,e,t,y,time-t).high,time,i,q);
                        if(!first_unsafe||found.ut_s<first_unsafe->ut_s)first_unsafe=found;
                    }
                }
            }
            const double g0=radial(s,e,points[part],t+u*h,i),g1=radial(s,e,points[part+1],t+v*h,i);
            if(g0<0&&g1>=0){
                double time=root(s,e,t,y,h,u,v,[&](State z,double at){return radial(s,e,z,at,i);});
                if(!result.closest_approaches.empty()&&result.closest_approaches.back().body_id==s.bodies[i].id&&std::abs(result.closest_approaches.back().ut_s-time)<1e-5)continue;
                result.closest_approaches.push_back(event(s,e,trial(s,e,t,y,time-t).high,time,i,q));
            }
        }
    }
    std::sort(result.closest_approaches.begin(),result.closest_approaches.end(),[](const EncounterEvent& a,const EncounterEvent& b){return a.ut_s<b.ut_s;});
    result.closest_approaches.erase(std::unique(result.closest_approaches.begin(),result.closest_approaches.end(),[](const EncounterEvent& a,const EncounterEvent& b){
        return a.body_id==b.body_id&&std::abs(a.ut_s-b.ut_s)<1e-5;
    }),result.closest_approaches.end());
    if(first_unsafe){
        result.closest_approaches.erase(std::remove_if(result.closest_approaches.begin(),result.closest_approaches.end(),[&](const EncounterEvent& x){return x.ut_s>first_unsafe->ut_s;}),result.closest_approaches.end());
        result.unsafe=first_unsafe;result.success=false;result.final_ut_s=first_unsafe->ut_s;result.final_state=first_unsafe->spacecraft_state;
    }
}
}
SpacecraftResult propagate(const Snapshot& s,const Ephemeris& e,State initial,const SpacecraftSettings& q){
    validate(s,e,initial,q);
    SpacecraftResult result;result.success=true;result.final_state=initial;result.final_ut_s=q.start_ut_s;
    result.requested_abs_position_tolerance_m=q.abs_position_tolerance_m;result.requested_abs_velocity_tolerance_mps=q.abs_velocity_tolerance_mps;
    result.requested_relative_tolerance=q.relative_tolerance;result.ephemeris_snapshot_hash=e.metadata.snapshot_hash;
    result.ephemeris_integrator=e.metadata.integrator+"/"+e.metadata.integrator_version;result.ephemeris_force_model=e.metadata.force_model;
    result.ephemeris_metadata=e.metadata;
    for(std::size_t i=0;i<s.bodies.size();++i)if(clearance(s,e,initial,q.start_ut_s,i,q)<=0){
        result.success=false;result.unsafe=event(s,e,initial,q.start_ut_s,i,q);return result;
    }
    auto burns=q.burns;std::stable_sort(burns.begin(),burns.end(),[](auto& a,auto& b){return a.ut_s<b.ut_s;});
    std::size_t burn_index=0;double t=q.start_ut_s,h=q.max_step_s;State y=initial;
    while(true){
        while(burn_index<burns.size()&&burns[burn_index].ut_s==t){
            const auto& b=burns[burn_index++];
            for(std::size_t i=0;i<s.bodies.size();++i)if(clearance(s,e,y,t,i,q)<=0)throw SpacecraftError("burn inside body or safety surface",t,y);
            State after=y;after.velocity_mps=after.velocity_mps+b.delta_v_mps;
            result.burns.push_back({t,y,after,b.delta_v_mps,norm(b.delta_v_mps)});y=after;result.final_state=y;
        }
        if(t>=q.end_ut_s)break;
        const double target=burn_index<burns.size()?std::min(q.end_ut_s,burns[burn_index].ut_s):q.end_ut_s;
        double dt=std::min(h,target-t);
        if(dt<=0)throw SpacecraftError("step made no progress",t,y);
        // The exact remainder to a burn or endpoint may be shorter than min_step_s.
        if(dt<q.min_step_s&&target-t>q.min_step_s*1e-9&&h<q.min_step_s)throw SpacecraftError("minimum step reached before tolerance",t,y);
        Trial candidate;
        try{candidate=trial(s,e,t,y,dt);}catch(const EphemerisError& error){throw SpacecraftError(std::string("ephemeris coverage: ")+error.what(),t,y);}
        const double p_error=norm(candidate.high.position_m-candidate.low.position_m);
        const double v_error=norm(candidate.high.velocity_mps-candidate.low.velocity_mps);
        const auto [near_start,speed_start]=nearest_relative_scales(s,e,y,t);
        const auto [near_end,speed_end]=nearest_relative_scales(s,e,candidate.high,t+dt);
        const double p_scale=q.abs_position_tolerance_m+q.relative_tolerance*std::max(near_start,near_end);
        const double v_scale=q.abs_velocity_tolerance_mps+q.relative_tolerance*std::max(speed_start,speed_end);
        const double error=std::max(p_error/p_scale,v_error/v_scale);
        if(!finite(error))throw SpacecraftError("adaptive error nonfinite",t,y);
        if(error>1){
            ++result.rejected_steps;const double factor=std::clamp(0.9*std::pow(error,-0.2),0.1,0.5);
            h=dt*factor;if(h<q.min_step_s)throw SpacecraftError("minimum step reached before tolerance",t,y);continue;
        }
        if(!clear_interval_bound(s,e,q,y,t,dt)){
            bool endpoint_inside=false;
            for(std::size_t i=0;i<s.bodies.size();++i)
                endpoint_inside|=clearance(s,e,candidate.high,t+dt,i,q)<=0;
            if(endpoint_inside){
                events(s,e,q,result,t,y,dt,candidate.high);
                if(result.unsafe)return result;
            }
            ++result.rejected_steps;
            if(dt/2<q.min_step_s){
                // All committed prior steps were bounded clear. Look ahead only
                // to bracket an entry; never advance a successful state here.
                const double horizon=target-t;
                double probe=std::min(horizon,std::max(dt,q.min_step_s));
                for(int attempt=0;attempt<32;++attempt){
                    const auto future=trial(s,e,t,y,probe).high;
                    bool crossed=false;
                    for(std::size_t i=0;i<s.bodies.size();++i)
                        crossed|=clearance(s,e,future,t+probe,i,q)<=0;
                    if(crossed){
                        events(s,e,q,result,t,y,probe,future);
                        if(result.unsafe)return result;
                        break;
                    }
                    if(probe>=horizon)break;
                    probe=std::min(horizon,probe*2);
                }
                throw SpacecraftError("unsafe interval unresolved at minimum step",t,y);
            }
            h=dt/2;continue;
        }
        if(result.accepted_steps>=q.max_accepted_steps)throw SpacecraftError("accepted step limit",t,y);
        events(s,e,q,result,t,y,dt,candidate.high);
        if(result.unsafe)return result;
        ++result.accepted_steps;result.smallest_accepted_step_s=result.accepted_steps==1?dt:std::min(result.smallest_accepted_step_s,dt);
        result.largest_accepted_step_s=std::max(result.largest_accepted_step_s,dt);
        result.max_accepted_position_local_error_m=std::max(result.max_accepted_position_local_error_m,p_error);
        result.max_accepted_velocity_local_error_mps=std::max(result.max_accepted_velocity_local_error_mps,v_error);
        result.max_accepted_normalized_error=std::max(result.max_accepted_normalized_error,error);
        t=target-t==dt?target:t+dt;y=candidate.high;result.final_state=y;result.final_ut_s=t;
        h=std::clamp(dt*std::clamp(0.9*std::pow(std::max(error,1e-16),-0.2),0.2,5.0),q.min_step_s,q.max_step_s);
    }
    return result;
}
}
