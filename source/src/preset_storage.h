#pragma once
#include "preset_controls.h"
#include <cstddef>
#include <cstring>
namespace legio_nam {
struct PresetRecord {
  std::uint32_t magic=0x50534e4c,signature=0x5947524d,version=1,generation=0;
  PresetBank bank{};
  std::uint32_t checksum=0;
};
static_assert(sizeof(PresetRecord)==60);
inline std::uint32_t PresetChecksum(const PresetRecord& r) {
  const auto* b=reinterpret_cast<const unsigned char*>(&r);
  std::uint32_t h=2166136261u;
  for(unsigned i=0;i<offsetof(PresetRecord,checksum);++i) h=(h^b[i])*16777619u;
  return h;
}
inline bool ValidRecord(const PresetRecord& r) {
  if(r.magic!=0x50534e4c||r.signature!=0x5947524d||r.version!=1
     ||r.bank.valid>7||r.checksum!=PresetChecksum(r)) return false;
  for(unsigned i=0;i<3;++i) if(!ValidPreset(r.bank.slots[i])) return false;
  return true;
}
// Two independent 4 KiB sectors. The previous valid record is never erased
// before the new record passes readback/checksum. Unknown contents are refused.
// Backend reads must invalidate mapped cache after writes; calls run main-only.
template<class Backend> class PresetStorage {
 public:
  explicit PresetStorage(Backend& backend):backend_(backend) {}
  bool Load(PresetBank& bank) {
    current_=-1;generation_=0;
    for(unsigned i=0;i<2;++i) {
      PresetRecord r;backend_.Read(i,r);
      if(ValidRecord(r)&&(current_<0||std::int32_t(r.generation-generation_)>0)) {
        current_=int(i);generation_=r.generation;bank=r.bank;
      }
    }
    return current_>=0;
  }
  bool Save(const PresetBank& bank) {
    PresetBank ignored;Load(ignored); // Revalidate ownership before any erase.
    PresetRecord next;next.generation=generation_+1;next.bank=bank;
    next.checksum=PresetChecksum(next);
    if(!ValidRecord(next)) return false;
    if(current_>=0) {
      PresetRecord current;backend_.Read(unsigned(current_),current);
      if(ValidRecord(current)&&std::memcmp(&bank,&current.bank,sizeof(bank))==0) return true;
    }
    unsigned target=current_==0?1:0;
    if(current_<0&&!backend_.Blank(0)&&backend_.Blank(1)) target=1;
    PresetRecord old;backend_.Read(target,old);
    const bool blank=backend_.Blank(target);
    if(!blank) {
      // A torn record with our full signature is reclaimable only while a
      // separate valid copy survives. Arbitrary/unknown contents stay protected.
      const bool owned=ValidRecord(old)||(current_>=0&&old.magic==0x50534e4c
                         &&old.signature==0x5947524d&&old.version==1);
      if(!owned||!backend_.TailBlank(target,sizeof(old))
         ||!backend_.Erase(target)) return false;
    }
    if(!backend_.Write(target,next)) return false;
    PresetRecord check;backend_.Read(target,check);
    if(std::memcmp(&next,&check,sizeof(next))!=0||!ValidRecord(check)) return false;
    current_=int(target);generation_=next.generation;return true;
  }
 private:
  Backend& backend_;int current_=-1;std::uint32_t generation_=0;
};
}
