#pragma once
#include "SearchGrid.hpp"
#include <optional>
#include <string>
#include <vector>

namespace ksp {
enum class ReturnCondition { parking_capture, entry };
struct RouteRequest {
    std::string central_body_id,home_body_id,mars_body_id,venus_body_id;
    std::vector<DatedLegCandidate> home_mars,mars_venus,venus_home;
    Vec3 reference_normal;
    double max_lambert_position_residual_m=0,max_lambert_velocity_residual_mps=0;
    double launch_start_ut_s=0,launch_end_ut_s=0,fixed_stay_s=0,time_tolerance_s=0,max_total_duration_s=0;
    double home_parking_altitude_m=0,mars_parking_altitude_m=0,return_capture_altitude_m=0;
    std::optional<double> home_atmosphere_altitude_m,mars_atmosphere_altitude_m,venus_atmosphere_altitude_m;
    double venus_safety_margin_m=0,venus_maximum_periapsis_m=0,venus_speed_tolerance_mps=0;
    std::size_t max_combinations=0,max_routes=0;
    ReturnCondition return_condition=ReturnCondition::parking_capture;
};
struct ScreenedRoute {
    std::string result_label="patched_conic_screened_route",route_id,snapshot_hash,source_confidence;
    DatedLegCandidate home_mars,mars_venus,venus_home;
    FlybyResult flyby;
    double home_injection_mps=0,mars_capture_mps=0,mars_departure_mps=0,home_return_capture_mps=0;
    double departure_c3_m2_s2=0,return_c3_m2_s2=0,total_optimistic_delta_v_mps=0;
    double launch_ut_s=0,return_ut_s=0,total_duration_s=0,fixed_stay_s=5184000;
    std::string stay_type="parking_orbit";
    std::string leg_verification="Lambert re-solved against source ephemeris; endpoint velocities canonicalized";
    std::string excluded_costs="plane changes, parking phasing, corrections, ascent/descent, finite burns";
};
struct RouteResult {std::vector<ScreenedRoute> routes;std::string diagnostic;std::size_t considered_combinations=0,rejected_combinations=0,peak_retained_routes=0;};
RouteResult assemble_routes(const Snapshot& snapshot,const Ephemeris& ephemeris,const RouteRequest& request);
}
