#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace ksp {
class StudyReportError:public std::runtime_error {public:using std::runtime_error::runtime_error;};
void validate_study_report(const nlohmann::json& document);
// Compose a historical study from exact runtime bytes, the submitted mission request, and its terminal worker event.
nlohmann::json compose_runtime_study_report(const std::string& runtime_json_bytes,
    const std::string& expected_sha256,const nlohmann::json& request,const nlohmann::json& terminal);
void save_study_report(const nlohmann::json& document,const std::filesystem::path& path);
nlohmann::json load_study_report(const std::filesystem::path& path);
}
