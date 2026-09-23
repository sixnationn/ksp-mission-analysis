#include "UpdateCheck.hpp"
#include <stdexcept>
#include <string>
#include <iostream>

using namespace ksp::update;
namespace {
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
std::string response(const std::string& sha,const std::string& conclusion="success",
                     const std::string& status="completed",const std::string& branch="main"){
    return R"({"workflow_runs":[{"id":123456,"head_sha":")"+sha+
        R"(","conclusion":")"+conclusion+R"(","status":")"+status+
        R"(","head_branch":")"+branch+R"("}]})";
}
void rejects(const std::string& body){
    try{(void)inspect_latest(std::string(40,'a'),body);}
    catch(const std::exception&){return;}
    throw std::runtime_error("invalid latest-run response accepted");
}
}
int main(){
    try{
        const std::string current(40,'a'),other(40,'b');
        const auto same=inspect_latest(current,response(current));
        check(same.kind==Kind::Current,"matching bundle reported current");
        check(same.url=="https://github.com/sixnationn/ksp-mission-analysis/actions/runs/123456",
              "run link built from validated numeric ID");
        const auto changed=inspect_latest(current,response(other));
        check(changed.kind==Kind::Different,"different successful build reported");
        check(changed.commit==other,"latest source commit retained");
        check(inspect_latest("",response(other)).kind==Kind::UnknownLocal,
              "bundle without a valid source identity is not called current");
        rejects(R"({"workflow_runs":[]})");
        rejects(response(other,"failure"));
        rejects(response(other,"success","in_progress"));
        rejects(response(other,"success","completed","feature"));
        rejects(response("invalid-sha"));
        rejects(R"({"workflow_runs":[{"id":-1,"head_sha":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","conclusion":"success","status":"completed","head_branch":"main"}]})");
        rejects("not JSON");
        std::cout<<"update response contract passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
