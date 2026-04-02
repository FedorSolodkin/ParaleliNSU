cmake_minimum_required(VERSION 3.10)
project(SLAU_Iteration LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)

add_executable(sla_solve main.cpp)
target_link_libraries(sla_solve OpenMP::OpenMP_CXX)
target_compile_options(sla_solve PRIVATE -O3 -march=native -DNDEBUG)