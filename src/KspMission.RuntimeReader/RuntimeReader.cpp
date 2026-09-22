#include "RuntimeReader.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace ksp {
namespace {
using json=nlohmann::json;
const json& field(const json& value,const char* key,const std::string& at){
    if(!value.is_object()||!value.contains(key))throw RuntimeReaderError("missing "+at+"."+key);
    return value.at(key);
}
const json& object(const json& value,const char* key,const std::string& at){
    const auto& result=field(value,key,at);
    if(!result.is_object())throw RuntimeReaderError(at+"."+key+" must be an object");
    return result;
}
const json& array(const json& value,const char* key,const std::string& at){
    const auto& result=field(value,key,at);
    if(!result.is_array())throw RuntimeReaderError(at+"."+key+" must be an array");
    return result;
}
std::string string(const json& value,const char* key,const std::string& at){
    const auto& result=field(value,key,at);
    if(!result.is_string()||result.get<std::string>().empty())throw RuntimeReaderError(at+"."+key+" must be nonempty text");
    return result.get<std::string>();
}
double number(const json& value,const char* key,const std::string& at){
    const auto& result=field(value,key,at);
    if(!result.is_number())throw RuntimeReaderError(at+"."+key+" must be finite");
    const double answer=result.get<double>();
    if(!std::isfinite(answer))throw RuntimeReaderError(at+"."+key+" must be finite");
    return answer;
}
bool boolean(const json& value,const char* key,const std::string& at){
    const auto& result=field(value,key,at);
    if(!result.is_boolean())throw RuntimeReaderError(at+"."+key+" must be boolean");
    return result.get<bool>();
}
Vec3 vector3(const json& value,const char* key,const std::string& at){
    const auto& a=array(value,key,at);
    if(a.size()!=3)throw RuntimeReaderError(at+"."+key+" needs three coordinates");
    for(const auto& v:a)if(!v.is_number()||!std::isfinite(v.get<double>()))
        throw RuntimeReaderError(at+"."+key+" nonfinite coordinate");
    return {a[0].get<double>(),a[1].get<double>(),a[2].get<double>()};
}
double dot(Vec3 x,Vec3 y){return x.x*y.x+x.y*y.y+x.z*y.z;}
void check_hierarchy(const std::vector<Body>& bodies,const std::vector<std::string>& parents){
    std::unordered_map<std::string,std::size_t> by_id;
    for(std::size_t i=0;i<bodies.size();++i)
        if(!by_id.emplace(bodies[i].id,i).second)throw RuntimeReaderError("duplicate body id "+bodies[i].id);
    std::size_t roots=0;
    for(std::size_t i=0;i<bodies.size();++i){
        if(parents[i].empty()){++roots;continue;}
        std::unordered_set<std::size_t> visited;
        auto cursor=i;
        while(!parents[cursor].empty()){
            if(!visited.insert(cursor).second)throw RuntimeReaderError("body parent cycle at "+bodies[cursor].id);
            const auto it=by_id.find(parents[cursor]);
            if(it==by_id.end())throw RuntimeReaderError("body missing parent "+parents[cursor]);
            cursor=it->second;
        }
    }
    if(roots!=1)throw RuntimeReaderError("body hierarchy needs one root");
}
void check_geometry(const std::vector<Body>& bodies){
    double total=0;for(const auto& b:bodies)total+=b.mu_m3_s2;
    if(!std::isfinite(total)||total<=0)throw RuntimeReaderError("body barycenter mass overflow");
    for(std::size_t i=0;i<bodies.size();++i)
    for(std::size_t j=i+1;j<bodies.size();++j){
        const auto separation=norm(bodies[i].state->position_m-bodies[j].state->position_m);
        if(!std::isfinite(separation)||separation<=bodies[i].radius_m+bodies[j].radius_m)
            throw RuntimeReaderError("body overlap: "+bodies[i].id+" and "+bodies[j].id);
    }
    Vec3 centre{},velocity{};double position_scale=0,velocity_scale=0;
    for(const auto& b:bodies){
        const double weight=b.mu_m3_s2/total;
        centre=centre+b.state->position_m*weight;
        velocity=velocity+b.state->velocity_mps*weight;
        position_scale=std::max(position_scale,norm(b.state->position_m));
        velocity_scale=std::max(velocity_scale,norm(b.state->velocity_mps));
    }
    if(norm(centre)>std::max(1.0,position_scale*1e-12)||
       norm(velocity)>std::max(1e-6,velocity_scale*1e-12))
        throw RuntimeReaderError("states disagree with system_barycenter origin");
}
}
RuntimeLoad read_runtime_snapshot(const std::string& json_bytes,const std::string& source_sha256){
    if(source_sha256.size()!=64||!std::all_of(source_sha256.begin(),source_sha256.end(),[](unsigned char c){return std::isxdigit(c)!=0;}))
        throw RuntimeReaderError("source hash must be 64 hexadecimal SHA-256 characters");
    if(sha256_hex(json_bytes)!=source_sha256)
        throw RuntimeReaderError("source hash mismatch for exact snapshot bytes");
    json document;
    try{document=json::parse(json_bytes);}catch(const json::exception& error){throw RuntimeReaderError(std::string("invalid snapshot JSON: ")+error.what());}
    if(!document.is_object()||!field(document,"schema_version","snapshot").is_number_integer()||
       document["schema_version"].get<int>()!=1)throw RuntimeReaderError("unsupported snapshot schema_version");
    const auto confidence=string(document,"confidence","snapshot");
    if(confidence=="runtime_verified")throw RuntimeReaderError("runtime_verified needs external comparison evidence");
    if(confidence!="runtime_observed_uncompared")throw RuntimeReaderError("runtime snapshot confidence invalid");
    const auto& capture=object(document,"capture","snapshot");
    RuntimeLoad loaded;
    loaded.exporter_id=string(capture,"exporter_id","capture");
    loaded.exporter_version=string(capture,"exporter_version","capture");
    loaded.game_version=string(capture,"game_version","capture");
    loaded.save_id=string(capture,"save_id","capture");
    loaded.capture_ut_s=number(capture,"capture_ut_s","capture");
    if(!boolean(capture,"principia_loaded","capture")||
       string(capture,"state_source","capture")!="principia_celestial_from_parent")
        throw RuntimeReaderError("capture state_source or Principia condition invalid");
    bool principia_mod=false;
    std::unordered_set<std::string> mod_ids;
    for(const auto& mod:array(capture,"mods","capture")){
        const auto id=string(mod,"id","capture.mods");string(mod,"version","capture.mods");
        if(!mod_ids.insert(id).second)throw RuntimeReaderError("duplicate capture.mods id "+id);
        principia_mod|=id=="Principia";
    }
    if(!principia_mod)throw RuntimeReaderError("capture mods missing Principia");
    const auto& frame=object(document,"frame","snapshot");
    loaded.snapshot.frame={string(frame,"origin","frame"),string(frame,"axes","frame"),
        string(frame,"handedness","frame"),boolean(frame,"inertial","frame")};
    if(loaded.snapshot.frame.origin!="system_barycenter"||
       loaded.snapshot.frame.axes!="principia_alicesun_frozen_at_capture"||
       loaded.snapshot.frame.handedness!="right"||!loaded.snapshot.frame.inertial||
       string(frame,"source_frame","frame")!="Principia/AliceSun"||
       string(frame,"transform_method","frame")!="parent_relative_sum_then_com_translation"||
       string(frame,"transform_version","frame")!="1")
        throw RuntimeReaderError("frame transform is not a supported inertial Principia capture");
    const auto& calendar=object(document,"calendar","snapshot");
    loaded.display_day_duration_s=number(calendar,"day_duration_s","calendar");
    loaded.display_origin_ut_s=number(calendar,"display_origin_ut_s","calendar");
    if(loaded.display_day_duration_s<=0||boolean(calendar,"use_leap_years","calendar"))
        throw RuntimeReaderError("calendar requires positive no-leap day");
    const auto& months=array(calendar,"month_lengths","calendar");
    int days=0;for(const auto& month:months){if(!month.is_number_integer()||month.get<int>()<=0)
        throw RuntimeReaderError("calendar month_lengths invalid");days+=month.get<int>();}
    if(months.size()!=12||days!=365)throw RuntimeReaderError("calendar month_lengths must total 365");
    std::vector<std::string> parents;
    const auto& list=array(document,"bodies","snapshot");
    if(list.size()<2)throw RuntimeReaderError("runtime snapshot needs two bodies");
    for(const auto& item:list){
        const auto id=string(item,"id","body");
        const auto& parent=field(item,"parent_id","body "+id);
        if(!parent.is_null()&&!parent.is_string())throw RuntimeReaderError("body parent_id invalid");
        if(parent.is_string()&&parent.get<std::string>().empty())throw RuntimeReaderError("body parent_id must be nonempty or null");
        parents.push_back(parent.is_null()?"":parent.get<std::string>());
        const double mu=number(item,"mu_m3_s2","body "+id),radius=number(item,"radius_m","body "+id);
        if(mu<=0||radius<=0)throw RuntimeReaderError("body "+id+" mu_m3_s2 or radius_m invalid");
        const auto& atmosphere=field(item,"atmosphere_boundary_m","body "+id);
        double altitude=0;
        if(!atmosphere.is_null()){
            if(!atmosphere.is_number()||!std::isfinite(atmosphere.get<double>())||(altitude=atmosphere.get<double>())<0)
                throw RuntimeReaderError("body "+id+" atmosphere_boundary_m invalid");
        }
        const double epoch=number(item,"state_epoch_ut_s","body "+id);
        if(epoch!=loaded.capture_ut_s)throw RuntimeReaderError("body "+id+" epoch mismatch");
        const State state{vector3(item,"position_m","body "+id),vector3(item,"velocity_mps","body "+id)};
        loaded.snapshot.bodies.push_back({id,mu,radius,state,epoch});
        loaded.atmosphere_boundaries.push_back({id,altitude});
    }
    check_hierarchy(loaded.snapshot.bodies,parents);
    check_geometry(loaded.snapshot.bodies);
    loaded.snapshot.analysis_ready=true;
    loaded.snapshot.confidence=confidence;
    loaded.snapshot.snapshot_hash=source_sha256;
    loaded.snapshot.state_epoch_ut_s=loaded.capture_ut_s;
    return loaded;
}
}
