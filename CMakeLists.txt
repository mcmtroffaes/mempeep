cmake_minimum_required(VERSION 3.30)
project("mempeep")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  include(CTest)
endif()

add_subdirectory(cpp)