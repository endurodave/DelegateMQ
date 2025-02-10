# Example CMake command line to create project build files:
#
# *** Windows ***
# cmake -G "Visual Studio 17 2022" -A Win32 -B build -S .
# cmake -G "Visual Studio 17 2022" -A x64 -B build -S .
#
# *** Linux ***
# cmake -G "Unix Makefiles" -B build -S .
# cmake -G "Unix Makefiles" -B build -S . -DTEST_COVERAGE=ON

# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Project name and language (C or C++)
project(DelegateProject VERSION 1.0 LANGUAGES CXX)

# Add subdirectory for building delegate_app
add_subdirectory(test ${CMAKE_BINARY_DIR}/delegate_app)
