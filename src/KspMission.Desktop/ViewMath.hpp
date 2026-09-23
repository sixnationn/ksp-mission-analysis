#pragma once
#include "Ephemeris.hpp"
#include <algorithm>
#include <cmath>

namespace ksp::view {
struct Camera {
    double yaw_rad=0.18,tilt_rad=0.55,zoom=1,pan_x=0,pan_y=0;
};
struct Projected {double x,y,z;};

// Orthographic scale is measured in the shorter viewport dimension, so a
// world-space circle cannot become an oval when the GTK window is resized.
inline Projected project(Vec3 position_m,double scale_m,const Camera& camera,int width,int height){
    const double x=position_m.x/scale_m;
    const double y=position_m.y/scale_m;
    const double z=position_m.z/scale_m;
    const double cy=std::cos(camera.yaw_rad),sy=std::sin(camera.yaw_rad);
    const double cp=std::cos(camera.tilt_rad),sp=std::sin(camera.tilt_rad);
    const double rotated_x=cy*x-sy*y;
    const double rotated_y=sy*x+cy*y;
    const double screen_y=cp*rotated_y-sp*z;
    const double aspect=static_cast<double>(std::max(1,width))/std::max(1,height);
    // Screen zoom must not push otherwise visible orbit arcs past OpenGL's
    // normalized depth limits. Scene scale already bounds their depth.
    return {rotated_x/camera.zoom/std::max(1.0,aspect)+camera.pan_x,
            screen_y/camera.zoom*std::min(1.0,aspect)+camera.pan_y,
            sp*rotated_y+cp*z};
}

// Upward drag reaches the orbital normal and stops there instead of crossing
// it into an inverted view. Downward drag approaches an edge-on view.
inline double drag_tilt(double initial_rad,double offset_y_px){
    return std::clamp(initial_rad+offset_y_px*0.004,0.0,1.45);
}

inline State circular_orbit(double central_mu_m3_s2,double radius_m,double phase_rad,
                            double inclination_rad,double ascending_node_rad){
    const double c=std::cos(phase_rad),s=std::sin(phase_rad);
    const double ci=std::cos(inclination_rad),si=std::sin(inclination_rad);
    const double cn=std::cos(ascending_node_rad),sn=std::sin(ascending_node_rad);
    const double speed=std::sqrt(central_mu_m3_s2/radius_m);
    return {{radius_m*(cn*c-sn*s*ci),radius_m*(sn*c+cn*s*ci),radius_m*s*si},
            {speed*(-cn*s-sn*c*ci),speed*(-sn*s+cn*c*ci),speed*c*si}};
}
}
