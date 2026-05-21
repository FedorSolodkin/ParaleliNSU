cmake_minimum_required(VERSION 3.14)
project(Task2 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -fsanitize=thread")

find_package(Threads REQUIRED)

# ── Основная программа ───────────────────────────────────────────────────────
add_executable(main main.cpp)
target_link_libraries(main Threads::Threads)
target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ── Тест: проверка результатов из файлов ─────────────────────────────────────
add_executable(test_results test.cpp)
target_link_libraries(test_results Threads::Threads)

# ── Бенчмарк: выбор контейнера (map vs unordered_map) ────────────────────────
add_executable(benchmark_container benchmark_container.cpp)

# ── Бенчмарк: выбор синхронизации (cv vs polling) ────────────────────────────
add_executable(benchmark_sync benchmark_sync.cpp)
target_link_libraries(benchmark_sync Threads::Threads)
