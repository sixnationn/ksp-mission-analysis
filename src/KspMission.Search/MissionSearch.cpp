#include "MissionSearch.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace ksp {
namespace {
bool finite(double x){return std::isfinite(x);}
std::size_t count(double first,double last,double step,const char* label){
    if(!finite(first)||!finite(last)||last<first||!finite(step)||step<=0)
        throw SearchError(std::string(label)+" grid range or step invalid");
    const double intervals=(last-first)/step;
    if(!finite(intervals)||intervals>100000)
        throw SearchError(std::string(label)+" grid cell limit exceeded");
    if(std::abs(intervals-std::round(intervals))>1e-9)
        throw SearchError(std::string(label)+" grid requires integer steps");
    return static_cast<std::size_t>(std::llround(intervals))+1;
}
std::size_t bounded_add(std::size_t a,std::size_t b,std::size_t limit){
    if(b>limit||a>limit-b)throw SearchError("mission search cell limit exceeded");
    return a+b;
}
std::size_t bounded_multiply(std::size_t a,std::size_t b,std::size_t limit){
    if(b!=0&&a>limit/b)throw SearchError("mission search cell limit exceeded");
    return a*b;
}
double at(double first,double last,double step,std::size_t index,std::size_t size){
    return index+1==size?last:first+static_cast<double>(index)*step;
}
bool better(const ScreenedRoute& a,const ScreenedRoute& b){
    if(a.total_optimistic_delta_v_mps!=b.total_optimistic_delta_v_mps)
        return a.total_optimistic_delta_v_mps<b.total_optimistic_delta_v_mps;
    if(a.launch_ut_s!=b.launch_ut_s)return a.launch_ut_s<b.launch_ut_s;
    if(a.return_ut_s!=b.return_ut_s)return a.return_ut_s<b.return_ut_s;
    return a.route_id<b.route_id;
}
std::string route_id(const RouteRequest& q,const ScreenedRoute& r){
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::hexfloat;
    out<<r.snapshot_hash<<':'<<q.central_body_id<<':'<<q.home_body_id<<':'<<q.mars_body_id<<':'<<q.venus_body_id;
    for(const auto& leg:{r.home_mars,r.mars_venus,r.venus_home})
        out<<':'<<static_cast<int>(leg.branch)<<':'<<static_cast<int>(leg.direction)
           <<':'<<leg.departure_ut_s<<':'<<leg.arrival_ut_s;
    return out.str();
}
GridRequest one_cell(const MissionSearchRequest& q,const std::string& from,const std::string& to,
                     double departure,double flight,const FlightGrid& grid){
    GridRequest cell;cell.central_body_id=q.route.central_body_id;
    cell.departure_body_id=from;cell.arrival_body_id=to;
    cell.launch_start_ut_s=departure;cell.launch_end_ut_s=departure;cell.launch_step_s=1;
    cell.flight_min_s=flight;cell.flight_max_s=flight;cell.flight_step_s=1;
    cell.reference_normal=q.route.reference_normal;cell.branch=grid.branch;cell.direction=grid.direction;
    cell.max_position_residual_m=q.route.max_lambert_position_residual_m;
    cell.max_velocity_residual_mps=q.route.max_lambert_velocity_residual_mps;
    cell.central_atmosphere_altitude_m=q.central_atmosphere_altitude_m;
    cell.central_safety_margin_m=q.central_safety_margin_m;
    cell.max_candidates=1;cell.max_grid_cells=1;return cell;
}
void report(const MissionSearchResult& out,const MissionSearchProgressCallback& progress){
    if(progress)progress({out.sampled_cells,out.total_upper_bound_cells,out.rejected_cells,
                          out.considered_combinations,out.rejected_route_combinations,out.routes.size()});
}
}
MissionSearchResult search_mission(const Snapshot& s,const Ephemeris& e,const MissionSearchRequest& q,
    MissionSearchCancellation cancelled,MissionSearchProgressCallback progress){
    if(!q.route.home_mars.empty()||!q.route.mars_venus.empty()||!q.route.venus_home.empty())
        throw SearchError("mission search seed vectors must be empty");
    const auto launches=count(q.route.launch_start_ut_s,q.route.launch_end_ut_s,q.launch_step_s,"launch");
    std::array<std::size_t,3> flights{};
    for(std::size_t i=0;i<3;++i){
        flights[i]=count(q.legs[i].min_s,q.legs[i].max_s,q.legs[i].step_s,"flight");
        if(q.legs[i].min_s<=0)throw SearchError("flight grid time must be positive");
    }
    if(q.max_cells==0||q.max_cells>100000||q.max_routes==0||q.max_routes>1000)
        throw SearchError("mission search cell limit or route limit invalid");
    if(!q.central_atmosphere_altitude_m||!finite(*q.central_atmosphere_altitude_m)||
       *q.central_atmosphere_altitude_m<0||!finite(q.central_safety_margin_m)||q.central_safety_margin_m<0)
        throw SearchError("central atmosphere or safety margin unknown or invalid");
    const auto first=bounded_multiply(launches,flights[0],q.max_cells);
    const auto second=bounded_multiply(first,flights[1],q.max_cells);
    const auto third=bounded_multiply(second,flights[2],q.max_cells);
    const auto total=bounded_add(bounded_add(first,second,q.max_cells),third,q.max_cells);
    if(q.route.fixed_stay_s!=5184000.0)throw SearchError("fixed stay must be exactly 5184000 seconds");
    const double last_return=q.route.launch_end_ut_s+q.legs[0].max_s+5184000.0+
                             q.legs[1].max_s+q.legs[2].max_s;
    if(!finite(last_return)||last_return>e.metadata.end_ut_s||q.route.launch_start_ut_s<e.metadata.start_ut_s)
        throw SearchError("ephemeris coverage gap in mission search");
    RouteRequest route=q.route;route.max_combinations=1;route.max_routes=1;
    // The assembler checks source, frame, body roles, epochs and every route constraint.
    assemble_routes(s,e,route);
    MissionSearchResult out;out.snapshot_hash=s.snapshot_hash;out.source_confidence=s.confidence;
    out.total_upper_bound_cells=total;out.diagnostic="no screened route in window";
    auto stopped=[&](){if(cancelled&&cancelled()){
        out.cancelled=true;out.diagnostic="cancelled with partial screened results";return true;
    }return false;};
    auto screen=[&](const std::string& from,const std::string& to,double departure,double flight,
                    const FlightGrid& grid)->std::vector<DatedLegCandidate>{
        auto result=screen_single_leg(s,e,one_cell(q,from,to,departure,flight,grid));
        ++out.sampled_cells;out.rejected_cells+=result.rejected_cells;report(out,progress);
        return result.candidates;
    };
    for(std::size_t li=0;li<launches;++li){
        const double launch=at(q.route.launch_start_ut_s,q.route.launch_end_ut_s,q.launch_step_s,li,launches);
        for(std::size_t i=0;i<flights[0];++i){
            if(stopped())return out;
            const double first_time=at(q.legs[0].min_s,q.legs[0].max_s,q.legs[0].step_s,i,flights[0]);
            auto first_candidates=screen(q.route.home_body_id,q.route.mars_body_id,launch,first_time,q.legs[0]);
            if(first_candidates.empty())continue;
            const auto& leg1=first_candidates.front();
            const double mars_departure=leg1.arrival_ut_s+5184000.0;
            for(std::size_t j=0;j<flights[1];++j){
                if(stopped())return out;
                const double second_time=at(q.legs[1].min_s,q.legs[1].max_s,q.legs[1].step_s,j,flights[1]);
                auto second_candidates=screen(q.route.mars_body_id,q.route.venus_body_id,mars_departure,second_time,q.legs[1]);
                if(second_candidates.empty())continue;
                const auto& leg2=second_candidates.front();
                for(std::size_t k=0;k<flights[2];++k){
                    if(stopped())return out;
                    const double third_time=at(q.legs[2].min_s,q.legs[2].max_s,q.legs[2].step_s,k,flights[2]);
                    auto third_candidates=screen(q.route.venus_body_id,q.route.home_body_id,leg2.arrival_ut_s,third_time,q.legs[2]);
                    if(third_candidates.empty())continue;
                    route.home_mars={leg1};route.mars_venus={leg2};route.venus_home={third_candidates.front()};
                    route.launch_start_ut_s=launch;route.launch_end_ut_s=launch;
                    auto assembled=assemble_routes(s,e,route);
                    out.considered_combinations+=assembled.considered_combinations;
                    out.rejected_route_combinations+=assembled.rejected_combinations;
                    if(assembled.routes.empty())continue;
                    auto candidate=std::move(assembled.routes.front());
                    candidate.route_id=route_id(q.route,candidate);
                    if(out.routes.size()<q.max_routes){
                        out.routes.push_back(std::move(candidate));
                        std::sort(out.routes.begin(),out.routes.end(),better);
                    }else if(better(candidate,out.routes.back())){
                        out.routes.back()=std::move(candidate);
                        std::sort(out.routes.begin(),out.routes.end(),better);
                    }
                    out.peak_retained_routes=std::max(out.peak_retained_routes,out.routes.size());
                    out.diagnostic="patched-conic screened routes";
                    report(out,progress);
                }
            }
        }
    }
    return out;
}
}
