cmake_minimum_required(VERSION 3.16)
project(lfy VERSION 0.1.0 LANGUAGES CXX)

set(PROJECT_NAME lfy)

# Debug vs Release specific flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

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

# Dummy target to force compile_commands.json generation for clangd and IDEs
file(GLOB_RECURSE lfy_PUBLIC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/*.hpp")
set(DUMMY_CPP "${CMAKE_CURRENT_SOURCE_DIR}/src/dummy.cpp")
add_library(lfy_dummy OBJECT ${lfy_PUBLIC_HEADERS} ${DUMMY_CPP})
target_include_directories(lfy_dummy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(lfy_dummy PROPERTIES EXCLUDE_FROM_ALL TRUE EXCLUDE_FROM_DEFAULT_BUILD TRUE)
add_executable(lfy_dummy_exe $<TARGET_OBJECTS:lfy_dummy>)

# Add RapidJSON as an interface target (header-only)
add_library(RapidJSON INTERFACE)
target_include_directories(RapidJSON INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rapidjson/include
)

# Example: link RapidJSON to your dummy target (add to others as needed)
target_link_libraries(lfy_dummy PRIVATE RapidJSON)

# Automatically create/update copy for compile_commands.json in project root
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    set(COMPILE_COMMANDS "${CMAKE_BINARY_DIR}/compile_commands.json")
    set(COPY_TARGET "${CMAKE_SOURCE_DIR}/compile_commands.json")
    if(EXISTS "${COMPILE_COMMANDS}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E copy "${COMPILE_COMMANDS}" "${COPY_TARGET}"
            RESULT_VARIABLE result
        )
        if(result EQUAL 0)
            message(STATUS "Copied: ${COPY_TARGET} <- ${COMPILE_COMMANDS}")
        else()
            message(WARNING "Could not copy compile_commands.json (maybe insufficient permissions).")
        endif()
    endif()
endif()