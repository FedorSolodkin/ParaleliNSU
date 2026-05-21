cmake_minimum_required(VERSION 3.14)
project(Task1 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# -O2: оптимизации компилятора; без -march=native чтобы бинарь переносился
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

find_package(Threads REQUIRED)

add_executable(matvec matvec.cpp)
target_link_libraries(matvec Threads::Threads)
