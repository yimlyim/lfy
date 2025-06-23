cmake_minimum_required(VERSION 3.16)
project(Log4Yum VERSION 0.1.0 LANGUAGES CXX)

# Debug vs Release specific flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Include directories
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*.cpp"  # For subdirectories if needed
)

# Create the library (can be STATIC, SHARED, or OBJECT)
add_library(Log4Yum STATIC ${PROJECT_SOURCES})

# Set properties
set_target_properties(Log4Yum PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)