cmake_minimum_required(VERSION 3.18)
project(cleo64plugins LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(AML_DIR "" CACHE PATH "AndroidModLoader checkout (contains mod/amlmod.h)")
set(CLEO_SDK_DIR "" CACHE PATH "directory containing cleo.h")

# AML ships logger and config as source files that every mod compiles in.
file(GLOB AML_SRC "${AML_DIR}/mod/logger.cpp" "${AML_DIR}/mod/config.cpp")
message(STATUS "AML sources: ${AML_SRC}")

function(make_plugin target)
  add_library(${target} SHARED main.cpp peds.cpp ${AML_SRC} ${ARGN})
  target_include_directories(${target} PRIVATE ${AML_DIR} ${CLEO_SDK_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_options(${target} PRIVATE -O2 -Wno-unused-function -Wno-unused-parameter)
  target_link_libraries(${target} log dl)
endfunction()

make_plugin(CLEO_VisualObject visual.cpp)
target_compile_definitions(CLEO_VisualObject PRIVATE BUILD_VISUALOBJECT)
make_plugin(CLEOPlus plus.cpp)
