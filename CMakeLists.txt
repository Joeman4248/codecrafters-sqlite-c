cmake_minimum_required(VERSION 3.13)

project(codecrafters-sqlite)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# CONFIGURE_DEPENDS re-checks the file list on every build, so files added,
# removed or brought in by switching branches are picked up without a reconfigure
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*.c src/*.h)

set(CMAKE_C_STANDARD 23) # Enable the C23 standard

add_executable(sqlite ${SOURCE_FILES})

# includes name their folder, as in "storage/record.h"
target_include_directories(sqlite PRIVATE src)
