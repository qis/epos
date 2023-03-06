cmake_minimum_required(VERSION 3.23 FATAL_ERROR)
project(epos DESCRIPTION "EPOS" VERSION 0.1.0 LANGUAGES CXX)

# Conan
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
list(PREPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# Dependencies
find_package(Boost REQUIRED COMPONENTS headers)
find_package(TBB REQUIRED COMPONENTS tbb)
find_package(deus REQUIRED)

# Executable
add_executable(main WIN32 src/main.cpp src/main.manifest src/main.rc)
set_target_properties(main PROPERTIES OUTPUT_NAME epos)
target_link_libraries(main PRIVATE deus::deus)
