#pragma once
#include "Spacecraft.hpp"
#include <cstddef>

namespace ksp {
struct RefineRequest {
    State initial_state;
    // A position in the same inertial frame as the snapshot at propagation.end_ut_s.
    // This is position targeting only; arrival velocity and mission constraints are not checked.
    Vec3 target_position_m,seed_impulse_mps;
    SpacecraftSettings propagation;
    double target_position_tolerance_m=0;
    double finite_difference_step_mps=0;
    double maximum_impulse_mps=0;
    double strict_disagreement_limit_m=0;
    std::size_t maximum_iterations=0;
};
struct RefineResult {
    Vec3 impulse_mps;
    double impulse_magnitude_mps=0,position_residual_m=0,strict_disagreement_m=0,ephemeris_step_disagreement_m=0;
    std::size_t iterations=0,evaluations=0;
    std::string result_label="terminal_position_targeted_only";
    SpacecraftResult trajectory,strict_trajectory;
};
RefineResult refine_terminal_position(const Snapshot& snapshot,const Ephemeris& ephemeris,const RefineRequest& request);
}
