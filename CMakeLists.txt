cmake_minimum_required(VERSION 3.23 FATAL_ERROR)
project(epos DESCRIPTION "EPOS" VERSION 0.1.0 LANGUAGES CXX)

# Third Party
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
list(PREPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# Dependencies
find_package(Boost REQUIRED COMPONENTS headers)
find_package(TBB REQUIRED COMPONENTS tbb)
find_package(deus REQUIRED)

foreach(dir ${TBB_LIB_DIRS})
  get_filename_component(dir ${dir} DIRECTORY)
  foreach(lib ${TBB_LIBRARY_LIST})
    if(EXISTS ${dir}/bin/${lib}.dll)
      if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${lib}.dll)
        file(COPY ${dir}/bin/${lib}.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
      endif()
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${lib}.dll DESTINATION bin)
    endif()
  endforeach()
endforeach()

# Executable
file(GLOB_RECURSE headers src/*.hpp)
file(GLOB_RECURSE sources src/*.cpp)

add_executable(main WIN32 ${headers} ${sources} src/main.manifest src/main.rc)
set_target_properties(main PROPERTIES OUTPUT_NAME epos)
target_link_libraries(main PRIVATE Boost::headers TBB::tbb deus::deus)
target_link_libraries(main PRIVATE comctl32 d2d1 d3d11 dcomp dinput8 dwrite dxgi dxguid)
target_compile_definitions(main PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX DIRECTINPUT_VERSION=0x0800)
target_compile_definitions(main PRIVATE EPOS_USE_AVX2)
target_compile_options(main PRIVATE /arch:AVX2)
target_include_directories(main PRIVATE src)
