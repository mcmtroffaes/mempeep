add_library(mempeep INTERFACE)
target_include_directories(mempeep INTERFACE include)
target_compile_features(mempeep INTERFACE cxx_std_23)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
  add_subdirectory(test)
endif()