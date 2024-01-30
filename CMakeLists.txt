cmake_minimum_required(VERSION 3.10)
enable_language(CXX)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Submodules/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")

project(Beeftext)

add_subdirectory(Submodules/XMiLib/XMiLib)
add_subdirectory(Beeftext)
