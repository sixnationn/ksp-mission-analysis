#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace ksp {
class EvaluationReportError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
nlohmann::json compose_fixed_evaluation_report(const std::string& runtime_bytes,
    const std::string& expected_sha256,const nlohmann::json& request,
    const std::vector<nlohmann::json>& events);
void validate_fixed_evaluation_report(const nlohmann::json& document);
void save_fixed_evaluation_report(const nlohmann::json& document,const std::filesystem::path& path);
nlohmann::json load_fixed_evaluation_report(const std::filesystem::path& path);
}
