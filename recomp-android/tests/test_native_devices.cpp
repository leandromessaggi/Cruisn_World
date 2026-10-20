#include "../android/app/src/main/cpp/recomp_devices.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
int main(){
  using namespace cw;
  GuestClock time;
  time.tickVi();
  assert(time.viCount()==1&&time.totalCount()==782031);
  for(int i=1;i<600;++i)time.tickVi();
  assert(time.totalCount()==469218750);
  GuestMessageQueue queue(2);
  std::uint32_t msg=0;
  assert(!queue.receive(&msg)&&queue.send(0xAA)&&queue.send(0xBB));
  assert(!queue.send(0xCC)&&queue.receive(&msg)&&msg==0xAA);
  assert(queue.send(0xCC)&&queue.receive(&msg)&&msg==0xBB);
  assert(queue.receive(&msg)&&msg==0xCC&&!queue.receive(nullptr));
  GuestMessageQueue zero(0);assert(!zero.send(1));
  ControllerSample input;
  input.buttons=kButtonA|kButtonZ|kButtonStart|kDpadLeft;
  input.stick_x=-54;input.stick_y=72;
  assert((input.serialize()==std::array<std::uint8_t,4>{{0xB2,0x00,0xCA,0x48}}));
  Eeprom16k save;
  std::array<std::uint8_t,8> block{},got{};
  assert(save.readBlock(0,got.data(),8)&&got[0]==0xFF);
  assert(!save.readBlock(256,got.data(),8)&&!save.writeBlock(0,nullptr,8));
  const std::string path="cw_ep16k_test.eep";
  std::remove(path.c_str());
  assert(save.load(path));
  assert(std::ifstream(path,std::ios::binary|std::ios::ate).tellg()==2048);
  for(unsigned i=0;i<8;++i)block[i]=static_cast<std::uint8_t>(i*11);
  assert(save.writeBlock(255,block.data(),8)&&save.save());
  Eeprom16k reloaded;
  assert(reloaded.load(path)&&reloaded.readBlock(255,got.data(),8)&&got==block);
  {std::ofstream f(path,std::ios::binary|std::ios::trunc);f.put('x');}
  assert(!reloaded.load(path));
  std::remove(path.c_str());
  std::cout<<"PASS: COP0 clock, bounded VI FIFO, N64 controls, persisted 16 Kbit EEPROM\n";
}