#include "MissionRoute.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ksp {
namespace {
bool finite(double x){return std::isfinite(x);}
bool finite(Vec3 x){return finite(x.x)&&finite(x.y)&&finite(x.z);}
const Body& body(const Snapshot& s,const std::string& id){
    auto it=std::find_if(s.bodies.begin(),s.bodies.end(),[&](const Body& b){return b.id==id;});
    if(it==s.bodies.end())throw SearchError("unknown route body: "+id);
    if(!finite(it->mu_m3_s2)||it->mu_m3_s2<=0||!finite(it->radius_m)||it->radius_m<=0)throw SearchError("route body radius or gravity invalid: "+id);
    return *it;
}
void atmosphere(std::optional<double> altitude,const char* name){
    if(!altitude||!finite(*altitude)||*altitude<0)throw SearchError(std::string(name)+" atmosphere unknown or invalid");
}
DatedLegCandidate validate_leg(const Snapshot& s,const Ephemeris& e,const RouteRequest& q,const DatedLegCandidate& c,
                  const std::string& from,const std::string& to){
    if(c.central_body_id!=q.central_body_id||c.departure_body_id!=from||c.arrival_body_id!=to)throw SearchError("role order or central body mismatch");
    if(c.snapshot_hash!=s.snapshot_hash||c.ephemeris_metadata.snapshot_hash!=e.metadata.snapshot_hash)throw SearchError("snapshot_hash mismatch in leg");
    if(c.source_confidence!=s.confidence||c.ephemeris_metadata.source_confidence!=e.metadata.source_confidence)throw SearchError("source confidence mismatch in leg");
    if(c.frame_origin!=s.frame.origin||c.frame_axes!=s.frame.axes||c.ephemeris_metadata.frame_origin!=e.metadata.frame_origin||
       c.ephemeris_metadata.frame_axes!=e.metadata.frame_axes||c.ephemeris_metadata.frame_handedness!=e.metadata.frame_handedness||
       !c.ephemeris_metadata.frame_inertial)throw SearchError("frame mismatch in leg");
    const auto& m=c.ephemeris_metadata;const auto& p=e.metadata;
    if(m.start_ut_s!=p.start_ut_s||m.end_ut_s!=p.end_ut_s||m.step_s!=p.step_s||m.state_epoch_ut_s!=p.state_epoch_ut_s||
       m.integrator!=p.integrator||m.integrator_version!=p.integrator_version||m.force_model!=p.force_model||m.result_label!=p.result_label||
       m.interpolation!=p.interpolation||m.segment_spacing_s!=p.segment_spacing_s||m.safety_split_count!=p.safety_split_count||
       m.max_position_fit_error_m!=p.max_position_fit_error_m||m.max_velocity_fit_error_mps!=p.max_velocity_fit_error_mps||
       m.measured_max_position_error_m!=p.measured_max_position_error_m||m.measured_max_velocity_error_mps!=p.measured_max_velocity_error_mps)
        throw SearchError("ephemeris identity mismatch in leg");
    if(!finite(c.departure_ut_s)||!finite(c.arrival_ut_s)||!finite(c.flight_time_s)||
       c.flight_time_s<=0||c.arrival_ut_s<=c.departure_ut_s||std::abs(c.arrival_ut_s-c.departure_ut_s-c.flight_time_s)>q.time_tolerance_s||
       c.departure_ut_s<p.start_ut_s||c.arrival_ut_s>p.end_ut_s)throw SearchError("leg time or coverage invalid");
    if(!finite(c.departure_barycentric_velocity_mps)||!finite(c.arrival_barycentric_velocity_mps))throw SearchError("leg velocity invalid");
    const auto origin=e.query(from,c.departure_ut_s),target=e.query(to,c.arrival_ut_s);
    const auto centre0=e.query(q.central_body_id,c.departure_ut_s),centre1=e.query(q.central_body_id,c.arrival_ut_s);
    LambertRequest request;request.departure_relative_position_m=origin.position_m-centre0.position_m;
    request.arrival_relative_position_m=target.position_m-centre1.position_m;
    request.reference_normal=q.reference_normal;request.mu_m3_s2=body(s,q.central_body_id).mu_m3_s2;
    request.time_of_flight_s=c.flight_time_s;request.branch=c.branch;request.direction=c.direction;
    request.max_position_residual_m=q.max_lambert_position_residual_m;
    request.max_velocity_residual_mps=q.max_lambert_velocity_residual_mps;
    LambertResult solved;
    try{solved=solve_lambert(request);}catch(const SearchError& error){throw SearchError(std::string("Lambert leg failed: ")+error.what());}
    const auto expected0=solved.departure_relative_velocity_mps+centre0.velocity_mps;
    const auto expected1=solved.arrival_relative_velocity_mps+centre1.velocity_mps;
    const double actual0=norm(expected0-origin.velocity_mps),actual1=norm(expected1-target.velocity_mps);
    if(c.lambert.branch!=c.branch||c.lambert.direction!=c.direction||
       !finite(c.lambert.departure_relative_velocity_mps)||!finite(c.lambert.arrival_relative_velocity_mps)||
       !finite(c.lambert.position_residual_m)||!finite(c.lambert.velocity_residual_mps)||
       c.lambert.position_residual_m<0||c.lambert.velocity_residual_mps<0||
       c.lambert.position_residual_m>q.max_lambert_position_residual_m||c.lambert.velocity_residual_mps>q.max_lambert_velocity_residual_mps||
       norm(c.departure_barycentric_velocity_mps-expected0)>q.max_lambert_velocity_residual_mps||
       norm(c.arrival_barycentric_velocity_mps-expected1)>q.max_lambert_velocity_residual_mps||
       norm(c.lambert.departure_relative_velocity_mps-solved.departure_relative_velocity_mps)>q.max_lambert_velocity_residual_mps||
       norm(c.lambert.arrival_relative_velocity_mps-solved.arrival_relative_velocity_mps)>q.max_lambert_velocity_residual_mps||
       !finite(c.departure_vinf_mps)||!finite(c.arrival_vinf_mps)||!finite(c.screening_score_mps)||
       std::abs(c.departure_vinf_mps-actual0)>q.max_lambert_velocity_residual_mps||
       std::abs(c.arrival_vinf_mps-actual1)>q.max_lambert_velocity_residual_mps||
       std::abs(c.screening_score_mps-actual0-actual1)>2*q.max_lambert_velocity_residual_mps)
        throw SearchError("Lambert candidate inconsistent with ephemeris");
    auto canonical=c;
    canonical.lambert=solved;
    canonical.departure_barycentric_velocity_mps=expected0;
    canonical.arrival_barycentric_velocity_mps=expected1;
    canonical.departure_vinf_mps=actual0;
    canonical.arrival_vinf_mps=actual1;
    canonical.screening_score_mps=actual0+actual1;
    return canonical;
}
double parking_burn(double vinf,const Body& b,double altitude){
    const double radius=b.radius_m+altitude;
    const double value=std::sqrt(vinf*vinf+2*b.mu_m3_s2/radius)-std::sqrt(b.mu_m3_s2/radius);
    if(!finite(value)||value<0)throw SearchError("parking burn nonfinite");return value;
}
bool better(const ScreenedRoute& a,const ScreenedRoute& b){
    if(a.total_optimistic_delta_v_mps!=b.total_optimistic_delta_v_mps)return a.total_optimistic_delta_v_mps<b.total_optimistic_delta_v_mps;
    if(a.launch_ut_s!=b.launch_ut_s)return a.launch_ut_s<b.launch_ut_s;
    if(a.return_ut_s!=b.return_ut_s)return a.return_ut_s<b.return_ut_s;
    return a.route_id<b.route_id;
}
}
RouteResult assemble_routes(const Snapshot& s,const Ephemeris& e,const RouteRequest& q){
    if(!s.analysis_ready||s.snapshot_hash.empty()||s.confidence.empty())throw SearchError("analysis_ready source required");
    if(!s.state_epoch_ut_s||!finite(*s.state_epoch_ut_s)||*s.state_epoch_ut_s!=e.metadata.state_epoch_ut_s)throw SearchError("state epoch mismatch");
    if(s.snapshot_hash!=e.metadata.snapshot_hash||s.confidence!=e.metadata.source_confidence)throw SearchError("snapshot_hash or source confidence mismatch");
    if(!s.frame.inertial||s.frame.handedness!="right"||!e.metadata.frame_inertial||e.metadata.frame_handedness!="right"||
       s.frame.origin!=e.metadata.frame_origin||s.frame.axes!=e.metadata.frame_axes)throw SearchError("frame mismatch");
    if(e.metadata.force_model!="newtonian_point_mass"||e.metadata.result_label!="independent_newtonian_nbody")throw SearchError("force model mismatch");
    if(q.central_body_id.empty()||q.home_body_id.empty()||q.mars_body_id.empty()||q.venus_body_id.empty()||
       q.central_body_id==q.home_body_id||q.central_body_id==q.mars_body_id||q.central_body_id==q.venus_body_id||
       q.home_body_id==q.mars_body_id||q.home_body_id==q.venus_body_id||q.mars_body_id==q.venus_body_id)throw SearchError("duplicated route roles");
    const auto& home=body(s,q.home_body_id);const auto& mars=body(s,q.mars_body_id);const auto& venus=body(s,q.venus_body_id);body(s,q.central_body_id);
    for(const auto& b:s.bodies)if(!b.state||!b.epoch_ut_s||*b.epoch_ut_s!=*s.state_epoch_ut_s||
        !finite(b.state->position_m)||!finite(b.state->velocity_mps))throw SearchError("incomplete body state or epoch");
    for(const auto& id:{q.central_body_id,q.home_body_id,q.mars_body_id,q.venus_body_id})
        if(std::find(e.body_ids.begin(),e.body_ids.end(),id)==e.body_ids.end())throw SearchError("ephemeris body identity missing");
    atmosphere(q.home_atmosphere_altitude_m,"home");atmosphere(q.mars_atmosphere_altitude_m,"mars");atmosphere(q.venus_atmosphere_altitude_m,"venus");
    if(!finite(q.home_parking_altitude_m)||!finite(q.mars_parking_altitude_m)||!finite(q.return_capture_altitude_m)||
       q.home_parking_altitude_m<*q.home_atmosphere_altitude_m||q.mars_parking_altitude_m<*q.mars_atmosphere_altitude_m||
       q.return_capture_altitude_m<*q.home_atmosphere_altitude_m)throw SearchError("parking altitude invalid or below atmosphere");
    if(q.return_condition!=ReturnCondition::parking_capture)throw SearchError("entry return unsupported");
    if(!finite(q.launch_start_ut_s)||!finite(q.launch_end_ut_s)||q.launch_end_ut_s<q.launch_start_ut_s||
       q.launch_start_ut_s<e.metadata.start_ut_s||q.launch_end_ut_s>e.metadata.end_ut_s||
       !finite(q.fixed_stay_s)||!finite(q.time_tolerance_s)||q.time_tolerance_s<0||q.time_tolerance_s>1||
       std::abs(q.fixed_stay_s-5184000)>q.time_tolerance_s||!finite(q.max_total_duration_s)||q.max_total_duration_s<=0)
       throw SearchError("route date, fixed stay, or duration invalid");
    if(!finite(q.reference_normal)||norm(q.reference_normal)==0||!finite(q.max_lambert_position_residual_m)||
       q.max_lambert_position_residual_m<=0||!finite(q.max_lambert_velocity_residual_mps)||
       q.max_lambert_velocity_residual_mps<=0)throw SearchError("Lambert verification bounds invalid");
    if(!finite(q.venus_safety_margin_m)||q.venus_safety_margin_m<0||!finite(q.venus_maximum_periapsis_m)||
       q.venus_maximum_periapsis_m<=venus.radius_m+*q.venus_atmosphere_altitude_m+q.venus_safety_margin_m||
       !finite(q.venus_speed_tolerance_mps)||q.venus_speed_tolerance_mps<0)throw SearchError("flyby limits invalid");
    if(q.max_combinations==0||q.max_combinations>1000000||q.max_routes==0||q.max_routes>1000)
        throw SearchError("combination or route limit invalid");
    if(q.home_mars.size()>q.max_combinations||q.mars_venus.size()>q.max_combinations||q.venus_home.size()>q.max_combinations||
       (!q.mars_venus.empty()&&q.home_mars.size()>q.max_combinations/q.mars_venus.size())||
       (!q.venus_home.empty()&&q.home_mars.size()*q.mars_venus.size()>q.max_combinations/q.venus_home.size()))throw SearchError("combination limit exceeded");
    std::vector<DatedLegCandidate> verified_home_mars,verified_mars_venus,verified_venus_home;
    verified_home_mars.reserve(q.home_mars.size());verified_mars_venus.reserve(q.mars_venus.size());verified_venus_home.reserve(q.venus_home.size());
    for(const auto& c:q.home_mars)verified_home_mars.push_back(validate_leg(s,e,q,c,q.home_body_id,q.mars_body_id));
    for(const auto& c:q.mars_venus)verified_mars_venus.push_back(validate_leg(s,e,q,c,q.mars_body_id,q.venus_body_id));
    for(const auto& c:q.venus_home)verified_venus_home.push_back(validate_leg(s,e,q,c,q.venus_body_id,q.home_body_id));
    RouteResult out;out.diagnostic="no screened route in window";bool stay_mismatch=false,flyby_mismatch=false;
    for(std::size_t i=0;i<q.home_mars.size();++i)for(std::size_t j=0;j<q.mars_venus.size();++j)for(std::size_t k=0;k<q.venus_home.size();++k){
        ++out.considered_combinations;
        const auto& first=verified_home_mars[i];const auto& second=verified_mars_venus[j];const auto& third=verified_venus_home[k];
        if(first.departure_ut_s<q.launch_start_ut_s||first.departure_ut_s>q.launch_end_ut_s){++out.rejected_combinations;continue;}
        if(std::abs((second.departure_ut_s-first.arrival_ut_s)-5184000.0)>q.time_tolerance_s){stay_mismatch=true;++out.rejected_combinations;continue;}
        if(third.departure_ut_s!=second.arrival_ut_s){flyby_mismatch=true;++out.rejected_combinations;continue;}
        if(third.arrival_ut_s-first.departure_ut_s>q.max_total_duration_s){++out.rejected_combinations;continue;}
        const auto h0=e.query(q.home_body_id,first.departure_ut_s),m1=e.query(q.mars_body_id,first.arrival_ut_s);
        const auto m2=e.query(q.mars_body_id,second.departure_ut_s),v=e.query(q.venus_body_id,second.arrival_ut_s);
        const auto h3=e.query(q.home_body_id,third.arrival_ut_s);
        FlybyRequest flyby;flyby.incoming_vinf_mps=second.arrival_barycentric_velocity_mps-v.velocity_mps;
        flyby.outgoing_vinf_mps=third.departure_barycentric_velocity_mps-v.velocity_mps;
        flyby.mu_planet_m3_s2=venus.mu_m3_s2;flyby.radius_m=venus.radius_m;flyby.atmosphere_altitude_m=q.venus_atmosphere_altitude_m;
        flyby.safety_margin_m=q.venus_safety_margin_m;flyby.speed_tolerance_mps=q.venus_speed_tolerance_mps;flyby.maximum_periapsis_m=q.venus_maximum_periapsis_m;
        FlybyResult assist;try{assist=screen_unpowered_flyby(flyby);}catch(const SearchError&){++out.rejected_combinations;continue;}
        ScreenedRoute route;route.route_id=s.snapshot_hash+":"+std::to_string(i)+":"+std::to_string(j)+":"+std::to_string(k);
        route.snapshot_hash=s.snapshot_hash;route.source_confidence=s.confidence;route.home_mars=first;route.mars_venus=second;route.venus_home=third;route.flyby=assist;
        const double home_departure_vinf=norm(first.departure_barycentric_velocity_mps-h0.velocity_mps);
        const double mars_arrival_vinf=norm(first.arrival_barycentric_velocity_mps-m1.velocity_mps);
        const double mars_departure_vinf=norm(second.departure_barycentric_velocity_mps-m2.velocity_mps);
        const double home_return_vinf=norm(third.arrival_barycentric_velocity_mps-h3.velocity_mps);
        route.home_injection_mps=parking_burn(home_departure_vinf,home,q.home_parking_altitude_m);
        route.mars_capture_mps=parking_burn(mars_arrival_vinf,mars,q.mars_parking_altitude_m);
        route.mars_departure_mps=parking_burn(mars_departure_vinf,mars,q.mars_parking_altitude_m);
        route.home_return_capture_mps=parking_burn(home_return_vinf,home,q.return_capture_altitude_m);
        route.departure_c3_m2_s2=home_departure_vinf*home_departure_vinf;route.return_c3_m2_s2=home_return_vinf*home_return_vinf;
        route.total_optimistic_delta_v_mps=route.home_injection_mps+route.mars_capture_mps+route.mars_departure_mps+route.home_return_capture_mps;
        route.launch_ut_s=first.departure_ut_s;route.return_ut_s=third.arrival_ut_s;route.total_duration_s=route.return_ut_s-route.launch_ut_s;
        route.fixed_stay_s=5184000.0;
        if(out.routes.size()<q.max_routes){out.routes.push_back(std::move(route));std::push_heap(out.routes.begin(),out.routes.end(),better);}
        else if(better(route,out.routes.front())){
            std::pop_heap(out.routes.begin(),out.routes.end(),better);
            out.routes.back()=std::move(route);std::push_heap(out.routes.begin(),out.routes.end(),better);
        }
        out.peak_retained_routes=std::max(out.peak_retained_routes,out.routes.size());
    }
    if(out.routes.empty()&&out.considered_combinations==1&&stay_mismatch)throw SearchError("fixed stay time mismatch");
    if(out.routes.empty()&&out.considered_combinations==1&&flyby_mismatch)throw SearchError("flyby time mismatch");
    std::sort(out.routes.begin(),out.routes.end(),better);
    if(!out.routes.empty())out.diagnostic="patched-conic screened routes";
    return out;
}
}
