#pragma once
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace ksp {
class WorkerClientError:public std::runtime_error {public:using std::runtime_error::runtime_error;};
enum class WorkerResultKind { screened_seed, screened_route };
class WorkerEventValidator {
public:
    WorkerEventValidator(std::string request_id,std::string snapshot_hash,std::string source_confidence,
                         std::size_t max_candidates,WorkerResultKind result_kind=WorkerResultKind::screened_seed);
    nlohmann::json accept_line(const std::string& line);
    bool terminal() const noexcept {return terminal_;}
private:
    std::string request_id_,snapshot_hash_,source_confidence_;
    std::size_t max_candidates_,total_cells_=0,last_sampled_=0;
    WorkerResultKind result_kind_;
    bool started_=false,terminal_=false;
};
struct ClientOptions {
    std::string executable_path,request_line,request_id,expected_snapshot_hash,expected_source_confidence;
    std::vector<std::string> arguments;
    std::chrono::milliseconds timeout{30000},cancel_grace{500};
    std::size_t max_retained_events=512,max_candidates=1000;
    WorkerResultKind result_kind=WorkerResultKind::screened_seed;
};
class WorkerClient {
public:
    WorkerClient()=default;
    WorkerClient(const WorkerClient&)=delete;WorkerClient& operator=(const WorkerClient&)=delete;
    ~WorkerClient();
    bool start(ClientOptions options);
    void cancel() noexcept;
    bool running() const noexcept {return active_.load();}
    bool wait_for(std::chrono::milliseconds duration);
    std::vector<nlohmann::json> drain();
private:
    void run(ClientOptions options) noexcept;
    bool push(nlohmann::json value,std::size_t max_count);
    void failure(const std::string& code,const std::string& detail,std::size_t max_count);
    std::thread thread_;
    std::atomic<bool> active_{false},cancel_requested_{false},shutdown_{false};
    std::mutex queue_mutex_;
    std::deque<nlohmann::json> queue_;
};
}
