cmake_minimum_required(VERSION 3.30)
project("mempeep")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  option(BUILD_TESTING "Build tests" ON)
  include(CTest)
endif()

add_subdirectory(cpp)