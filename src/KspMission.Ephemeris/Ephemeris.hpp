#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ksp {
struct Vec3 { double x=0,y=0,z=0; };
Vec3 operator+(Vec3 a,Vec3 b); Vec3 operator-(Vec3 a,Vec3 b); Vec3 operator*(Vec3 a,double k);
double norm(Vec3 a);
struct State { Vec3 position_m,velocity_mps; };
struct Frame { std::string origin,axes,handedness; bool inertial=false; };
struct Body { std::string id; double mu_m3_s2=0,radius_m=0; std::optional<State> state; std::optional<double> epoch_ut_s; };
struct Snapshot {
    bool analysis_ready=false; std::string confidence,snapshot_hash;
    std::optional<double> state_epoch_ut_s; Frame frame; std::vector<Body> bodies;
};
struct Settings {
    double start_ut_s=0,end_ut_s=0,step_s=0;
    double max_position_fit_error_m=0,max_velocity_fit_error_mps=0;
    std::size_t max_steps=1000000;
    std::string result_label="independent_newtonian_nbody";
};
struct Metadata {
    std::string snapshot_hash,source_confidence,frame_origin,frame_axes,frame_handedness;
    bool frame_inertial=false;
    std::string force_model="newtonian_point_mass",integrator="velocity_verlet_with_certified_safety_substeps",integrator_version="2";
    std::string result_label="independent_newtonian_nbody",interpolation="cubic_hermite_position_velocity";
    double state_epoch_ut_s=0,start_ut_s=0,end_ut_s=0,step_s=0,segment_spacing_s=0;
    double max_position_fit_error_m=0,max_velocity_fit_error_mps=0;
    double measured_max_position_error_m=0,measured_max_velocity_error_mps=0;
    std::size_t safety_split_count=0;
};
class EphemerisError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
class Ephemeris {
public:
    Metadata metadata;
    std::vector<std::string> body_ids;
    std::vector<std::vector<State>> samples;
    State query(const std::string& body_id,double ut_s) const;
};
Ephemeris integrate(const Snapshot& snapshot,const Settings& settings);
bool cache_compatible(const Metadata& metadata,const Snapshot& snapshot,const Settings& settings);
}
