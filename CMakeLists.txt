cmake_minimum_required(VERSION 3.20)
project(lob_sim CXX)
set(CMAKE_CXX_STANDARD 20)

add_executable(lob_sim
  src/main.cpp 
  src/order_book.cpp 
  src/matching_engine.cpp 
  src/strategies.cpp
  src/sim.cpp 
  src/metrics.cpp)
  
target_include_directories(lob_sim PRIVATE include)
add_compile_options(-Wall -Wextra -O3)
