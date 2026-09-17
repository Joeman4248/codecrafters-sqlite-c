cmake_minimum_required(VERSION 3.13)

project(codecrafters-sqlite)

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h lib/*.c lib/*.h)

set(CMAKE_C_STANDARD 23) # Enable the C23 standard

add_executable(sqlite ${SOURCE_FILES})

target_include_directories(sqlite PRIVATE lib/include)
