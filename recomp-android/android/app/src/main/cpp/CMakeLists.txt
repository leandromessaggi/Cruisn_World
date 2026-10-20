cmake_minimum_required(VERSION 3.18.1)
project(cruisn_world_port_boot LANGUAGES CXX)
add_library(cw_boot SHARED boot_probe.cpp)
target_compile_features(cw_boot PRIVATE cxx_std_17)
target_compile_options(cw_boot PRIVATE -Wall -Wextra -Werror -fvisibility=hidden)

# 16 KiB ELF alignment is required on newer Android devices.
target_link_options(cw_boot PRIVATE -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384)
