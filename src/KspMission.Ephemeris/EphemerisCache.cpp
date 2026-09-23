#include "EphemerisCache.hpp"
#include "RuntimeReader.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ksp {
namespace {
static_assert(sizeof(double)==8&&std::numeric_limits<double>::is_iec559&&CHAR_BIT==8,
    "ephemeris cache requires IEEE-754 binary64 and 8-bit bytes");
constexpr std::size_t max_file_bytes=128ull*1024*1024;
constexpr std::size_t max_bodies=128,max_text_bytes=4096;
constexpr std::array<std::uint8_t,8> magic={'K','S','P','2','E','P','H',0};
using Bytes=std::vector<std::uint8_t>;
void need(bool ok,const char* message){if(!ok)throw EphemerisCacheError(message);}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool finite(State v){return finite(v.position_m)&&finite(v.velocity_mps);}
bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool same(State a,State b){return same(a.position_m,b.position_m)&&same(a.velocity_mps,b.velocity_mps);}
void put_u8(Bytes& out,std::uint8_t v){out.push_back(v);}
void put_u32(Bytes& out,std::uint32_t v){for(int i=0;i<4;++i)put_u8(out,static_cast<std::uint8_t>(v>>(8*i)));}
void put_u64(Bytes& out,std::uint64_t v){for(int i=0;i<8;++i)put_u8(out,static_cast<std::uint8_t>(v>>(8*i)));}
void put_double(Bytes& out,double v){need(std::isfinite(v),"nonfinite binary64 value");put_u64(out,std::bit_cast<std::uint64_t>(v));}
void put_text(Bytes& out,const std::string& value){need(!value.empty()&&value.size()<=max_text_bytes,"empty or oversized cache text");put_u32(out,static_cast<std::uint32_t>(value.size()));out.insert(out.end(),value.begin(),value.end());}
void put_vec(Bytes& out,Vec3 v){put_double(out,v.x);put_double(out,v.y);put_double(out,v.z);}
void put_state(Bytes& out,State s){put_vec(out,s.position_m);put_vec(out,s.velocity_mps);}
void validate_input(const Snapshot& s,const Settings& q){
 need(s.analysis_ready,"analysis_ready false");need(!s.confidence.empty()&&s.confidence!="raw_config_provisional"&&s.confidence!="requested_override_preview","source provisional");
 need(!s.snapshot_hash.empty()&&s.snapshot_hash.size()<=max_text_bytes,"snapshot hash missing or oversized");
 need(s.state_epoch_ut_s&&std::isfinite(*s.state_epoch_ut_s),"snapshot epoch invalid");
 need(!s.frame.origin.empty()&&!s.frame.axes.empty()&&s.frame.handedness=="right"&&s.frame.inertial,"inertial frame invalid");
 need(s.bodies.size()>=2&&s.bodies.size()<=max_bodies,"body count invalid");std::set<std::string> ids;
 for(const auto& b:s.bodies){need(!b.id.empty()&&b.id.size()<=max_text_bytes&&ids.insert(b.id).second,"body ID empty, duplicate or oversized");
  need(std::isfinite(b.mu_m3_s2)&&b.mu_m3_s2>0&&std::isfinite(b.radius_m)&&b.radius_m>0,"body physical parameter invalid");
  need(b.state&&finite(*b.state)&&b.epoch_ut_s&&*b.epoch_ut_s==*s.state_epoch_ut_s,"body state or epoch invalid");}
 need(q.result_label=="independent_newtonian_nbody","unsupported result label");
 need(std::isfinite(q.start_ut_s)&&std::isfinite(q.end_ut_s)&&std::isfinite(q.step_s)&&q.start_ut_s==*s.state_epoch_ut_s&&q.end_ut_s>q.start_ut_s&&q.step_s>0,"coverage or step invalid");
 const double steps=(q.end_ut_s-q.start_ut_s)/q.step_s;
 need(std::isfinite(steps)&&steps>=1&&steps<=static_cast<double>(q.max_steps)&&std::abs(steps-std::round(steps))<=1e-9,"step count or max_steps invalid");
 need(steps+1<=static_cast<double>(max_file_bytes/(s.bodies.size()*48ull))&&steps<static_cast<double>(std::numeric_limits<std::int64_t>::max()),"cache sample count exceeds file cap");
 need(std::isfinite(q.max_position_fit_error_m)&&q.max_position_fit_error_m>0&&std::isfinite(q.max_velocity_fit_error_mps)&&q.max_velocity_fit_error_mps>0,"fit limit invalid");
}
std::size_t sample_count(const Settings& q){return static_cast<std::size_t>(std::llround((q.end_ut_s-q.start_ut_s)/q.step_s))+1;}
void validate_ephemeris(const Ephemeris& e,const Snapshot& s,const Settings& q){
 need(cache_compatible(e.metadata,s,q),"ephemeris metadata incompatible with snapshot/settings");
 need(e.body_ids.size()==s.bodies.size()&&e.samples.size()==s.bodies.size(),"ephemeris body dimensions invalid");
 const auto count=sample_count(q);need(count>=2,"ephemeris sample count invalid");
 need(count<=max_file_bytes/(s.bodies.size()*48ull),"ephemeris sample bytes exceed cap");
 for(std::size_t i=0;i<s.bodies.size();++i){need(e.body_ids[i]==s.bodies[i].id,"ephemeris body order mismatch");
  need(e.samples[i].size()==count,"ephemeris sample count mismatch");
  need(same(e.samples[i].front(),*s.bodies[i].state),"first sample differs from snapshot");
  for(const auto& value:e.samples[i])need(finite(value),"nonfinite ephemeris sample");}
}
struct Reader {
 const Bytes& in;std::size_t at=0,limit=0;
 void room(std::size_t count){need(count<=limit-at,"cache truncated or count overflow");}
 std::uint8_t u8(){room(1);return in[at++];}
 std::uint32_t u32(){room(4);std::uint32_t out=0;for(int i=0;i<4;++i)out|=std::uint32_t(u8())<<(8*i);return out;}
 std::uint64_t u64(){room(8);std::uint64_t out=0;for(int i=0;i<8;++i)out|=std::uint64_t(u8())<<(8*i);return out;}
 double real(){double v=std::bit_cast<double>(u64());need(std::isfinite(v),"nonfinite cache value");return v;}
 std::string str(){const auto length=u32();need(length>0&&length<=max_text_bytes,"cache string size invalid");room(length);std::string out(reinterpret_cast<const char*>(in.data()+at),length);at+=length;return out;}
 Vec3 vec(){return {real(),real(),real()};}
 State state(){return {vec(),vec()};}
};
void put_metadata(Bytes& out,const Metadata& m){
 for(const auto* s:{&m.snapshot_hash,&m.source_confidence,&m.frame_origin,&m.frame_axes,&m.frame_handedness,&m.force_model,&m.integrator,&m.integrator_version,&m.result_label,&m.interpolation})put_text(out,*s);
 put_u8(out,m.frame_inertial?1:0);
 for(double v:{m.state_epoch_ut_s,m.start_ut_s,m.end_ut_s,m.step_s,m.segment_spacing_s,m.max_position_fit_error_m,m.max_velocity_fit_error_mps,m.measured_max_position_error_m,m.measured_max_velocity_error_mps})put_double(out,v);
 put_u64(out,m.safety_split_count);
}
Metadata get_metadata(Reader& r){Metadata m;
 for(auto* s:{&m.snapshot_hash,&m.source_confidence,&m.frame_origin,&m.frame_axes,&m.frame_handedness,&m.force_model,&m.integrator,&m.integrator_version,&m.result_label,&m.interpolation})*s=r.str();
 auto inertial=r.u8();need(inertial<=1,"cache inertial flag invalid");m.frame_inertial=inertial==1;
 for(auto* v:{&m.state_epoch_ut_s,&m.start_ut_s,&m.end_ut_s,&m.step_s,&m.segment_spacing_s,&m.max_position_fit_error_m,&m.max_velocity_fit_error_mps,&m.measured_max_position_error_m,&m.measured_max_velocity_error_mps})*v=r.real();
 const auto splits=r.u64();need(splits<=std::numeric_limits<std::size_t>::max(),"safety split count overflow");m.safety_split_count=static_cast<std::size_t>(splits);return m;
}
Bytes encode(const Ephemeris& e,const Snapshot& s,const Settings& q){Bytes out;out.reserve(4096+s.bodies.size()*sample_count(q)*48ull);out.insert(out.end(),magic.begin(),magic.end());put_u32(out,1);put_metadata(out,e.metadata);
 put_u32(out,static_cast<std::uint32_t>(s.bodies.size()));put_u64(out,sample_count(q));
 for(const auto& b:s.bodies){put_text(out,b.id);put_double(out,b.mu_m3_s2);put_double(out,b.radius_m);put_double(out,*b.epoch_ut_s);put_state(out,*b.state);}
 for(const auto& row:e.samples)for(const auto& value:row)put_state(out,value);
 need(out.size()+32<=max_file_bytes,"cache file exceeds 128 MiB");
 const std::string payload(reinterpret_cast<const char*>(out.data()),out.size());const auto digest=sha256_hex(payload);
 for(std::size_t i=0;i<digest.size();i+=2)put_u8(out,static_cast<std::uint8_t>(std::stoul(digest.substr(i,2),nullptr,16)));
 return out;
}
std::string hex_digest(const Bytes& in,std::size_t at){static constexpr char digits[]="0123456789abcdef";std::string result;result.reserve(64);for(std::size_t i=0;i<32;++i){auto v=in[at+i];result.push_back(digits[v>>4]);result.push_back(digits[v&15]);}return result;}
Ephemeris decode(const Bytes& data,const Snapshot& s,const Settings& q){need(data.size()>=8+4+32&&data.size()<=max_file_bytes,"cache file length invalid");
 need(std::equal(magic.begin(),magic.end(),data.begin()),"cache magic invalid");Reader r{data,8,data.size()-32};need(r.u32()==1,"cache version unsupported");
 Ephemeris e;e.metadata=get_metadata(r);need(cache_compatible(e.metadata,s,q),"cache metadata incompatible with caller");
 auto bodies=r.u32();auto samples=r.u64();need(bodies==s.bodies.size()&&bodies>=2&&bodies<=max_bodies,"cache body count mismatch");
 need(samples==sample_count(q)&&samples>=2&&samples-1<=q.max_steps,"cache sample count or max_steps mismatch");
 need(samples<=r.limit/(bodies*48ull),"cache sample bytes exceed file bound");
 e.body_ids.reserve(bodies);e.samples.resize(bodies);
 for(std::size_t i=0;i<bodies;++i){auto id=r.str();const auto& b=s.bodies[i];need(id==b.id,"cache body order mismatch");
  need(r.real()==b.mu_m3_s2&&r.real()==b.radius_m&&r.real()==*b.epoch_ut_s&&same(r.state(),*b.state),"cache body descriptor differs from snapshot");e.body_ids.push_back(std::move(id));}
 for(std::size_t i=0;i<bodies;++i){auto& row=e.samples[i];row.reserve(static_cast<std::size_t>(samples));for(std::size_t j=0;j<samples;++j)row.push_back(r.state());need(same(row.front(),*s.bodies[i].state),"cache first sample differs from snapshot");}
 need(r.at==r.limit,"cache exact payload length mismatch");
 const std::string payload(reinterpret_cast<const char*>(data.data()),r.limit);need(sha256_hex(payload)==hex_digest(data,r.limit),"cache checksum mismatch");
 return e;
}
std::atomic<std::uint64_t> sequence{0};
std::filesystem::path temporary_path(const std::filesystem::path& path){auto temporary=path;temporary+=std::filesystem::path(".tmp."+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"."+std::to_string(sequence.fetch_add(1)));return temporary;}
void write_exclusive(const std::filesystem::path& path,const Bytes& data){
#ifdef _WIN32
 HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);need(h!=INVALID_HANDLE_VALUE,"temporary cache creation failed");
 bool ok=true;std::size_t at=0;while(at<data.size()){DWORD done=0;DWORD count=static_cast<DWORD>(std::min<std::size_t>(data.size()-at,1<<20));if(!WriteFile(h,data.data()+at,count,&done,nullptr)||done==0){ok=false;break;}at+=done;}
 if(ok)ok=FlushFileBuffers(h)!=0;CloseHandle(h);need(ok,"temporary cache write failed");
#else
 int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);need(fd>=0,"temporary cache creation failed");bool ok=true;std::size_t at=0;
 while(at<data.size()){ssize_t count=write(fd,data.data()+at,data.size()-at);if(count<=0){ok=false;break;}at+=static_cast<std::size_t>(count);}
 if(ok)ok=fsync(fd)==0;if(close(fd)!=0)ok=false;need(ok,"temporary cache write failed");
