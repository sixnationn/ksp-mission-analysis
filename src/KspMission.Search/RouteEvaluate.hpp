#pragma once
#include "MissionRoute.hpp"
#include "Spacecraft.hpp"
#include <array>
#include <stdexcept>
#include <string>

namespace ksp {
struct RouteEvaluationRequest {
    ScreenedRoute route;
    std::string expected_snapshot_hash,expected_frame_origin,expected_frame_axes,expected_frame_handedness;
    double expected_state_epoch_ut_s=0;
    State launch_parking_state;
    std::array<Vec3,4> impulses_mps{};
    std::array<State,4> checkpoint_targets_relative{};
    double home_parking_altitude_m=0,mars_parking_altitude_m=0,home_capture_altitude_m=0;
    double fixed_position_tolerance_m=0,fixed_velocity_tolerance_mps=0;
    double parking_radius_tolerance_m=0,parking_radial_velocity_tolerance_mps=0,
           parking_tangential_speed_tolerance_mps=0;
    double venus_window_halfwidth_s=0,venus_max_encounter_radius_m=0,venus_safety_margin_m=0;
    double disagreement_position_m=0,disagreement_velocity_mps=0,disagreement_event_time_s=0;
    double disagreement_mars_extremum_radius_m=0,disagreement_mars_extremum_time_s=0;
    Settings coarse_ephemeris,strict_ephemeris;
    SpacecraftSettings coarse_spacecraft,strict_spacecraft;
    // Used only by evaluate_fixed_route_synthetic_fixture; runtime values come from exact bytes.
    std::vector<AtmosphereBoundary> atmosphere_boundaries;
};
struct RouteCheckpoint {
    std::string name;double ut_s=0;State spacecraft,relative;
    double position_error_m=0,velocity_error_mps=0,parking_radius_error_m=0,
           radial_velocity_mps=0,tangential_speed_error_mps=0;
};
struct RoutePass {
    std::array<RouteCheckpoint,4> checkpoints;
    EncounterEvent venus;
    RadiusExtrema mars_stay_radius;
    std::array<BurnRecord,4> burns;
    std::size_t accepted_steps=0,rejected_steps=0;
};
struct RouteEvaluationResult {
    // Parking is checked at named endpoints; the complete Mars stay is not certified.
    std::string result_label="independent_nbody_fixed_impulse_checkpointed_only",snapshot_hash,source_confidence;
    bool route_seed_evidence_revalidated=false,mars_stay_continuously_verified=false;
    RoutePass coarse,strict;
    double maximum_checkpoint_position_disagreement_m=0,
           maximum_checkpoint_velocity_disagreement_mps=0,
           venus_event_time_disagreement_s=0,venus_radius_disagreement_m=0;
    double mars_minimum_radius_disagreement_m=0,mars_maximum_radius_disagreement_m=0,
           mars_minimum_time_disagreement_s=0,mars_maximum_time_disagreement_s=0;
    double total_charged_delta_v_mps=0;
};
class RouteEvaluationError:public std::runtime_error {public:using std::runtime_error::runtime_error;};
// Reject every recorded Venus periapsis before selecting the encounter in the screened window.
EncounterEvent select_safe_venus_encounter(const std::vector<EncounterEvent>& events,
    const std::string& venus_id,double expected_ut_s,double window_halfwidth_s,
    double unsafe_boundary_m,double maximum_encounter_radius_m);
// Runtime entrypoint: exact bytes and hash are parsed before any numerical work.
RouteEvaluationResult evaluate_fixed_route_runtime(const std::string& json_bytes,
    const std::string& expected_sha256,const RouteEvaluationRequest& request);
// Manufactured synthetic fixtures only. Never pass imported or claimed-runtime states here.
RouteEvaluationResult evaluate_fixed_route_synthetic_fixture(const Snapshot& snapshot,
    const RouteEvaluationRequest& request);
}
