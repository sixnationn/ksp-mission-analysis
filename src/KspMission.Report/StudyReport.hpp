#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <stdexcept>

namespace ksp {
class StudyReportError:public std::runtime_error {public:using std::runtime_error::runtime_error;};
void validate_study_report(const nlohmann::json& document);
void save_study_report(const nlohmann::json& document,const std::filesystem::path& path);
nlohmann::json load_study_report(const std::filesystem::path& path);
}
