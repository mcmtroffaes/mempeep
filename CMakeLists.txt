cmake_minimum_required(VERSION 3.30)
project("mempeep")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  option(BUILD_TESTING "Build tests" ON)
  include(CTest)

  option(ENABLE_COVERAGE "Enable coverage instrumentation" OFF)
  if(ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      message(FATAL_ERROR "Coverage requires Clang")
    endif()
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
  endif()
endif()

add_subdirectory(cpp)