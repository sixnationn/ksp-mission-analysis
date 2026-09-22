#include "StudyReport.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ksp {
namespace {
using json=nlohmann::json;
constexpr std::size_t max_report_bytes=16*1024*1024;
const json& object(const json& parent,const char* key){
    if(!parent.is_object()||!parent.contains(key)||!parent.at(key).is_object())
        throw StudyReportError(std::string(key)+" object missing");
    return parent.at(key);
}
const json& array(const json& parent,const char* key){
    if(!parent.is_object()||!parent.contains(key)||!parent.at(key).is_array())
        throw StudyReportError(std::string(key)+" array missing");
    return parent.at(key);
}
std::string string(const json& parent,const char* key){
    if(!parent.is_object()||!parent.contains(key)||!parent.at(key).is_string())
        throw StudyReportError(std::string(key)+" text missing");
    auto value=parent.at(key).get<std::string>();
    if(value.empty())throw StudyReportError(std::string(key)+" empty");
    return value;
}
double number(const json& parent,const char* key){
    if(!parent.is_object()||!parent.contains(key)||!parent.at(key).is_number())
        throw StudyReportError(std::string(key)+" number missing");
    const double value=parent.at(key).get<double>();
    if(!std::isfinite(value))throw StudyReportError(std::string(key)+" nonfinite");
    return value;
}
std::size_t count(const json& parent,const char* key){
    if(!parent.is_object()||!parent.contains(key)||!parent.at(key).is_number_integer())
        throw StudyReportError(std::string(key)+" count invalid");
    const auto value=parent.at(key).get<std::int64_t>();
    if(value<0)throw StudyReportError(std::string(key)+" count invalid");
    return static_cast<std::size_t>(value);
}
void finite_tree(const json& node){
    if(node.is_number_float()&&!std::isfinite(node.get<double>()))throw StudyReportError("nonfinite report value");
    if(node.is_array())for(const auto& item:node)finite_tree(item);
    if(node.is_object())for(auto it=node.begin();it!=node.end();++it)finite_tree(it.value());
}
bool lower_hash(const std::string& hash){
    return hash.size()==64&&std::all_of(hash.begin(),hash.end(),[](unsigned char c){
        return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
std::size_t grid_count(double low,double high,double step,const char* label){
    if(low<0||high<low||step<=0)throw StudyReportError(std::string(label)+" grid invalid");
    const double intervals=(high-low)/step;
    if(!std::isfinite(intervals)||intervals>100000||std::abs(intervals-std::round(intervals))>1e-9)
        throw StudyReportError(std::string(label)+" grid invalid");
    return static_cast<std::size_t>(std::llround(intervals))+1;
}
std::size_t bounded_multiply(std::size_t a,std::size_t b,std::size_t limit){
    if(b!=0&&a>limit/b)throw StudyReportError("result work counts invalid");
    return a*b;
}
void replace_file(const std::filesystem::path& temporary,const std::filesystem::path& target){
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw StudyReportError("atomic report replacement failed");
#else
    if(std::rename(temporary.c_str(),target.c_str())!=0)throw StudyReportError("atomic report replacement failed");
#endif
}
void write_synced(const std::filesystem::path& path,const std::string& bytes){
    FILE* file=nullptr;
#ifdef _WIN32
    if(_wfopen_s(&file,path.c_str(),L"wb")!=0||!file)throw StudyReportError("temporary report open failed");
#else
    file=std::fopen(path.c_str(),"wb");
    if(!file)throw StudyReportError("temporary report open failed");
#endif
    bool good=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size()&&std::fflush(file)==0;
    if(good){
#ifdef _WIN32
        good=_commit(_fileno(file))==0;
#else
        good=fsync(fileno(file))==0;
#endif
    }
    if(std::fclose(file)!=0)good=false;
    if(!good)throw StudyReportError("temporary report write failed");
}
}
void validate_study_report(const json& document){
    finite_tree(document);
    if(!document.is_object()||!document.contains("schema_version")||
       !document.at("schema_version").is_number_integer()||document.at("schema_version")!=1)
        throw StudyReportError("study report version unsupported");
    const auto& source=object(document,"source");
    const auto hash=string(source,"snapshot_hash"),confidence=string(source,"confidence");
    if(confidence!="synthetic_fixture"&&confidence!="runtime_observed_uncompared")
        throw StudyReportError("source confidence unsupported");
    if(confidence=="runtime_observed_uncompared"){
        if(!lower_hash(hash))throw StudyReportError("snapshot_hash invalid");
        string(source,"exporter_id");string(source,"exporter_version");
        string(source,"game_version");string(source,"save_id");
        number(source,"capture_ut_s");
    }
    string(source,"frame_origin");string(source,"frame_axes");
    if(string(source,"frame_handedness")!="right"||!source.contains("frame_inertial")||
       !source.at("frame_inertial").is_boolean()||!source.at("frame_inertial").get<bool>())
        throw StudyReportError("frame must be right-handed inertial");
    const double epoch=number(source,"state_epoch_ut_s");
    const auto& ephemeris=object(document,"ephemeris");
    if(string(ephemeris,"force_model")!="newtonian_point_mass"||
       string(ephemeris,"result_label")!="independent_newtonian_nbody")
        throw StudyReportError("ephemeris model unsupported");
    string(ephemeris,"integrator");string(ephemeris,"integrator_version");
    const double ep_start=number(ephemeris,"start_ut_s"),ep_end=number(ephemeris,"end_ut_s");
    const double position_budget=number(ephemeris,"max_position_fit_error_m");
    const double velocity_budget=number(ephemeris,"max_velocity_fit_error_mps");
    const double measured_position=number(ephemeris,"measured_max_position_error_m");
    const double measured_velocity=number(ephemeris,"measured_max_velocity_error_mps");
    if(ep_start>epoch||ep_end<=ep_start||number(ephemeris,"step_s")<=0||
       position_budget<=0||velocity_budget<=0||measured_position<0||measured_velocity<0||
       measured_position>position_budget||measured_velocity>velocity_budget)
        throw StudyReportError("ephemeris coverage or fit invalid");
    const auto& mission=object(document,"mission");
    const auto central=string(mission,"central_body_id"),home=string(mission,"home_body_id"),
               mars=string(mission,"mars_body_id"),venus=string(mission,"venus_body_id");
    if(std::set<std::string>{central,home,mars,venus}.size()!=4)throw StudyReportError("mission roles duplicated");
    const double launch_start=number(mission,"launch_start_ut_s"),launch_end=number(mission,"launch_end_ut_s");
    const auto launch_cells=grid_count(launch_start,launch_end,number(mission,"launch_step_s"),"launch");
    if(launch_start<ep_start||launch_end>ep_end)throw StudyReportError("launch outside ephemeris coverage");
    if(number(mission,"fixed_stay_s")!=5184000.0||string(mission,"stay_type")!="parking_orbit")
        throw StudyReportError("fixed stay invalid");
    if(string(mission,"return_condition")!="parking_capture")throw StudyReportError("return condition unsupported");
    const auto& grids=array(mission,"flight_grids");
    if(grids.size()!=3)throw StudyReportError("three flight grids required");
    std::size_t leg_cells[3]{};
    for(std::size_t i=0;i<3;++i){
        const auto& grid=grids[i];
        const double low=number(grid,"min_s"),high=number(grid,"max_s"),step=number(grid,"step_s");
        if(low<=0)throw StudyReportError("flight grid duration invalid");
        leg_cells[i]=grid_count(low,high,step,"flight");
    }
    const auto max_cells=count(mission,"max_cells"),max_routes=count(mission,"max_routes");
    if(max_cells==0||max_cells>100000||max_routes==0||max_routes>1000)
        throw StudyReportError("study limits invalid");
    const auto& calendar=object(document,"calendar");
    if(!calendar.contains("display_only")||calendar.at("display_only")!=true||
       !calendar.contains("use_leap_years")||calendar.at("use_leap_years")!=false||
       number(calendar,"day_duration_s")<=0)
        throw StudyReportError("no-leap calendar invalid");
    number(calendar,"display_origin_ut_s");
    const auto& months=array(calendar,"month_lengths");
    if(months.size()!=12)throw StudyReportError("calendar month_lengths invalid");
    for(const auto& month:months)if(!month.is_number_integer()||month.get<int>()<=0)
        throw StudyReportError("calendar month_lengths invalid");
    const auto& result=object(document,"result");
    const auto status=string(result,"status");
    if(status!="complete"&&status!="cancelled")throw StudyReportError("result status invalid");
    if(string(result,"validation_status")!="patched_conic_screen_only")
        throw StudyReportError("validation status unsupported");
    const auto sampled=count(result,"sampled_cells"),upper=count(result,"total_upper_bound_cells");
    const auto first=bounded_multiply(launch_cells,leg_cells[0],max_cells);
    const auto second=bounded_multiply(first,leg_cells[1],max_cells);
    const auto third=bounded_multiply(second,leg_cells[2],max_cells);
    if(first>max_cells||second>max_cells||third>max_cells||
       first>max_cells-second||first+second>max_cells-third||
       sampled>upper||upper!=first+second+third)
        throw StudyReportError("result work counts invalid");
    const auto& routes=array(result,"ranked_routes");
    if(routes.size()>max_routes)throw StudyReportError("route limit exceeded");
    std::set<std::string> ids;
    double prior_total=-1;
    for(const auto& route:routes){
        const auto id=string(route,"route_id");
        if(!ids.insert(id).second)throw StudyReportError("duplicate route_id");
        if(id.rfind(hash+":",0)!=0)throw StudyReportError("route_id source mismatch");
        if(string(route,"result_label")!="patched_conic_screened_route")
            throw StudyReportError("route result_label invalid");
        if(string(route,"snapshot_hash")!=hash)throw StudyReportError("route snapshot_hash mismatch");
        if(string(route,"source_confidence")!=confidence)throw StudyReportError("route source confidence mismatch");
        if(number(route,"fixed_stay_s")!=5184000.0)throw StudyReportError("route fixed stay invalid");
        const auto& legs=array(route,"legs");
        if(legs.size()!=3)throw StudyReportError("route needs three legs");
        double start[3],end[3];
        for(std::size_t i=0;i<3;++i){
            start[i]=number(legs[i],"departure_ut_s");end[i]=number(legs[i],"arrival_ut_s");
            if(end[i]<=start[i]||start[i]<ep_start||end[i]>ep_end)
                throw StudyReportError("leg date or coverage invalid");
            const auto& grid=grids[i];
            if(end[i]-start[i]<number(grid,"min_s")-1e-6||end[i]-start[i]>number(grid,"max_s")+1e-6)
                throw StudyReportError("leg flight grid mismatch");
            const auto from=string(legs[i],"departure_body_id"),to=string(legs[i],"arrival_body_id");
            if(from!=(i==0?home:i==1?mars:venus)||to!=(i==0?mars:i==1?venus:home))
                throw StudyReportError("leg roles mismatch");
            const auto branch=string(legs[i],"branch"),direction=string(legs[i],"direction");
            if((branch!="short"&&branch!="long")||(direction!="positive"&&direction!="negative"))
                throw StudyReportError("leg branch or direction invalid");
        }
        if(start[0]<launch_start||start[0]>launch_end||start[1]!=end[0]+5184000.0)
            throw StudyReportError("route stay date mismatch");
        if(start[2]!=end[1])throw StudyReportError("flyby date mismatch");
        const double a=number(route,"home_injection_mps"),b=number(route,"mars_capture_mps"),
                     c=number(route,"mars_departure_mps"),d=number(route,"home_return_capture_mps"),
                     total=number(route,"total_optimistic_delta_v_mps");
        if(a<0||b<0||c<0||d<0||std::abs((a+b+c+d)-total)>1e-6)
            throw StudyReportError("burn total mismatch");
        if(total<prior_total)throw StudyReportError("route ranking invalid");
        prior_total=total;
        if(number(route,"flyby_periapsis_margin_m")<0)throw StudyReportError("unsafe flyby margin");
    }
}
void save_study_report(const json& document,const std::filesystem::path& path){
    validate_study_report(document);
    if(path.empty())throw StudyReportError("report path missing");
    const auto bytes=document.dump(2)+"\n";
    if(bytes.size()>max_report_bytes)throw StudyReportError("report size limit exceeded");
    const auto nonce=std::chrono::steady_clock::now().time_since_epoch().count();
    auto temporary=path;temporary+=std::filesystem::path(".tmp."+std::to_string(nonce));
    try{write_synced(temporary,bytes);replace_file(temporary,path);}
    catch(...){std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
}
json load_study_report(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)throw StudyReportError("report open failed");
    const auto length=file.tellg();
    if(length<0||length>static_cast<std::streamoff>(max_report_bytes))throw StudyReportError("report size limit exceeded");
    std::string bytes(static_cast<std::size_t>(length),'\0');file.seekg(0);
    if(!file.read(bytes.data(),length))throw StudyReportError("report read failed");
    auto document=json::parse(bytes,nullptr,false);
    if(document.is_discarded())throw StudyReportError("report JSON invalid");
    validate_study_report(document);return document;
}
}
