#pragma once
#include <nlohmann/json.hpp>
#include <functional>
#include <string>
namespace ksp {
using WorkerEmit = std::function<void(const nlohmann::json&)>;
using WorkerCancel = std::function<bool()>;
void process_start_line(const std::string& line,const WorkerEmit& emit,const WorkerCancel& cancelled);
bool is_cancel_line(const std::string& line,const std::string& request_id);
}
