#pragma once
#include "Ephemeris.hpp"
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ksp {
struct Impulse { double ut_s=0; Vec3 delta_v_mps; };
struct AtmosphereBoundary { std::string body_id; double altitude_m=0; };
struct SpacecraftSettings {
    double start_ut_s=0,end_ut_s=0;
    double abs_position_tolerance_m=0,abs_velocity_tolerance_mps=0,relative_tolerance=0;
    double min_step_s=0,max_step_s=0,safety_margin_m=0;
    std::vector<Impulse> burns;
    std::vector<AtmosphereBoundary> atmosphere_boundaries;
    std::size_t max_accepted_steps=1000000;
};
struct BurnRecord { double ut_s=0; State before,after; Vec3 delta_v_mps; double delta_v_magnitude_mps=0; };
struct EncounterEvent { double ut_s=0; std::string body_id,frame_origin,frame_axes; double distance_m=0,clearance_m=0; State spacecraft_state; };
struct SpacecraftResult {
    bool success=false; State final_state; double final_ut_s=0;
    std::vector<BurnRecord> burns; std::vector<EncounterEvent> closest_approaches;
    std::optional<EncounterEvent> unsafe;
    std::size_t accepted_steps=0,rejected_steps=0;
    double smallest_accepted_step_s=0,largest_accepted_step_s=0;
    double requested_abs_position_tolerance_m=0,requested_abs_velocity_tolerance_mps=0,requested_relative_tolerance=0;
    double max_accepted_position_local_error_m=0,max_accepted_velocity_local_error_mps=0,max_accepted_normalized_error=0;
    std::string ephemeris_snapshot_hash,ephemeris_integrator,ephemeris_force_model;
    Metadata ephemeris_metadata;
};
class SpacecraftError : public std::runtime_error {
public:
    explicit SpacecraftError(const std::string& message):std::runtime_error(message){}
    SpacecraftError(const std::string& message,double last_ut,State last_state)
        :std::runtime_error(message),last_valid_ut_s(last_ut),last_valid_state(last_state){}
    std::optional<double> last_valid_ut_s; std::optional<State> last_valid_state;
};
SpacecraftResult propagate(const Snapshot& snapshot,const Ephemeris& ephemeris,State initial,const SpacecraftSettings& settings);
}
