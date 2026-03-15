cmake_minimum_required (VERSION 3.30)
project ("mempeep")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)

add_library(mempeep INTERFACE)

target_include_directories(mempeep SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp
)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  include(CTest)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
  add_subdirectory("cpp/test")
endif()
