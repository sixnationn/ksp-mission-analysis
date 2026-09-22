#include "StudyReport.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace ksp;
using json=nlohmann::json;
namespace {
int checks=0;
void check(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
template<class F> void rejects(F call,const char* fragment){try{call();}catch(const StudyReportError& e){
    if(std::string(e.what()).find(fragment)==std::string::npos)
        throw std::runtime_error(std::string("expected ")+fragment+", got "+e.what());
    ++checks;return;}
    throw std::runtime_error(fragment);}
json report(){
    const json leg1={{"departure_ut_s",0.0},{"arrival_ut_s",1000.0},{"departure_body_id","home"},
        {"arrival_body_id","mars"},{"branch","short"},{"direction","positive"}};
    const json leg2={{"departure_ut_s",5185000.0},{"arrival_ut_s",5186000.0},{"departure_body_id","mars"},
        {"arrival_body_id","venus"},{"branch","short"},{"direction","positive"}};
    const json leg3={{"departure_ut_s",5186000.0},{"arrival_ut_s",5187000.0},{"departure_body_id","venus"},
        {"arrival_body_id","home"},{"branch","short"},{"direction","positive"}};
    const json route={{"route_id","route-search-fixture:0:1:2"},{"result_label","patched_conic_screened_route"},
        {"snapshot_hash","route-search-fixture"},{"source_confidence","synthetic_fixture"},
        {"legs",json::array({leg1,leg2,leg3})},{"fixed_stay_s",5184000.0},
        {"home_injection_mps",10.0},{"mars_capture_mps",20.0},{"mars_departure_mps",30.0},
        {"home_return_capture_mps",40.0},{"total_optimistic_delta_v_mps",100.0},
        {"flyby_periapsis_margin_m",1000.0}};
    return {{"schema_version",1},
        {"source",{{"snapshot_hash","route-search-fixture"},{"confidence","synthetic_fixture"},
            {"frame_origin","barycenter"},{"frame_axes","X,Y,Z"},{"frame_handedness","right"},
            {"frame_inertial",true},{"state_epoch_ut_s",0.0}}},
        {"ephemeris",{{"force_model","newtonian_point_mass"},{"result_label","independent_newtonian_nbody"},
            {"integrator","fixture"},{"integrator_version","1"},{"start_ut_s",0.0},{"end_ut_s",6000000.0},
            {"step_s",6000000.0},{"max_position_fit_error_m",1.0},{"max_velocity_fit_error_mps",0.01},
            {"measured_max_position_error_m",0.0},{"measured_max_velocity_error_mps",0.0}}},
        {"mission",{{"central_body_id","sun"},{"home_body_id","home"},{"mars_body_id","mars"},{"venus_body_id","venus"},
            {"launch_start_ut_s",0.0},{"launch_end_ut_s",0.0},{"launch_step_s",1.0},
            {"flight_grids",json::array({{{"min_s",1000.0},{"max_s",1000.0},{"step_s",1.0}},
                {{"min_s",1000.0},{"max_s",1000.0},{"step_s",1.0}},
                {{"min_s",1000.0},{"max_s",1000.0},{"step_s",1.0}}})},
            {"fixed_stay_s",5184000.0},{"stay_type","parking_orbit"},{"return_condition","parking_capture"},
            {"max_cells",100},{"max_routes",5}}},
        {"calendar",{{"display_only",true},{"use_leap_years",false},{"day_duration_s",86400.0},
            {"display_origin_ut_s",0.0},{"month_lengths",{31,28,31,30,31,30,31,31,30,31,30,31}}}},
        {"result",{{"status","complete"},{"validation_status","patched_conic_screen_only"},
            {"sampled_cells",3},{"total_upper_bound_cells",3},{"ranked_routes",json::array({route})}}}};
}
void cases(){
    auto original=report();validate_study_report(original);++checks;
    const auto path=std::filesystem::temp_directory_path()/"ksp-study-report-tests.json";
    save_study_report(original,path);check(load_study_report(path)==original,"exact JSON roundtrip");
    auto wrong=original;wrong["schema_version"]=2;rejects([&]{validate_study_report(wrong);},"version");
    wrong=original;wrong["source"]["snapshot_hash"]="";rejects([&]{validate_study_report(wrong);},"snapshot_hash");
    wrong=original;wrong["source"]["confidence"]="runtime_observed_uncompared";
    rejects([&]{validate_study_report(wrong);},"snapshot_hash");
    auto runtime=original;const std::string runtime_hash(64,'a');
    runtime["source"]["snapshot_hash"]=runtime_hash;
    runtime["source"]["confidence"]="runtime_observed_uncompared";
    runtime["source"]["exporter_id"]="KspMission.RuntimeExporter";
    runtime["source"]["exporter_version"]="1";
    runtime["source"]["game_version"]="1.12.5";
    runtime["source"]["save_id"]="synthetic-report-test";
    runtime["source"]["capture_ut_s"]=0.0;
    runtime["result"]["ranked_routes"][0]["snapshot_hash"]=runtime_hash;
    runtime["result"]["ranked_routes"][0]["source_confidence"]="runtime_observed_uncompared";
    runtime["result"]["ranked_routes"][0]["route_id"]=runtime_hash+":0:1:2";
    validate_study_report(runtime);++checks;
    runtime["source"].erase("game_version");
    rejects([&]{validate_study_report(runtime);},"game_version");
    wrong=original;wrong["source"]["frame_handedness"]="left";rejects([&]{validate_study_report(wrong);},"frame");
    wrong=original;wrong["ephemeris"]["step_s"]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{validate_study_report(wrong);},"nonfinite");
    wrong=original;wrong["mission"]["fixed_stay_s"]=5184001;rejects([&]{validate_study_report(wrong);},"stay");
    wrong=original;wrong["result"]["ranked_routes"][0]["total_optimistic_delta_v_mps"]=101;
    rejects([&]{validate_study_report(wrong);},"burn total");
    wrong=original;wrong["result"]["ranked_routes"][0]["legs"][1]["departure_ut_s"]=5185001;
    wrong["result"]["ranked_routes"][0]["legs"][1]["arrival_ut_s"]=5186001;
    wrong["result"]["ranked_routes"][0]["legs"][2]["departure_ut_s"]=5186001;
    wrong["result"]["ranked_routes"][0]["legs"][2]["arrival_ut_s"]=5187001;
    rejects([&]{validate_study_report(wrong);},"stay date");
    wrong=original;wrong["result"]["ranked_routes"][0]["legs"][1]["departure_body_id"]="home";
    rejects([&]{validate_study_report(wrong);},"leg roles");
    wrong=original;wrong["result"]["ranked_routes"][0]["flyby_periapsis_margin_m"]=-1;
    rejects([&]{validate_study_report(wrong);},"flyby");
    wrong=original;wrong["result"]["ranked_routes"].push_back(wrong["result"]["ranked_routes"][0]);
    rejects([&]{validate_study_report(wrong);},"duplicate route_id");
    wrong=original;wrong["result"]["ranked_routes"][0]["source_confidence"]="runtime_observed_uncompared";
    rejects([&]{validate_study_report(wrong);},"source confidence");
    wrong=original;wrong["result"]["validation_status"]="principia_equivalent";
    rejects([&]{validate_study_report(wrong);},"validation status");
    rejects([&]{save_study_report(wrong,path);},"validation status");
    check(load_study_report(path)==original,"rejected replacement preserves previous report");
    {std::ofstream truncated(path,std::ios::trunc);truncated<<"{";}
    rejects([&]{load_study_report(path);},"JSON");
    save_study_report(original,path);check(load_study_report(path)==original,"atomic replacement after bad file");
    std::filesystem::remove(path);
}
}
int main(){try{cases();std::cout<<"PASS "<<checks<<" report checks\n";}
catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
