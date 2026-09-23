#include "ViewMath.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace ksp;

namespace {
void check(bool okay, const char* message){if(!okay)throw std::runtime_error(message);}
void nearly(double actual,double expected,double tolerance,const char* message){
    check(std::abs(actual-expected)<=tolerance,message);
}
void aspect_ratio(){
    view::Camera camera;camera.yaw_rad=0;camera.tilt_rad=0;
    for(const auto [width,height]:{std::pair{2700,700},std::pair{900,900},std::pair{700,900}}){
        const auto right=view::project({1e7,0,0},1e7,camera,width,height);
        const auto up=view::project({0,1e7,0},1e7,camera,width,height);
        nearly(right.x*width/2.0,up.y*height/2.0,1e-9,
            "equal world radii must occupy equal screen pixels after resize");
    }
}
void overhead_drag(){
    const double initial=0.55;
    check(view::drag_tilt(initial,-100)<initial,"upward drag approaches overhead");
    nearly(view::drag_tilt(initial,-1000),0,1e-12,
        "upward drag stops at overhead instead of passing below the plane");
    check(view::drag_tilt(view::drag_tilt(initial,-1000),20)>0,
        "downward reversal moves away from an overhead clamp immediately");
    check(view::drag_tilt(initial,1000)<1.57,"downward drag cannot flip under the plane");
}
void inclined_fixture(){
    constexpr double mu=3.986004418e14,radius=1.5e7;
    const auto flat=view::circular_orbit(mu,radius,0.8,0,0.5);
    const auto inclined=view::circular_orbit(mu,radius,0.8,0.35,0.5);
    nearly(flat.position_m.z,0,1e-9,"zero inclination stays in reference plane");
    check(std::abs(inclined.position_m.z)>1e6,"inclined orbit visibly leaves reference plane");
    nearly(norm(inclined.position_m),radius,1e-8,"inclination preserves orbit radius");
    nearly(inclined.position_m.x*inclined.velocity_mps.x+
           inclined.position_m.y*inclined.velocity_mps.y+
           inclined.position_m.z*inclined.velocity_mps.z,0,1e-5,
           "circular orbit velocity stays tangential after inclination");
}
}
int main(){
    try{aspect_ratio();overhead_drag();inclined_fixture();std::cout<<"PASS view geometry\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
