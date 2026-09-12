#include <array>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <random>

// Host mirror of the ESP-IDF API. The benchmark is deterministic and intentionally
// reports only host evidence; it must not be read as ESP32-S3 timing.
struct Engram {
  struct Slot { std::array<int8_t, 16> key{}; uint8_t successor=0, confidence=0, age=0; bool used=false; };
  std::array<Slot, 32> slots{};
  static uint8_t hash(const std::array<int8_t,16>& k) { uint32_t h=2166136261u; for(auto v:k){h^=(uint8_t)v;h*=16777619u;} return h%32; }
  bool lookup(const std::array<int8_t,16>& key, uint8_t& s, uint8_t& c) const { auto start=hash(key); for(uint8_t p=0;p<4;++p){const auto& x=slots[(start+p)%32]; if(!x.used) return false; if(x.key==key){s=x.successor;c=x.confidence;return true;}} return false; }
  void update(const std::array<int8_t,16>& key, uint8_t s) { auto start=hash(key); Slot* victim=nullptr; for(uint8_t p=0;p<4;++p){auto& x=slots[(start+p)%32]; if(!x.used||x.key==key){victim=&x;break;} if(!victim||x.age>victim->age)victim=&x;} victim->key=key; if(victim->used&&victim->successor==s&&victim->confidence<255)++victim->confidence; else {victim->successor=s;victim->confidence=1;} victim->age=0; victim->used=true; for(auto& x:slots)if(x.used&&x.age<255)++x.age; }
};

static std::array<int8_t, 16> quantize(const std::array<int8_t, 16>& input) {
  std::array<int8_t, 16> output{};
  for (size_t i = 0; i < output.size(); ++i) output[i] = static_cast<int8_t>(input[i] / 16);
  return output;
}

int main() {
  std::mt19937 rng(108); std::uniform_int_distribution<int> noise(-7,7);
  std::array<std::array<int8_t,16>,32> bank{}; for(size_t s=0;s<bank.size();++s)for(size_t d=0;d<16;++d)bank[s][d]=(int8_t)((s*11+d*3)%127-63);
  Engram memory; size_t raw_correct=0, aided_correct=0, used=0, harmed=0; uint8_t previous=0; bool has_previous=false;
  for(size_t i=0;i<100000;++i){ uint8_t truth=(uint8_t)((i/5+1)%32); auto q=bank[truth]; for(auto& v:q)v=(int8_t)(v+noise(rng)); int raw=0,best=1<<30; for(int s=0;s<32;++s){int dist=0;for(int d=0;d<16;++d)dist+=std::abs((int)q[d]-bank[s][d]);if(dist<best){best=dist;raw=s;}} auto key=quantize(q); int aided=raw; uint8_t successor=0,confidence=0; if(memory.lookup(key,successor,confidence)&&confidence>=2&&successor<32){aided=successor;++used;} if(raw==(int)truth)++raw_correct; if(aided==(int)truth)++aided_correct; if(aided!=raw&&aided!=(int)truth&&raw==(int)truth)++harmed; memory.update(key,truth); previous=aided;has_previous=true; (void)previous;(void)has_previous; }
  std::cout << "v1.08 engram-assisted benchmark\n" << "samples=100000\n" << "raw_accuracy=" << (100.0*raw_correct/100000.0) << "\n" << "aided_accuracy=" << (100.0*aided_correct/100000.0) << "\n" << "engram_activation_pct=" << (100.0*used/100000.0) << "\n" << "harmed_queries=" << harmed << "\n" << "BOARD_PASS=NO (no physical serial evidence)\n";
}
