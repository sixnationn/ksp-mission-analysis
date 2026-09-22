#include "SearchGrid.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ksp {
namespace {
bool finite(double x){return std::isfinite(x);}bool finite(Vec3 x){return finite(x.x)&&finite(x.y)&&finite(x.z);}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
bool better(const DatedLegCandidate& a,const DatedLegCandidate& b){
    if(a.screening_score_mps!=b.screening_score_mps)return a.screening_score_mps<b.screening_score_mps;
    if(a.tie_key!=b.tie_key)return a.tie_key<b.tie_key;
    if(a.departure_ut_s!=b.departure_ut_s)return a.departure_ut_s<b.departure_ut_s;
    return a.flight_time_s<b.flight_time_s;
}
bool clears_central_body(Vec3 r,Vec3 v,double mu,double clearance){
    const double radius=norm(r);
    if(!finite(radius)||radius<=clearance)return false;
    const Vec3 h=cross(r,v);
    const Vec3 eccentricity=cross(v,h)*(1/mu)-r*(1/radius);
    const double periapsis=dot(h,h)/(mu*(1+norm(eccentricity)));
    return finite(periapsis)&&periapsis>clearance;
}
std::uint64_t mix(std::uint64_t x){x+=0x9e3779b97f4a7c15ULL;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;return x^(x>>31);}
std::size_t count(double start,double end,double step,const char* name){
    if(!finite(start)||!finite(end)||end<start)throw SearchError(std::string(name)+" range invalid");
    if(!finite(step)||step<=0)throw SearchError(std::string(name)+"_step_s invalid");
    const double intervals=(end-start)/step;
    if(!finite(intervals)||intervals>1000000)throw SearchError(std::string(name)+" grid excessive");
    if(std::abs(intervals-std::round(intervals))>1e-9)throw SearchError(std::string(name)+" interval must be integer steps");
    return static_cast<std::size_t>(std::llround(intervals))+1;
}
const Body& body(const Snapshot& s,const std::string& id,const char* field){
    auto it=std::find_if(s.bodies.begin(),s.bodies.end(),[&](const Body& b){return b.id==id;});
    if(it==s.bodies.end())throw SearchError(std::string(field)+" missing: "+id);
    return *it;
}
void validate(const Snapshot& s,const Ephemeris& e,const GridRequest& q,std::size_t& launch_count,std::size_t& flight_count){
    if(!s.analysis_ready)throw SearchError("analysis_ready is false");
    if(s.snapshot_hash.empty()||e.metadata.snapshot_hash!=s.snapshot_hash)throw SearchError("snapshot_hash mismatch");
    if(!s.state_epoch_ut_s||!finite(*s.state_epoch_ut_s)||e.metadata.state_epoch_ut_s!=*s.state_epoch_ut_s)throw SearchError("epoch mismatch");
    if(s.confidence.empty()||e.metadata.source_confidence!=s.confidence)throw SearchError("source confidence mismatch");
    if(!s.frame.inertial||!e.metadata.frame_inertial||s.frame.handedness!="right"||e.metadata.frame_origin!=s.frame.origin||e.metadata.frame_axes!=s.frame.axes||e.metadata.frame_handedness!=s.frame.handedness)throw SearchError("frame mismatch");
    if(e.metadata.force_model!="newtonian_point_mass"||e.metadata.result_label!="independent_newtonian_nbody")throw SearchError("force model mismatch");
    if(!finite(e.metadata.measured_max_position_error_m)||!finite(e.metadata.measured_max_velocity_error_mps)||e.metadata.measured_max_position_error_m<0||e.metadata.measured_max_velocity_error_mps<0||
       e.metadata.measured_max_position_error_m>e.metadata.max_position_fit_error_m||e.metadata.measured_max_velocity_error_mps>e.metadata.max_velocity_fit_error_mps)throw SearchError("ephemeris fit invalid");
    const auto& central=body(s,q.central_body_id,"central_body_id");body(s,q.departure_body_id,"departure_body_id");body(s,q.arrival_body_id,"arrival_body_id");
    for(const auto& entry:s.bodies){
        if(!entry.state||!finite(entry.state->position_m)||!finite(entry.state->velocity_mps)||!entry.epoch_ut_s||*entry.epoch_ut_s!=*s.state_epoch_ut_s)
            throw SearchError("body state or epoch incomplete: "+entry.id);
    }
    if(!finite(central.mu_m3_s2)||central.mu_m3_s2<=0)throw SearchError("central mu invalid");
    if(!q.central_atmosphere_altitude_m||!finite(*q.central_atmosphere_altitude_m)||*q.central_atmosphere_altitude_m<0)
        throw SearchError("central atmosphere unknown or invalid");
    if(!finite(q.central_safety_margin_m)||q.central_safety_margin_m<0||
       !finite(central.radius_m+*q.central_atmosphere_altitude_m+q.central_safety_margin_m))
        throw SearchError("central safety margin invalid");
    if(q.central_body_id==q.departure_body_id||q.central_body_id==q.arrival_body_id)throw SearchError("central body cannot be endpoint");
    for(const auto& id:{q.central_body_id,q.departure_body_id,q.arrival_body_id})if(std::find(e.body_ids.begin(),e.body_ids.end(),id)==e.body_ids.end())throw SearchError("ephemeris body identity mismatch");
    launch_count=count(q.launch_start_ut_s,q.launch_end_ut_s,q.launch_step_s,"launch");
    flight_count=count(q.flight_min_s,q.flight_max_s,q.flight_step_s,"flight");
    if(q.flight_min_s<=0)throw SearchError("flight time must be positive");
    if(q.max_grid_cells==0||q.max_grid_cells>1000000||launch_count>q.max_grid_cells/flight_count)
        throw SearchError("grid cell limit exceeded");
    if(q.max_candidates==0)throw SearchError("max_candidates must be positive");
    if(!finite(q.reference_normal)||norm(q.reference_normal)==0)throw SearchError("reference_normal invalid");
    if(!finite(q.max_position_residual_m)||q.max_position_residual_m<=0||!finite(q.max_velocity_residual_mps)||q.max_velocity_residual_mps<=0)throw SearchError("residual bounds invalid");
    if(q.launch_start_ut_s<e.metadata.start_ut_s||q.launch_end_ut_s+q.flight_max_s>e.metadata.end_ut_s||!finite(q.launch_end_ut_s+q.flight_max_s))throw SearchError("coverage gap in grid");
}
}
GridResult screen_single_leg(const Snapshot& s,const Ephemeris& e,const GridRequest& q){
    std::size_t launches=0,flights=0;validate(s,e,q,launches,flights);
    GridResult out;out.snapshot_hash=s.snapshot_hash;out.ephemeris_metadata=e.metadata;out.stable_seed=q.stable_seed;
    const auto& central=body(s,q.central_body_id,"central_body_id");
    const double central_clearance=central.radius_m+*q.central_atmosphere_altitude_m+q.central_safety_margin_m;
    for(std::size_t li=0;li<launches;++li){
        const double departure=li+1==launches?q.launch_end_ut_s:q.launch_start_ut_s+li*q.launch_step_s;
        const auto center_start=e.query(q.central_body_id,departure),origin=e.query(q.departure_body_id,departure);
        for(std::size_t fi=0;fi<flights;++fi){
            ++out.sampled_cells;
            const double flight=fi+1==flights?q.flight_max_s:q.flight_min_s+fi*q.flight_step_s;
            const double arrival=departure+flight;
            const auto center_end=e.query(q.central_body_id,arrival),target=e.query(q.arrival_body_id,arrival);
            LambertRequest request;
            request.departure_relative_position_m=origin.position_m-center_start.position_m;
            request.arrival_relative_position_m=target.position_m-center_end.position_m;
            request.reference_normal=q.reference_normal;request.mu_m3_s2=central.mu_m3_s2;request.time_of_flight_s=flight;
            request.branch=q.branch;request.direction=q.direction;request.max_position_residual_m=q.max_position_residual_m;
            request.max_velocity_residual_mps=q.max_velocity_residual_mps;
            try{
                auto solved=solve_lambert(request);
                if(!clears_central_body(request.departure_relative_position_m,solved.departure_relative_velocity_mps,
                                        central.mu_m3_s2,central_clearance)){
                    ++out.rejected_cells;continue;
                }
                const Vec3 v0=solved.departure_relative_velocity_mps+center_start.velocity_mps;
                const Vec3 v1=solved.arrival_relative_velocity_mps+center_end.velocity_mps;
                const double departure_dv=norm(v0-origin.velocity_mps),arrival_dv=norm(v1-target.velocity_mps);
                if(!finite(departure_dv)||!finite(arrival_dv)||!finite(departure_dv+arrival_dv)){++out.rejected_cells;continue;}
                const auto tie=mix(q.stable_seed^mix(li)^mix(fi+0x9e3779b97f4a7c15ULL));
                DatedLegCandidate candidate{departure,arrival,flight,q.central_body_id,q.departure_body_id,q.arrival_body_id,s.snapshot_hash,s.confidence,s.frame.origin,s.frame.axes,
                    e.metadata,q.branch,q.direction,solved,v0,v1,departure_dv,arrival_dv,departure_dv+arrival_dv,tie};
                if(out.candidates.size()<q.max_candidates){
                    out.candidates.push_back(std::move(candidate));
                    std::push_heap(out.candidates.begin(),out.candidates.end(),better);
                }else if(better(candidate,out.candidates.front())){
                    std::pop_heap(out.candidates.begin(),out.candidates.end(),better);
                    out.candidates.back()=std::move(candidate);
                    std::push_heap(out.candidates.begin(),out.candidates.end(),better);
                }
            }catch(const SearchError&){++out.rejected_cells;}
        }
    }
    std::sort(out.candidates.begin(),out.candidates.end(),better);
    out.diagnostic=out.candidates.empty()?"no feasible screened seed":"screened candidates";
    return out;
}
}
