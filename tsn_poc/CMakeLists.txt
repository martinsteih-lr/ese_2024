# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.10)

# Project's name
project(tsn_poc)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)

# Add executable target with source files listed in SOURCE_FILES variable
set(SOURCE_FILES_RECEIVER main.cpp)
add_executable(${PROJECT_NAME} ${SOURCE_FILES_RECEIVER})

target_link_libraries(${PROJECT_NAME} rt pthread)

# Specify the install path for the executable
install(TARGETS ${PROJECT_NAME} DESTINATION bin)