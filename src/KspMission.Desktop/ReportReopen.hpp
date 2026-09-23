#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace ksp {
class ReportReopenError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
struct CurrentReportSource {
    std::string snapshot_hash;
    std::string confidence;
    std::string frame_origin;
    std::string frame_axes;
    std::string frame_handedness;
    double state_epoch_ut_s{};
};
enum class SavedReportKind { screened_study, fixed_impulse_evaluation, bounded_shooting };
struct ReopenedReport {
    SavedReportKind kind;
    nlohmann::json document;
    std::string summary;
};
ReopenedReport reopen_saved_report(const std::filesystem::path& path,const CurrentReportSource& current);
}
