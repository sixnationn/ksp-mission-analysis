#include "RuntimeReader.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ksp;
namespace {
int checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F action,const char* expected){
    try{action();}catch(const RuntimeReaderError& error){check(std::string(error.what()).find(expected)!=std::string::npos,"wrong diagnostic");return;}
    throw std::runtime_error(expected);
}
RuntimeLoad read(const nlohmann::json& document){const auto bytes=document.dump();return read_runtime_snapshot(bytes,sha256_hex(bytes));}
nlohmann::json fixture(){return {
    {"schema_version",1},{"confidence","runtime_observed_uncompared"},
    {"capture",{{"exporter_id","KspMission.RuntimeExporter"},{"exporter_version","1"},{"game_version","1.12.5"},
        {"save_id","synthetic"},{"capture_ut_s",0.0},{"principia_loaded",true},
        {"state_source","principia_celestial_from_parent"},{"mods",nlohmann::json::array({
            {{"id","Principia"},{"version","test"}},{{"id","JNSQ"},{"version","test"}},{{"id","JNSQ-Reborn"},{"version","test"}}})}}},
    {"frame",{{"origin","system_barycenter"},{"axes","principia_alicesun_frozen_at_capture"},{"handedness","right"},
        {"inertial",true},{"source_frame","Principia/AliceSun"},{"transform_method","parent_relative_sum_then_com_translation"},
        {"transform_version","1"}}},
    {"bodies",nlohmann::json::array({
        {{"id","Sun"},{"parent_id",nullptr},{"mu_m3_s2",1e20},{"radius_m",1e7},{"atmosphere_boundary_m",nullptr},
            {"state_epoch_ut_s",0.0},{"position_m",{-1e9,0,0}},{"velocity_mps",{-300,0,0}}},
        {{"id","Planet"},{"parent_id","Sun"},{"mu_m3_s2",1e18},{"radius_m",1e6},{"atmosphere_boundary_m",1e5},
            {"state_epoch_ut_s",0.0},{"position_m",{1e11,0,0}},{"velocity_mps",{30000,0,0}}}})},
    {"calendar",{{"day_duration_s",86400},{"display_origin_ut_s",0},{"use_leap_years",false},
        {"month_lengths",{31,28,31,30,31,30,31,31,30,31,30,31}}}}
};}
void run(){
    check(sha256_hex("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA-256 empty vector");
    check(sha256_hex("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA-256 abc vector");
    auto document=fixture();
    auto loaded=read(document);
    const auto hash=sha256_hex(document.dump());
    check(loaded.snapshot.analysis_ready&&loaded.snapshot.bodies.size()==2,"valid bridge");
    check(loaded.snapshot.snapshot_hash==hash&&loaded.snapshot.confidence=="runtime_observed_uncompared","identity and confidence");
    check(loaded.atmosphere_boundaries.size()==2&&loaded.atmosphere_boundaries[0].altitude_m==0&&
          loaded.atmosphere_boundaries[1].altitude_m==1e5,"explicit atmosphere none and height");
    check(loaded.snapshot.bodies[1].state->position_m.x==1e11,"Cartesian state");
    check(format_no_leap_ut(loaded,0)=="Y0 D0 00:00:00","calendar origin");
    check(format_no_leap_ut(loaded,365*86400.0)=="Y1 D0 00:00:00","calendar year boundary has no leap day");
    check(format_no_leap_ut(loaded,-1)=="Y-1 D364 23:59:59","calendar negative UT");
    auto short_day=loaded;short_day.display_day_duration_s=21600;
    check(format_no_leap_ut(short_day,5400)=="Y0 D0 06:00:00","calendar honors display day duration");
    rejects([&]{format_no_leap_ut(loaded,std::numeric_limits<double>::quiet_NaN());},"finite");
    short_day.display_day_duration_s=0;
    rejects([&]{format_no_leap_ut(short_day,0);},"day duration");
    ksp::Settings request;request.start_ut_s=0;request.end_ut_s=1000;request.step_s=100;
    request.max_position_fit_error_m=100;request.max_velocity_fit_error_mps=0.01;
    auto ephemeris=integrate(loaded.snapshot,request);
    check(ephemeris.metadata.snapshot_hash==hash&&ephemeris.query("Planet",500).position_m.x>0,"M2 consumes runtime-shaped seed");
    document["confidence"]="raw_config_provisional";
    rejects([&]{read(document);},"confidence");document=fixture();
    document["confidence"]="runtime_verified";
    rejects([&]{read(document);},"comparison");document=fixture();
    document["frame"]["handedness"]="left";
    rejects([&]{read(document);},"frame");document=fixture();
    document["frame"]["source_frame"]="World";
    rejects([&]{read(document);},"frame");document=fixture();
    document["bodies"][1]["state_epoch_ut_s"]=1;
    rejects([&]{read(document);},"epoch");document=fixture();
    document["bodies"][1]["id"]="Sun";
    rejects([&]{read(document);},"duplicate");document=fixture();
    document["bodies"][1].erase("atmosphere_boundary_m");
    rejects([&]{read(document);},"atmosphere");document=fixture();
    document["bodies"][1]["position_m"]={-1e9,0,0};
    rejects([&]{read(document);},"overlap");document=fixture();
    document["capture"]["mods"].push_back({{"id","Principia"},{"version","duplicate"}});
    rejects([&]{read(document);},"duplicate");document=fixture();
    document["bodies"][1]["parent_id"]="";
    rejects([&]{read(document);},"parent_id");document=fixture();
    rejects([&]{read_runtime_snapshot(document.dump(),"bad");},"hash");
    rejects([&]{read_runtime_snapshot(document.dump(),std::string(64,'0'));},"source hash mismatch");
}
}
int main(){try{run();std::cout<<"PASS "<<checks<<" runtime bridge checks\n";return 0;}
catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