#endif
}
void publish(const std::filesystem::path& temp,const std::filesystem::path& destination){
#ifdef _WIN32
 need(MoveFileExW(temp.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH)!=0,"cache destination exists or publish failed");
#else
 need(link(temp.c_str(),destination.c_str())==0,"cache destination exists or publish failed");
 need(unlink(temp.c_str())==0,"temporary cache cleanup failed");
#endif
}
}
void save_ephemeris_cache(const Ephemeris& e,const Snapshot& s,const Settings& q,const std::filesystem::path& path){
 validate_input(s,q);validate_ephemeris(e,s,q);need(!path.empty(),"cache path missing");const auto data=encode(e,s,q);auto temp=temporary_path(path);
 try{write_exclusive(temp,data);publish(temp,path);}catch(...){std::error_code ignored;std::filesystem::remove(temp,ignored);throw;}
}
Ephemeris load_ephemeris_cache(const std::filesystem::path& path,const Snapshot& s,const Settings& q){
 validate_input(s,q);need(!path.empty(),"cache path missing");std::ifstream file(path,std::ios::binary|std::ios::ate);need(static_cast<bool>(file),"cache open failed");
 const auto length=file.tellg();need(length>=0&&length<=static_cast<std::streamoff>(max_file_bytes),"cache file exceeds 128 MiB");
 Bytes data(static_cast<std::size_t>(length));file.seekg(0);need(static_cast<bool>(file.read(reinterpret_cast<char*>(data.data()),length)),"cache read failed");
 file.clear();file.seekg(0,std::ios::end);need(static_cast<bool>(file)&&file.tellg()==length,"cache file length changed during read");
 return decode(data,s,q);
}
}
