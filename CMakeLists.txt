cmake_minimum_required(VERSION 3.16)
project(lfy VERSION 0.1.0 LANGUAGES CXX)

set(PROJECT_NAME lfy)

add_library(lfy INTERFACE)

target_include_directories(lfy INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Set properties
set_target_properties(lfy PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/*.hpp"
)

# Dummy executable that links against lfy, for testing purposes
add_executable(lfy_dummy_exe src/dummy.cpp)
target_link_libraries(lfy_dummy_exe PRIVATE lfy)
target_include_directories(lfy_dummy_exe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(lfy_dummy_exe PROPERTIES EXCLUDE_FROM_ALL TRUE EXCLUDE_FROM_DEFAULT_BUILD TRUE)