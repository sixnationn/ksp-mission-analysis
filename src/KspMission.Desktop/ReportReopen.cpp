#include "ReportReopen.hpp"
#include "StudyReport.hpp"
#include "EvaluationReport.hpp"
#include "RuntimeReader.hpp"
#include <cmath>
#include <fstream>

namespace ksp {
namespace {
using nlohmann::json;
constexpr std::streamoff report_limit=32*1024*1024;
std::string field(const json& object,const char* key){
    if(!object.is_object()||!object.contains(key)||!object.at(key).is_string())
        throw ReportReopenError(std::string("report source ")+key+" missing");
    return object.at(key).get<std::string>();
}
double epoch(const json& object){
    if(!object.is_object()||!object.contains("state_epoch_ut_s")||
       !object.at("state_epoch_ut_s").is_number())throw ReportReopenError("report source epoch missing");
    const auto value=object.at("state_epoch_ut_s").get<double>();
    if(!std::isfinite(value))throw ReportReopenError("report source epoch invalid");
    return value;
}
void require_source_match(const json& source,const CurrentReportSource& current){
    if(current.confidence!="runtime_observed_uncompared"||
       field(source,"snapshot_hash")!=current.snapshot_hash||
       field(source,"confidence")!=current.confidence||
       field(source,"frame_origin")!=current.frame_origin||
       field(source,"frame_axes")!=current.frame_axes||
       field(source,"frame_handedness")!=current.frame_handedness||
       epoch(source)!=current.state_epoch_ut_s)
        throw ReportReopenError("saved report differs from the imported runtime source");
}
json embedded_source(const json& document){
    const auto bytes=field(document,"runtime_snapshot_json_bytes");
    const auto hash=field(document,"snapshot_sha256");
    const auto loaded=read_runtime_snapshot(bytes,hash);
    return {{"snapshot_hash",hash},{"confidence",loaded.snapshot.confidence},
        {"frame_origin",loaded.snapshot.frame.origin},{"frame_axes",loaded.snapshot.frame.axes},
        {"frame_handedness",loaded.snapshot.frame.handedness},
        {"state_epoch_ut_s",*loaded.snapshot.state_epoch_ut_s}};
}
}
ReopenedReport reopen_saved_report(const std::filesystem::path& path,const CurrentReportSource& current){
 try{
    if(path.empty())throw ReportReopenError("Enter a saved report path");
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)throw ReportReopenError("Saved report cannot be opened");
    const auto length=file.tellg();
    if(length<=0||length>report_limit)throw ReportReopenError("Saved report exceeds 32 MiB or is empty");
    std::string bytes(static_cast<std::size_t>(length),'\0');
    file.seekg(0);
    if(!file.read(bytes.data(),length))throw ReportReopenError("Saved report read incomplete");
    if(file.peek()!=std::char_traits<char>::eof())throw ReportReopenError("Saved report changed during read");
    auto document=json::parse(bytes,nullptr,false);
    if(document.is_discarded()||!document.is_object())throw ReportReopenError("Saved report JSON invalid");
    if(!document.contains("schema_version")||document.at("schema_version")!=1)
        throw ReportReopenError("Saved report schema unsupported");
    if(!document.contains("report_kind")){
        if(bytes.size()>16*1024*1024)throw ReportReopenError("Historical study exceeds 16 MiB");
        validate_study_report(document);
        require_source_match(document.at("source"),current);
        const auto& result=document.at("result");
        const auto summary="Historical patched-conic screen only · "+result.at("status").get<std::string>()+
            " · ranked routes "+std::to_string(result.at("ranked_routes").size())+
            " · runtime source observed, uncompared · SI / UT";
        return {SavedReportKind::screened_study,std::move(document),summary};
    }
    const auto kind=field(document,"report_kind");
    if(kind=="fixed_impulse_evaluation"){
        validate_fixed_evaluation_report(document);
        require_source_match(embedded_source(document),current);
        return {SavedReportKind::fixed_impulse_evaluation,std::move(document),
            "Historical checkpointed fixed-impulse trial · independent n-body · source observed, uncompared · SI / UT"};
    }
    if(kind=="bounded_shooting"){
        validate_shooting_report(document);
        require_source_match(embedded_source(document),current);
        const auto& terminal=document.at("events").back();
        const auto accepted=terminal.at("status")=="checkpointed_accepted";
        const std::string summary=accepted?
            "Historical bounded shooting · checkpointed acceptance only · source observed, uncompared · SI / UT":
            "Historical bounded shooting diagnostic · no checkpointed acceptance · source observed, uncompared · SI / UT";
        return {SavedReportKind::bounded_shooting,std::move(document),summary};
    }
    throw ReportReopenError("Saved report kind unsupported");
 }catch(const ReportReopenError&){throw;}
 catch(const std::exception& issue){throw ReportReopenError(std::string("Saved report rejected: ")+issue.what());}
}
}
