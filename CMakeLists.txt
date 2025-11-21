cmake_minimum_required(VERSION 3.16)

project(elfs-and-wizards)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

# Header
include_directories(${CMAKE_SOURCE_DIR}/include)

# Raylib Libs
link_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(elfs-and-wizards
    src/main.cpp
)

if(UNIX)
  target_link_libraries(elfs-and-wizards
        raylib
        m
        pthread
        dl
        GL
    )
endif()
