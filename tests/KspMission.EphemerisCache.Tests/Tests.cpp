#include "EphemerisCache.hpp"
#include "RuntimeReader.hpp"
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <string>

using namespace ksp;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void fails(const std::function<void()>& f,const char* message){try{f();}catch(const EphemerisCacheError&){return;}throw std::runtime_error(message);}
void query_fails(const std::function<void()>& f,const char* message){try{f();}catch(const EphemerisError&){return;}throw std::runtime_error(message);}
Snapshot pair(){constexpr double a=1.495978707e11,m1=1.32712440018e20,m2=3.986004418e14;
 double w=std::sqrt((m1+m2)/(a*a*a));Snapshot s;s.analysis_ready=true;s.confidence="synthetic_fixture";s.snapshot_hash="cache-pair-v1";s.state_epoch_ut_s=0;s.frame={"barycenter","X,Y,Z","right",true};
 s.bodies={{"star",m1,1e8,State{{-a*m2/(m1+m2),0,0},{0,-a*w*m2/(m1+m2),0}},0},{"planet",m2,1e6,State{{a*m1/(m1+m2),0,0},{0,a*w*m1/(m1+m2),0}},0}};return s;}
Settings settings(){Settings q;q.start_ut_s=0;q.end_ut_s=1200;q.step_s=600;q.max_position_fit_error_m=100;q.max_velocity_fit_error_mps=0.01;q.max_steps=10;return q;}
std::string bytes(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
void write(const std::filesystem::path& path,const std::string& data){std::ofstream f(path,std::ios::binary|std::ios::trunc);f.write(data.data(),static_cast<std::streamsize>(data.size()));}
std::uint32_t u32(const std::string& data,std::size_t at){std::uint32_t value=0;for(int i=0;i<4;++i)value|=std::uint32_t(static_cast<unsigned char>(data[at+i]))<<(8*i);return value;}
void repair_digest(std::string& data){auto digest=sha256_hex(data.substr(0,data.size()-32));for(std::size_t i=0;i<32;++i)data[data.size()-32+i]=static_cast<char>(std::stoul(digest.substr(2*i,2),nullptr,16));}
std::size_t counts_offset(const std::string& data){std::size_t at=12;for(int i=0;i<10;++i){auto n=u32(data,at);at+=4+n;}return at+1+9*8+8;}
}
int main(){try{
 auto s=pair();auto q=settings();auto e=integrate(s,q);
 std::random_device random;const auto unique=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(random());
 const auto directory=std::filesystem::temp_directory_path()/("ksp-ephemeris-cache-test-"+unique);
 check(std::filesystem::create_directory(directory),"unique cache test directory creation");
 const auto path=directory/"cache.bin";
 save_ephemeris_cache(e,s,q,path);auto original=bytes(path);check(!original.empty(),"cache file written");
 auto loaded=load_ephemeris_cache(path,s,q);check(loaded.metadata.snapshot_hash==e.metadata.snapshot_hash&&loaded.metadata.measured_max_position_error_m==e.metadata.measured_max_position_error_m&&loaded.body_ids==e.body_ids,"metadata and body order roundtrip");
 check(loaded.metadata.integrator==e.metadata.integrator&&loaded.metadata.integrator_version==e.metadata.integrator_version&&loaded.metadata.interpolation==e.metadata.interpolation&&loaded.metadata.safety_split_count==e.metadata.safety_split_count,"integrator/interpolation metadata roundtrip");
 check(loaded.samples.size()==e.samples.size(),"sample body dimensions roundtrip");
 for(std::size_t i=0;i<e.samples.size();++i){check(loaded.samples[i].size()==e.samples[i].size(),"sample time dimensions roundtrip");for(std::size_t j=0;j<e.samples[i].size();++j){const auto& a=e.samples[i][j],&b=loaded.samples[i][j];check(a.position_m.x==b.position_m.x&&a.position_m.y==b.position_m.y&&a.position_m.z==b.position_m.z&&a.velocity_mps.x==b.velocity_mps.x&&a.velocity_mps.y==b.velocity_mps.y&&a.velocity_mps.z==b.velocity_mps.z,"all stored sample states roundtrip");}}
 for(const char* id:{"star","planet"})for(double t:{0.0,300.0,1200.0}){auto a=e.query(id,t),b=loaded.query(id,t);check(a.position_m.x==b.position_m.x&&a.position_m.y==b.position_m.y&&a.velocity_mps.x==b.velocity_mps.x&&a.velocity_mps.y==b.velocity_mps.y,"query roundtrip");}
 query_fails([&]{loaded.query("planet",1201);},"loaded coverage rejection");
 query_fails([&]{loaded.query("missing",0);},"loaded unknown body rejection");
 fails([&]{save_ephemeris_cache(e,s,q,path);},"existing destination rejected");check(bytes(path)==original,"existing destination intact");
 auto altered=s;altered.snapshot_hash="other";fails([&]{load_ephemeris_cache(path,altered,q);},"hash mismatch");
 altered=s;altered.confidence="runtime_observed_uncompared";fails([&]{load_ephemeris_cache(path,altered,q);},"confidence mismatch");
 altered=s;altered.frame.axes="rotating";fails([&]{load_ephemeris_cache(path,altered,q);},"frame mismatch");
 altered=s;altered.state_epoch_ut_s=1;altered.bodies[0].epoch_ut_s=1;altered.bodies[1].epoch_ut_s=1;fails([&]{load_ephemeris_cache(path,altered,q);},"epoch mismatch");
 altered=s;std::swap(altered.bodies[0],altered.bodies[1]);fails([&]{load_ephemeris_cache(path,altered,q);},"body order mismatch");
 altered=s;altered.bodies[0].mu_m3_s2*=2;fails([&]{load_ephemeris_cache(path,altered,q);},"mu mismatch");
 altered=s;altered.bodies[0].radius_m*=2;fails([&]{load_ephemeris_cache(path,altered,q);},"radius mismatch");
 altered=s;altered.bodies[0].state->position_m.x+=1;fails([&]{load_ephemeris_cache(path,altered,q);},"initial state mismatch");
 auto changed=q;changed.step_s=300;fails([&]{load_ephemeris_cache(path,s,changed);},"step mismatch");
 changed=q;changed.end_ut_s=1800;fails([&]{load_ephemeris_cache(path,s,changed);},"coverage mismatch");
 changed=q;changed.max_position_fit_error_m=200;fails([&]{load_ephemeris_cache(path,s,changed);},"fit limit mismatch");
 changed=q;changed.max_steps=1;fails([&]{load_ephemeris_cache(path,s,changed);},"max_steps admission");
 altered=s;altered.analysis_ready=false;fails([&]{load_ephemeris_cache(path,altered,q);},"provisional snapshot");
 altered=s;altered.frame.inertial=false;fails([&]{save_ephemeris_cache(e,altered,q,path.string()+".bad");},"noninertial save rejection");
 altered=s;altered.bodies[0].state->velocity_mps.x=std::numeric_limits<double>::quiet_NaN();fails([&]{load_ephemeris_cache(path,altered,q);},"nonfinite snapshot");
 altered=s;altered.bodies[1].id="star";fails([&]{load_ephemeris_cache(path,altered,q);},"duplicate caller body ID");
 altered=s;altered.bodies[1].id="";fails([&]{load_ephemeris_cache(path,altered,q);},"empty caller body ID");
 auto invalid=e;invalid.metadata.result_label="principia_matched";fails([&]{save_ephemeris_cache(invalid,s,q,path.string()+".bad");},"unsupported label");
 invalid=e;invalid.metadata.measured_max_velocity_error_mps=std::numeric_limits<double>::quiet_NaN();fails([&]{save_ephemeris_cache(invalid,s,q,path.string()+".bad");},"nonfinite metadata");
 invalid=e;invalid.samples[0][1].position_m.x=std::numeric_limits<double>::quiet_NaN();fails([&]{save_ephemeris_cache(invalid,s,q,path.string()+".bad");},"nonfinite sample");
 check(!std::filesystem::exists(path.string()+".bad"),"invalid save created no destination");
 auto mutation=[&](std::string data,const char* name){write(path,data);fails([&]{load_ephemeris_cache(path,s,q);},name);};
 auto data=original;data[0]^=1;mutation(data,"magic");
 data=original;data[8]^=1;mutation(data,"version");
 data=original;data.resize(data.size()-1);mutation(data,"truncation");
 data=original;data.push_back('\0');mutation(data,"appended bytes");
 data=original;data[data.size()/2]^=1;mutation(data,"payload mutation");
 data=original;data.back()^=1;mutation(data,"checksum mutation");
 data=original;auto counts=counts_offset(data);for(int i=0;i<4;++i)data[counts+i]=static_cast<char>(0xff);repair_digest(data);mutation(data,"body count overflow with valid checksum");
 data=original;counts=counts_offset(data);std::size_t first_sample=counts+4+8;
 for(int i=0;i<2;++i){auto length=u32(data,first_sample);first_sample+=4+length+9*8;}
 constexpr unsigned char nan_bits[8]={0,0,0,0,0,0,0xf8,0x7f};for(int i=0;i<8;++i)data[first_sample+i]=static_cast<char>(nan_bits[i]);repair_digest(data);mutation(data,"nonfinite sample with valid checksum");
 write(path,original);check(load_ephemeris_cache(path,s,q).body_ids==e.body_ids,"valid cache retained after failures");
 std::filesystem::remove(path);std::filesystem::remove(directory);std::cout<<"ephemeris cache tests passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
