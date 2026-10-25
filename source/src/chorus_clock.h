#pragma once
#include <algorithm>
#include <cstdint>
namespace legio_nam {
// 1 kHz control-tick clock follower: eight input pulses per LFO cycle. Rate
// follows clock tempo; LFO phase remains continuous to avoid delay-tap jumps.
class ChorusClock {
 public:
  explicit ChorusClock(std::uint32_t ticks=0):ticks_(ticks) {}
  void Tick(bool high) {
    ++ticks_;
    if(high!=candidate_) {candidate_=high;debounce_=0;}
    if(debounce_<3)++debounce_;
    if(debounce_==3&&stable_!=candidate_) {
      stable_=candidate_;
      if(stable_) Edge();
    }
    const auto timeout=std::max<std::uint32_t>(500u,3u*period_);
    if(have_edge_&&std::uint32_t(ticks_-last_edge_)>timeout) {
      have_edge_=false;locked_=false;matches_=0;period_=0;
    }
  }
  bool Locked() const {return locked_;}
  std::uint32_t PeriodMs() const {return period_;}
  std::uint32_t Edges() const {return edges_;}
  float RateHz() const {return locked_?std::clamp(125.f/float(period_),.03f,1.f):0.f;}
 private:
  void Edge() {
    ++edges_;
    if(have_edge_) {
      const auto next=std::uint32_t(ticks_-last_edge_);
      if(next>=40&&next<=5000) {
        const auto difference=next>period_?next-period_:period_-next;
        matches_=period_&&difference<=std::max<std::uint32_t>(3u,period_/4u)?std::min(matches_+1,2u):1u;
        period_=next;locked_=matches_>=2;
      } else {period_=0;matches_=0;locked_=false;}
    }
    last_edge_=ticks_;have_edge_=true;
  }
  std::uint32_t ticks_=0,last_edge_=0,period_=0,edges_=0;
  unsigned debounce_=0,matches_=0;
  bool candidate_=false,stable_=false,have_edge_=false,locked_=false;
};
}
