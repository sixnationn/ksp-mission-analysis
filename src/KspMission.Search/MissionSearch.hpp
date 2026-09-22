#pragma once
#include "MissionRoute.hpp"
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace ksp {
struct FlightGrid {
    double min_s=0,max_s=0,step_s=0;
    TransferBranch branch=TransferBranch::short_path;
    AngularMomentumDirection direction=AngularMomentumDirection::positive;
};
struct MissionSearchRequest {
    RouteRequest route;
    double launch_step_s=0;
    std::array<FlightGrid,3> legs;
    std::optional<double> central_atmosphere_altitude_m;
    double central_safety_margin_m=0;
    std::size_t max_cells=0,max_routes=0;
};
struct MissionSearchProgress {
    std::size_t sampled_cells=0,total_upper_bound_cells=0,rejected_cells=0,
                considered_combinations=0,rejected_route_combinations=0,retained_routes=0;
};
struct MissionSearchResult {
    std::vector<ScreenedRoute> routes;
    std::string snapshot_hash,source_confidence,diagnostic;
    std::size_t sampled_cells=0,total_upper_bound_cells=0,rejected_cells=0,
                considered_combinations=0,rejected_route_combinations=0,peak_retained_routes=0;
    bool cancelled=false;
};
using MissionSearchCancellation=std::function<bool()>;
using MissionSearchProgressCallback=std::function<void(const MissionSearchProgress&)>;
MissionSearchResult search_mission(const Snapshot& snapshot,const Ephemeris& ephemeris,
    const MissionSearchRequest& request,
    MissionSearchCancellation cancelled={},MissionSearchProgressCallback progress={});
}
