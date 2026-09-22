#pragma once
#include "Ephemeris.hpp"
#include <cstddef>
#include <optional>
#include <stdexcept>

namespace ksp {
enum class TransferBranch { short_path, long_path };
enum class AngularMomentumDirection { positive, negative };
struct LambertRequest {
    // Both positions are relative to the SAME fixed central mass, expressed in
    // one right-handed inertial frame. Callers subtract the central body's
    // barycentric position at each endpoint before invoking this solver.
    Vec3 departure_relative_position_m,arrival_relative_position_m,reference_normal;
    double mu_m3_s2=0,time_of_flight_s=0;
    TransferBranch branch=TransferBranch::short_path;
    AngularMomentumDirection direction=AngularMomentumDirection::positive;
    double max_position_residual_m=0,max_velocity_residual_mps=0;
    std::size_t max_iterations=128;
};
struct LambertResult {
    // Relative velocities. Add the central body's barycentric velocity at
    // each corresponding endpoint to obtain inertial barycentric velocities.
    Vec3 departure_relative_velocity_mps,arrival_relative_velocity_mps;
    TransferBranch branch; AngularMomentumDirection direction;
    double position_residual_m=0,velocity_residual_mps=0,time_residual_s=0;
    std::size_t iterations=0;
};
struct FlybyRequest {
    Vec3 incoming_vinf_mps,outgoing_vinf_mps;
    double mu_planet_m3_s2=0,radius_m=0;
    std::optional<double> atmosphere_altitude_m;
    double safety_margin_m=0,speed_tolerance_mps=0,maximum_periapsis_m=0;
};
struct FlybyResult {
    double required_turn_rad=0,maximum_turn_rad=0,minimum_periapsis_m=0;
    double speed_mismatch_mps=0,speed_margin_mps=0,turn_margin_rad=0,periapsis_margin_m=0;
};
class SearchError:public std::runtime_error{public:using std::runtime_error::runtime_error;};
LambertResult solve_lambert(const LambertRequest& request);
FlybyResult screen_unpowered_flyby(const FlybyRequest& request);
}
