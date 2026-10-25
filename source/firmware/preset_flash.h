#pragma once
#include "daisy_seed.h"
#include "preset_storage.h"
namespace legio_nam {
// Last two 4 KiB sectors of the Seed's 8 MiB external flash. Firmware/weights
// remain entirely in internal flash. No startup write or default initialization.
class PresetFlash {
 public:
  explicit PresetFlash(daisy::QSPIHandle& qspi):qspi_(qspi) {}
  bool Ready() const {
    return qspi_.GetStatus()==daisy::QSPIHandle::Status::GOOD
        &&qspi_.GetConfig().device==daisy::QSPIHandle::Config::Device::IS25LP064A
        &&qspi_.GetConfig().mode==daisy::QSPIHandle::Config::Mode::MEMORY_MAPPED;
  }
  void Read(unsigned sector,PresetRecord& record) {
    if(!Ready()) {record={};record.magic=0;return;}
    auto* data=Data(sector);std::memcpy(&record,data,sizeof(record));
  }
  bool Blank(unsigned sector) {return TailBlank(sector,0);}
  bool TailBlank(unsigned sector,unsigned begin) {
    if(!Ready()) return false;
    auto* data=Data(sector);
    for(unsigned i=begin;i<4096;++i) if(data[i]!=0xff) return false;
    return true;
  }
  bool Erase(unsigned sector) {
    if(!Ready()) return false;
    const auto offset=Offset(sector);
    return qspi_.Erase(offset,offset+4096)==daisy::QSPIHandle::Result::OK;
  }
  bool Write(unsigned sector,const PresetRecord& record) {
    if(!Ready()) return false;
    // One aligned page. Unlike Write(), WritePage propagates programming errors.
    auto* bytes=reinterpret_cast<std::uint8_t*>(const_cast<PresetRecord*>(&record));
    return qspi_.WritePage(Offset(sector),sizeof(record),bytes)==daisy::QSPIHandle::Result::OK;
  }
 private:
  static std::uint32_t Offset(unsigned sector) {return 0x7fe000u+sector*4096u;}
  std::uint8_t* Data(unsigned sector) {
    auto* data=static_cast<std::uint8_t*>(qspi_.GetData(Offset(sector)));
    // Aligned address/length; never serve stale pre-write memory-mapped data.
    dsy_dma_invalidate_cache_for_buffer(data,4096);return data;
  }
  daisy::QSPIHandle& qspi_;
};
}
