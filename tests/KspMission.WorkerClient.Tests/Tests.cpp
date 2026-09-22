#include "WorkerClient.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
using namespace ksp;
using json=nlohmann::json;
namespace {
int checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class F> void rejects(F action,const char* fragment){try{action();}catch(const WorkerClientError& e){check(std::string(e.what()).find(fragment)!=std::string::npos,"wrong validation diagnostic");return;}throw std::runtime_error(fragment);}
std::string request(){return json{{"protocol_version",1},{"command","start"},{"request_id","client-test"},
 {"source",{{"mode","synthetic_fixture"},{"expected_snapshot_hash","synthetic-worker-v1"}}},
 {"grid",{{"central_body_id","star"},{"departure_body_id","home"},{"arrival_body_id","target"},
 {"launch_start_ut_s",0},{"launch_end_ut_s",0},{"launch_step_s",100},{"flight_min_s",2500},{"flight_max_s",2500},{"flight_step_s",100},
 {"reference_normal",{0,0,1}},{"branch","short"},{"direction","positive"},{"max_position_residual_m",0.1},
 {"max_velocity_residual_mps",1e-4},{"max_candidates",8},{"stable_seed",42},{"central_atmosphere_altitude_m",0.0},{"central_safety_margin_m",0.0}}},
 {"refine",{{"enabled",false}}}}.dump();}
json event(std::string type){json e={{"protocol_version",1},{"request_id","client-test"},{"type",type},
 {"snapshot_hash","synthetic-worker-v1"},{"source_confidence","synthetic_fixture"}};
 if(type=="started")e["total_cells"]=2;if(type=="progress"){e["sampled_cells"]=1;e["total_cells"]=2;}
 if(type=="complete")e["ranked_candidates"]=json::array();return e;}
void validator_cases(){WorkerEventValidator v("client-test","synthetic-worker-v1","synthetic_fixture",8);
 rejects([&]{v.accept_line("{");},"JSON");rejects([&]{v.accept_line(std::string(1024*1024+1,'x'));},"line limit");
 auto bad=event("started");bad["protocol_version"]=2;rejects([&]{v.accept_line(bad.dump());},"protocol_version");
 bad=event("started");bad["request_id"]="other";rejects([&]{v.accept_line(bad.dump());},"request_id");
 bad=event("started");bad["snapshot_hash"]="other";rejects([&]{v.accept_line(bad.dump());},"snapshot_hash");
 bad=event("started");bad["type"]="fabricated";rejects([&]{v.accept_line(bad.dump());},"event type");
 v.accept_line(event("started").dump());v.accept_line(event("progress").dump());
 bad=event("progress");bad["sampled_cells"]=0;rejects([&]{v.accept_line(bad.dump());},"progress regression");
 bad=event("complete");bad["ranked_candidates"]=json::array({{{"rank",2},{"status","screened_seed"},
  {"snapshot_hash","synthetic-worker-v1"},{"source_confidence","synthetic_fixture"},{"screening_score_mps",1}}});
 rejects([&]{v.accept_line(bad.dump());},"ranked order");
 WorkerEventValidator clean("client-test","synthetic-worker-v1","synthetic_fixture",8);
 clean.accept_line(event("started").dump());clean.accept_line(event("complete").dump());check(clean.terminal(),"terminal accepted");}
ClientOptions options(const std::string& path,const std::string& mode){ClientOptions x;x.executable_path=path;x.arguments={mode};
 x.request_line=request();x.request_id="client-test";x.expected_snapshot_hash="synthetic-worker-v1";
 x.expected_source_confidence="synthetic_fixture";x.timeout=std::chrono::milliseconds(5000);
 x.cancel_grace=std::chrono::milliseconds(150);x.max_retained_events=64;return x;}
std::vector<json> finish(WorkerClient& c){check(c.wait_for(std::chrono::seconds(8)),"client finishes");return c.drain();}
bool error(const std::vector<json>& events,const std::string& code){for(const auto& e:events)if(e.value("type",std::string{})=="client_error"&&e.value("code",std::string{})==code)return true;return false;}
void process_cases(const std::string& fake){WorkerClient c;auto x=options(fake,"valid");x.executable_path=fake+".missing";
 check(c.start(x),"missing executable asynchronous attempt");check(error(finish(c),"spawn_failed"),"missing executable error");
 for(const auto& p:{std::pair{"malformed","invalid_event"},{"oversized","invalid_event"},{"wrong_request","invalid_event"},
  {"wrong_source","invalid_event"},{"progress_regress","invalid_event"},{"premature_eof","premature_eof"},{"nonzero_exit","nonzero_exit"}}){
  x=options(fake,p.first);check(c.start(x),"fake launched");auto got=finish(c);if(!error(got,p.second))throw std::runtime_error(std::string("fake failure classified: ")+p.first+" expected "+p.second+" got "+json(got).dump());}
 x=options(fake,"hang_cancel");check(c.start(x),"hang fake launched");check(!c.start(x),"second start rejected");
 c.cancel();check(error(finish(c),"cancel_timeout"),"cancel escalation and reap");
 x=options(fake,"cancel_ack");check(c.start(x),"cancel acknowledging fake launched");
 std::vector<json> cancelled;bool saw_started=false;
 const auto ready_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 while(!saw_started&&std::chrono::steady_clock::now()<ready_deadline){
  auto batch=c.drain();for(auto& event:batch){
   if(event.value("type",std::string{})=="started")saw_started=true;
   cancelled.push_back(std::move(event));
  }
  if(!saw_started)std::this_thread::sleep_for(std::chrono::milliseconds(5));
 }
 check(saw_started,"fake worker accepted request before cancellation");c.cancel();
 auto terminal=finish(c);cancelled.insert(cancelled.end(),terminal.begin(),terminal.end());
 check(!cancelled.empty()&&cancelled.back().value("type",std::string{})=="cancelled","versioned cancellation acknowledged");
 x=options(fake,"valid");check(c.start(x),"restart after reap");auto events=finish(c);
 check(!events.empty()&&events.back()["type"]=="complete","valid fake completion");}
void blocked_write_cases(const std::string& fake){
 auto x=options(fake,"never_read");x.request_line=std::string(1024*1024-128,'x');
 x.timeout=std::chrono::milliseconds(350);x.cancel_grace=std::chrono::milliseconds(100);
 WorkerClient timed;auto begun=std::chrono::steady_clock::now();
 check(timed.start(x),"blocked request starts asynchronously");
 check(error(finish(timed),"timeout"),"blocked request write reaches deadline");
 check(std::chrono::steady_clock::now()-begun<std::chrono::seconds(3),"blocked request deadline bounded");
 x.timeout=std::chrono::seconds(10);WorkerClient cancelled;begun=std::chrono::steady_clock::now();
 check(cancelled.start(x),"blocked request starts for cancellation");cancelled.cancel();
 check(error(finish(cancelled),"cancelled"),"blocked request write responds to cancellation");
 check(std::chrono::steady_clock::now()-begun<std::chrono::seconds(3),"blocked request cancellation bounded");
 begun=std::chrono::steady_clock::now();{WorkerClient owner;check(owner.start(x),"blocked request starts for destruction");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));}
 check(std::chrono::steady_clock::now()-begun<std::chrono::seconds(3),"blocked request destruction bounded");
}
void real_case(const std::string& worker){WorkerClient c;auto x=options(worker,"valid");x.arguments.clear();
 check(c.start(x),"real worker launched");auto events=finish(c);
 if(events.empty()||events.front().value("type",std::string{})!="started"||
    events.back().value("type",std::string{})!="complete")
  throw std::runtime_error("real worker ordered terminal: "+json(events).dump());
 ++checks;}
}
int main(int argc,char** argv){try{if(argc!=3)throw std::runtime_error("paths required");validator_cases();process_cases(argv[2]);blocked_write_cases(argv[2]);real_case(argv[1]);
 std::cout<<"PASS "<<checks<<" worker client checks\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
