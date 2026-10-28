# CMakeLists.txt for ESPHome C++ component
cmake_minimum_required(VERSION 3.10)

# Define the component library
add_library(kaku_component kaku.cpp kaku.h)

# Link against necessary ESPHome libraries
target_link_libraries(kaku_component PRIVATE esphome::core)
