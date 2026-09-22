#include "RuntimeReader.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace ksp {
namespace {
constexpr std::array<std::uint32_t,64> round_constants={
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
constexpr std::array<std::uint32_t,8> initial_state={
    0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
std::uint32_t right(std::uint32_t value,unsigned count){return (value>>count)|(value<<(32-count));}
}
std::string sha256_hex(const std::string& bytes){
    if(bytes.size()>std::numeric_limits<std::uint64_t>::max()/8)
        throw RuntimeReaderError("snapshot exceeds SHA-256 length limit");
    const std::uint64_t bit_length=static_cast<std::uint64_t>(bytes.size())*8;
    std::vector<std::uint8_t> padded(bytes.begin(),bytes.end());
    padded.push_back(0x80);
    while(padded.size()%64!=56)padded.push_back(0);
    for(int shift=56;shift>=0;shift-=8)padded.push_back(static_cast<std::uint8_t>(bit_length>>shift));
    auto h=initial_state;
    for(std::size_t offset=0;offset<padded.size();offset+=64){
        std::array<std::uint32_t,64> words{};
        for(std::size_t i=0;i<16;++i){
            const std::size_t at=offset+i*4;
            words[i]=(std::uint32_t(padded[at])<<24)|(std::uint32_t(padded[at+1])<<16)|
                     (std::uint32_t(padded[at+2])<<8)|std::uint32_t(padded[at+3]);
        }
        for(std::size_t i=16;i<64;++i){
            const auto x=words[i-15],y=words[i-2];
            const auto small0=right(x,7)^right(x,18)^(x>>3);
            const auto small1=right(y,17)^right(y,19)^(y>>10);
            words[i]=words[i-16]+small0+words[i-7]+small1;
        }
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7];
        for(std::size_t i=0;i<64;++i){
            const auto big1=right(e,6)^right(e,11)^right(e,25);
            const auto choose=(e&f)^(~e&g);
            const auto t1=z+big1+choose+round_constants[i]+words[i];
            const auto big0=right(a,2)^right(a,13)^right(a,22);
            const auto majority=(a&b)^(a&c)^(b&c);
            const auto t2=big0+majority;
            z=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
    }
    constexpr char hex[]="0123456789abcdef";
    std::string result;result.reserve(64);
    for(const auto word:h)for(int shift=28;shift>=0;shift-=4)result.push_back(hex[(word>>shift)&0xf]);
    return result;
}
}
