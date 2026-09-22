#pragma once
#include "Spacecraft.hpp"
#include <stdexcept>
#include <string>
#include <vector>

namespace ksp {
struct RuntimeLoad {
    Snapshot snapshot;
    std::vector<AtmosphereBoundary> atmosphere_boundaries;
    std::string exporter_id,exporter_version,game_version,save_id;
    double capture_ut_s=0,display_day_duration_s=0,display_origin_ut_s=0;
};
class RuntimeReaderError : public std::runtime_error {public: using std::runtime_error::runtime_error;};
// SHA-256 of the exact UTF-8 JSON bytes; for the exporter this matches the .NET reader.
std::string sha256_hex(const std::string& bytes);
// The claimed source_sha256 is checked against these exact bytes before parsing.
RuntimeLoad read_runtime_snapshot(const std::string& json_bytes,const std::string& source_sha256);
}
