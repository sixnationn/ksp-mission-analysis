#include "Refine.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ksp {
namespace {
bool finite(double x){return std::isfinite(x);}
bool finite(Vec3 x){return finite(x.x)&&finite(x.y)&&finite(x.z);}
double component(Vec3 x,std::size_t i){return i==0?x.x:i==1?x.y:x.z;}
Vec3 basis(std::size_t i){return i==0?Vec3{1,0,0}:i==1?Vec3{0,1,0}:Vec3{0,0,1};}
Vec3 solve_linear(std::array<std::array<double,4>,3> a){
    double largest=0;
    for(const auto& row:a)for(std::size_t j=0;j<3;++j)largest=std::max(largest,std::abs(row[j]));
    if(!finite(largest)||largest==0)throw SpacecraftError("refinement Jacobian singular");
    for(std::size_t col=0;col<3;++col){
        std::size_t pivot=col;
        for(std::size_t row=col+1;row<3;++row)if(std::abs(a[row][col])>std::abs(a[pivot][col]))pivot=row;
        if(std::abs(a[pivot][col])<largest*1e-12)throw SpacecraftError("refinement Jacobian singular");
        std::swap(a[col],a[pivot]);
        const double divisor=a[col][col];
        for(std::size_t j=col;j<4;++j)a[col][j]/=divisor;
        for(std::size_t row=0;row<3;++row)if(row!=col){
            const double factor=a[row][col];
            for(std::size_t j=col;j<4;++j)a[row][j]-=factor*a[col][j];
        }
    }
    const Vec3 result{a[0][3],a[1][3],a[2][3]};
    if(!finite(result))throw SpacecraftError("refinement correction nonfinite");
    return result;
}
struct Evaluation {SpacecraftResult trajectory;double miss=std::numeric_limits<double>::infinity();};
}
RefineResult refine_terminal_position(const Snapshot& snapshot,const Ephemeris& ephemeris,const RefineRequest& q){
    if(!finite(q.initial_state.position_m)||!finite(q.initial_state.velocity_mps))throw SpacecraftError("initial_state invalid");
    if(!finite(q.target_position_m))throw SpacecraftError("target_position_m invalid");
    if(!finite(q.seed_impulse_mps))throw SpacecraftError("seed_impulse_mps invalid");
    if(!finite(q.target_position_tolerance_m)||q.target_position_tolerance_m<=0)throw SpacecraftError("target_position_tolerance_m invalid");
    if(!finite(q.finite_difference_step_mps)||q.finite_difference_step_mps<=0)throw SpacecraftError("finite_difference_step_mps invalid");
    if(!finite(q.maximum_impulse_mps)||q.maximum_impulse_mps<=0||norm(q.seed_impulse_mps)>q.maximum_impulse_mps)
        throw SpacecraftError("maximum_impulse_mps invalid or seed exceeds limit");
    if(!finite(q.strict_disagreement_limit_m)||q.strict_disagreement_limit_m<=0)throw SpacecraftError("strict_disagreement_limit_m invalid");
    if(q.maximum_iterations==0||q.maximum_iterations>100)throw SpacecraftError("maximum_iterations invalid");
    if(!q.propagation.burns.empty())throw SpacecraftError("preexisting burns are unsupported");
    if(q.propagation.end_ut_s<=q.propagation.start_ut_s)throw SpacecraftError("refinement time span invalid");
    const double duration=q.propagation.end_ut_s-q.propagation.start_ut_s;
    const auto& m=ephemeris.metadata;
    if(!finite(m.max_position_fit_error_m)||!finite(m.max_velocity_fit_error_mps)||
       m.max_position_fit_error_m<0||m.max_velocity_fit_error_mps<0||
       m.max_position_fit_error_m+m.max_velocity_fit_error_mps*duration>
           std::min(q.target_position_tolerance_m,q.strict_disagreement_limit_m)*0.1)
        throw SpacecraftError("ephemeris accuracy budget exceeds terminal tolerance");
    if(!finite(m.step_s)||m.step_s<=0||m.start_ut_s>q.propagation.start_ut_s||m.end_ut_s<q.propagation.end_ut_s)
        throw SpacecraftError("ephemeris coverage or step invalid for refinement");
    Settings tighter_planets;
    tighter_planets.start_ut_s=m.start_ut_s;tighter_planets.end_ut_s=m.end_ut_s;
    tighter_planets.step_s=m.step_s*0.5;
    tighter_planets.max_position_fit_error_m=m.max_position_fit_error_m;
    tighter_planets.max_velocity_fit_error_mps=m.max_velocity_fit_error_mps;
    tighter_planets.result_label=m.result_label;
    Ephemeris strict_ephemeris;
    try{strict_ephemeris=integrate(snapshot,tighter_planets);}
    catch(const EphemerisError& error){throw SpacecraftError(std::string("tighter planetary ephemeris failed: ")+error.what());}
    double planet_disagreement=0;
    for(const auto& body:snapshot.bodies)for(int sample=0;sample<=8;++sample){
        const double ut=q.propagation.start_ut_s+duration*(static_cast<double>(sample)/8.0);
        const State coarse=ephemeris.query(body.id,ut),fine=strict_ephemeris.query(body.id,ut);
        planet_disagreement=std::max(planet_disagreement,
            norm(coarse.position_m-fine.position_m)+norm(coarse.velocity_mps-fine.velocity_mps)*duration);
    }
    if(!finite(planet_disagreement)||planet_disagreement>std::min(q.target_position_tolerance_m,q.strict_disagreement_limit_m)*0.1)
        throw SpacecraftError("ephemeris convergence exceeds terminal tolerance");
    std::size_t evaluations=0;
    auto evaluate=[&](Vec3 impulse,const SpacecraftSettings& settings,const Ephemeris& planets){
        auto run=settings;run.burns={{run.start_ut_s,impulse}};
        ++evaluations;
        auto trajectory=propagate(snapshot,planets,q.initial_state,run);
        if(!trajectory.success||trajectory.unsafe)throw SpacecraftError("unsafe refinement trajectory");
        const double miss=norm(trajectory.final_state.position_m-q.target_position_m);
        if(!finite(miss))throw SpacecraftError("refinement miss nonfinite");
        return Evaluation{std::move(trajectory),miss};
    };
    Vec3 impulse=q.seed_impulse_mps;
    Evaluation current;
    try{current=evaluate(impulse,q.propagation,ephemeris);}
    catch(const SpacecraftError& error){
        if(std::string(error.what())=="unsafe refinement trajectory")throw SpacecraftError("unsafe seed trajectory");
        throw;
    }
    std::size_t iterations=0;
    while(current.miss>q.target_position_tolerance_m&&iterations<q.maximum_iterations){
        std::array<std::array<double,4>,3> system{};
        for(std::size_t axis=0;axis<3;++axis){
            const Vec3 offset=basis(axis)*q.finite_difference_step_mps;
            Evaluation plus,minus;
            try{
                plus=evaluate(impulse+offset,q.propagation,ephemeris);
                minus=evaluate(impulse-offset,q.propagation,ephemeris);
            }catch(const SpacecraftError&){throw SpacecraftError("finite difference trial unsafe or unresolved");}
            const Vec3 column=(plus.trajectory.final_state.position_m-minus.trajectory.final_state.position_m)*(0.5/q.finite_difference_step_mps);
            for(std::size_t row=0;row<3;++row)system[row][axis]=component(column,row);
        }
        const Vec3 residual=current.trajectory.final_state.position_m-q.target_position_m;
        for(std::size_t row=0;row<3;++row)system[row][3]=-component(residual,row);
        const Vec3 correction=solve_linear(system);
        bool improved=false;
        for(int line=0;line<12;++line){
            const double scale=std::ldexp(1.0,-line);
            const Vec3 trial=impulse+correction*scale;
            if(!finite(trial)||norm(trial)>q.maximum_impulse_mps)continue;
            try{
                auto candidate=evaluate(trial,q.propagation,ephemeris);
                if(candidate.miss<current.miss){impulse=trial;current=std::move(candidate);improved=true;break;}
            }catch(const SpacecraftError&){}
        }
        if(!improved)throw SpacecraftError("refinement line search made no progress");
        ++iterations;
    }
    if(current.miss>q.target_position_tolerance_m)throw SpacecraftError("refinement did not converge");
    auto strict=q.propagation;
    strict.abs_position_tolerance_m*=0.1;
    strict.abs_velocity_tolerance_mps*=0.1;
    strict.relative_tolerance*=0.1;
    strict.max_step_s*=0.5;
    if(strict.max_step_s<strict.min_step_s)throw SpacecraftError("strict repropagation step invalid");
    const auto verified=evaluate(impulse,strict,strict_ephemeris);
    const double disagreement=norm(verified.trajectory.final_state.position_m-current.trajectory.final_state.position_m);
    if(!finite(disagreement)||disagreement>q.strict_disagreement_limit_m||verified.miss>q.target_position_tolerance_m)
        throw SpacecraftError("strict repropagation exceeds target or disagreement limit");
    RefineResult result;
    result.impulse_mps=impulse;result.impulse_magnitude_mps=norm(impulse);
    result.position_residual_m=verified.miss;result.strict_disagreement_m=disagreement;
    result.ephemeris_step_disagreement_m=planet_disagreement;
    result.iterations=iterations;result.evaluations=evaluations;
    result.trajectory=std::move(current.trajectory);result.strict_trajectory=verified.trajectory;
    return result;
}
}
