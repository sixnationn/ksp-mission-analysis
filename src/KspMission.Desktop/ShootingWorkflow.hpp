#pragma once
#include <nlohmann/json.hpp>
#include <cstddef>
#include <string>
namespace ksp {
struct ShootingSource {
    std::string path,hash,confidence,frame_origin,frame_axes;
    double state_epoch_ut_s=0;
};
struct ShootingLimits {
    double finite_difference_impulse_mps=0,max_impulse_mps=0;
    std::size_t max_iterations=0,max_probe_evaluations=0;
};
struct ShootingPresentation {
    std::string text;
    bool saveable=false,accepted=false;
};
nlohmann::json build_shooting_request(const ShootingSource& source,const std::string& trial_bytes,
    const ShootingLimits& limits,const std::string& request_id);
ShootingPresentation present_shooting_completion(const nlohmann::json& terminal);
}
