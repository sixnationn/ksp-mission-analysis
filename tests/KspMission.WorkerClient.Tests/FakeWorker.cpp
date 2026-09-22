#include <nlohmann/json.hpp>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
using json=nlohmann::json;
int main(int argc,char** argv){const std::string mode=argc>1?argv[1]:"valid";
 if(mode=="never_read"){std::this_thread::sleep_for(std::chrono::seconds(30));return 0;}
 std::string request;std::getline(std::cin,request);
 auto started=json{{"protocol_version",1},{"request_id","client-test"},{"type","started"},
  {"snapshot_hash","synthetic-worker-v1"},{"source_confidence","synthetic_fixture"},{"total_cells",2}};
 auto progress=started;progress["type"]="progress";progress["sampled_cells"]=1;
 auto complete=started;complete["type"]="complete";complete["ranked_candidates"]=json::array();
 if(mode=="cancel_ack"){
  std::cout<<started.dump()<<std::endl;
  std::string control;std::getline(std::cin,control);
  auto parsed=json::parse(control,nullptr,false);
  if(!parsed.is_object()||parsed.value("protocol_version",0)!=1||parsed.value("command",std::string{})!="cancel"||
     parsed.value("request_id",std::string{})!="client-test")return 9;
  complete["type"]="cancelled";std::cout<<complete.dump()<<std::endl;return 0;}
 if(mode=="hang_cancel"){std::this_thread::sleep_for(std::chrono::seconds(30));return 0;}
 if(mode=="malformed"){std::cout<<"{bad\n";return 0;}
 if(mode=="oversized"){std::cout<<std::string(1024*1024+1,'x')<<'\n';return 0;}
 if(mode=="wrong_request")started["request_id"]="other";
 if(mode=="wrong_source")started["snapshot_hash"]="other";
 std::cout<<started.dump()<<std::endl;
 if(mode=="premature_eof")return 0;if(mode=="nonzero_exit")return 7;
 std::cout<<progress.dump()<<std::endl;
 if(mode=="progress_regress"){progress["sampled_cells"]=0;std::cout<<progress.dump()<<std::endl;}
 std::cout<<complete.dump()<<std::endl;}
