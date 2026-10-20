#include "recomp_devices.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
namespace cw {
void GuestClock::tickVi() {
  ++frames_;
  // Fixed point by rational division: 46.875 MHz at 60000/1001 NTSC VI/s.
  count_=frames_*kCountHz*kViRateDenominator/kViRateNumerator;
}
GuestMessageQueue::GuestMessageQueue(std::size_t capacity):capacity_(capacity){}
bool GuestMessageQueue::send(std::uint32_t message) {
  if(messages_.size()>=capacity_)return false;
  messages_.push_back(message);
  return true;
}
bool GuestMessageQueue::receive(std::uint32_t* message) {
  if(!message||messages_.empty())return false;
  *message=messages_.front();messages_.pop_front();return true;
}
std::array<std::uint8_t,4> ControllerSample::serialize() const {
  return {{static_cast<std::uint8_t>(buttons>>8),
           static_cast<std::uint8_t>(buttons),
           static_cast<std::uint8_t>(stick_x),
           static_cast<std::uint8_t>(stick_y)}};
}
Eeprom16k::Eeprom16k(){data_.fill(0xFF);}
bool Eeprom16k::load(const std::string& private_path,std::string* error) {
  if(private_path.empty()){if(error)*error="EEPROM path is empty";return false;}
  std::ifstream input(private_path,std::ios::binary|std::ios::ate);
  if(!input) {
    data_.fill(0xFF);
    path_=private_path;
    return save(error);
  }
  if(input.tellg()!=static_cast<std::streampos>(kTotalBytes)){
    if(error)*error="EEPROM file is not 2048 bytes";
    return false;
  }
  std::array<std::uint8_t,kTotalBytes> next{};
  input.seekg(0);
  input.read(reinterpret_cast<char*>(next.data()),
             static_cast<std::streamsize>(next.size()));
  if(!input){if(error)*error="EEPROM read failed";return false;}
  data_=next;path_=private_path;return true;
}
bool Eeprom16k::save(std::string* error) const {
  if(path_.empty()){if(error)*error="EEPROM not loaded";return false;}
  const std::string temporary=path_+".tmp";
  {
    std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
    if(!out){if(error)*error="EEPROM write denied";return false;}
    out.write(reinterpret_cast<const char*>(data_.data()),
              static_cast<std::streamsize>(data_.size()));
    out.flush();
    if(!out.good()){if(error)*error="EEPROM write failed";std::remove(temporary.c_str());return false;}
  }
  if(std::rename(temporary.c_str(),path_.c_str())!=0){
    if(error)*error="EEPROM rename failed";
    std::remove(temporary.c_str());return false;
  }
  return true;
}
bool Eeprom16k::readBlock(unsigned index,std::uint8_t* dest,std::size_t length) const {
  if(index>=kBlocks||!dest||length!=kBlockSize)return false;
  std::copy_n(data_.data()+index*kBlockSize,kBlockSize,dest);return true;
}
bool Eeprom16k::writeBlock(unsigned index,const std::uint8_t* src,std::size_t length) {
  if(index>=kBlocks||!src||length!=kBlockSize)return false;
  std::copy_n(src,kBlockSize,data_.data()+index*kBlockSize);return true;
}
}