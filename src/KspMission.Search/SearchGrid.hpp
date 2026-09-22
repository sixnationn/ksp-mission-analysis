#pragma once
#include "Search.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ksp {
struct GridRequest {
    std::string central_body_id,departure_body_id,arrival_body_id;
    double launch_start_ut_s=0,launch_end_ut_s=0,launch_step_s=0;
    double flight_min_s=0,flight_max_s=0,flight_step_s=0;
    Vec3 reference_normal;
    TransferBranch branch=TransferBranch::short_path;
    AngularMomentumDirection direction=AngularMomentumDirection::positive;
    double max_position_residual_m=0,max_velocity_residual_mps=0;
    std::optional<double> central_atmosphere_altitude_m;
    double central_safety_margin_m=0;
    std::size_t max_candidates=0,max_grid_cells=100000;
    std::uint64_t stable_seed=0;
};
struct DatedLegCandidate {
    double departure_ut_s=0,arrival_ut_s=0,flight_time_s=0;
    std::string central_body_id,departure_body_id,arrival_body_id,snapshot_hash,source_confidence,frame_origin,frame_axes;
    Metadata ephemeris_metadata;
    TransferBranch branch=TransferBranch::short_path;
    AngularMomentumDirection direction=AngularMomentumDirection::positive;
    LambertResult lambert;
    Vec3 departure_barycentric_velocity_mps,arrival_barycentric_velocity_mps;
    // Velocity mismatch at each planet, not a parking-orbit injection/capture burn.
    double departure_vinf_mps=0,arrival_vinf_mps=0,screening_score_mps=0;
    std::uint64_t tie_key=0;
};
struct GridResult {
    std::vector<DatedLegCandidate> candidates;
    std::string diagnostic;
    std::string snapshot_hash;
    Metadata ephemeris_metadata;
    std::uint64_t stable_seed=0;
    std::size_t sampled_cells=0,rejected_cells=0;
};
GridResult screen_single_leg(const Snapshot& snapshot,const Ephemeris& ephemeris,const GridRequest& request);
}
