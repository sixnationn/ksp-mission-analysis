#pragma once
#include "SearchGrid.hpp"
#include <nlohmann/json.hpp>
#include <functional>
#include <string>
namespace ksp {
using WorkerEmit = std::function<void(const nlohmann::json&)>;
using WorkerCancel = std::function<bool()>;
using WorkerAfterRead = std::function<void()>;
// The final callback is a deterministic test seam after the bounded file read.
void process_start_line(const std::string& line,const WorkerEmit& emit,const WorkerCancel& cancelled,
                        const WorkerAfterRead& after_runtime_bytes_read_for_test={});
std::string worker_candidate_id(const GridRequest& request,const DatedLegCandidate& candidate);
bool is_cancel_line(const std::string& line,const std::string& request_id);
}
