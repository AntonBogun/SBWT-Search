# This is the root CMakeLists which calls all other cmake files. It is used by
# the cmake tool to build the Makefile which would then be used to compile our
# program. Inside it, we specify the project name and details, we load all
# files within the cmake folder and then add the src and documentation folder,
# which also contain a CMakeLists.txt file, as subdirectories, so that the
# CMakeLists.txt file within them is also executed (sequentially).  Since we
# will be using CUDA and HIP, we want to use a pretty high version of CMake.

cmake_minimum_required(VERSION 3.20)

project(
  "SBWT Search"
  VERSION 0.0.1
  DESCRIPTION "An application to search for k-mers in a genome given an SBWT index"
  HOMEPAGE_URL https://github.com/CowKeyMan/SBWT-Search
  LANGUAGES CXX
)
set (CMAKE_CXX_STANDARD 20)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(cmake/PreventBuildingInCmakeDirectory.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Profiling.cmake)
include(cmake/Options.cmake)
include(cmake/CCache.cmake)
include(cmake/CCache.cmake)
include(cmake/SetHipTargetDevice.cmake)

option(
  BUILD_DOCS
  "Build the documentations"
  ON
)

add_subdirectory(src)
if (BUILD_DOCS)
  add_subdirectory (documentation)
endif()
