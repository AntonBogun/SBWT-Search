# Builds items which are commonly used between the main program and the tests.
# Usually these are classes, files and options which are used by the main program but
# are also tested individually

# Any common options are put as an interface
# rather than putting it with each file individually

add_library(common_options INTERFACE)
target_compile_options(
  common_options
  INTERFACE "$<$<CONFIG:Debug>:--coverage>" # only in debug mode
)
target_compile_options(
  common_options
  INTERFACE "$<$<CONFIG:Release>:-O3>" # only in release mode
)
target_link_libraries(common_options INTERFACE gcov)


include(ExternalProject)
# QueryFileParser Library
## Fetch kseqpp
ExternalProject_Add(
  kseqpp
  GIT_REPOSITORY https://github.com/cartoonist/kseqpp
  GIT_TAG        v0.2.1
  PREFIX         "${CMAKE_BINARY_DIR}/external/kseqpp"
  CMAKE_ARGS
		-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
)
## Fetch ZLIB
find_package(ZLIB)
add_library(
  query_file_parser
  "${PROJECT_SOURCE_DIR}/QueryFileParser/QueryFileParser.cpp"
)
## The library itself
target_link_libraries(
  query_file_parser
  PRIVATE
  common_options
  ZLIB::ZLIB
)
target_include_directories(
  query_file_parser
  PUBLIC "${PROJECT_SOURCE_DIR}/QueryFileParser"
  PUBLIC "${PROJECT_SOURCE_DIR}/Utils"
  PRIVATE SYSTEM "${CMAKE_BINARY_DIR}/external/kseqpp/include"
)
add_dependencies(query_file_parser kseqpp)

# RawSequenceParser Library
add_library(
  raw_sequences_parser
  INTERFACE
)
target_link_libraries(
  raw_sequences_parser
  INTERFACE common_options
)
target_include_directories(
  raw_sequences_parser
  INTERFACE "${PROJECT_SOURCE_DIR}/RawSequencesParser"
  INTERFACE "${PROJECT_SOURCE_DIR}/Utils"
)

# IndexFileParser library
## Get sdsl dependency
include(ExternalProject)
ExternalProject_Add(
  sdsl
  GIT_REPOSITORY  https://github.com/simongog/sdsl-lite/
  GIT_TAG         v2.1.1
  PREFIX          "${CMAKE_BINARY_DIR}/external/sdsl"
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
)
add_library(libsdsl SHARED IMPORTED)
set_target_properties(libsdsl PROPERTIES IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/external/sdsl/lib/libsdsl.a")
ExternalProject_Get_Property(sdsl SOURCE_DIR)
set (sdsl_SOURCE_DIR "${SOURCE_DIR}")
## The library itself
add_library(
  index_file_parser
  "${PROJECT_SOURCE_DIR}/IndexFileParser/IndexFileParser.cpp"
)
target_link_libraries(
  index_file_parser
  PRIVATE common_options
  PRIVATE libsdsl
)
target_include_directories(
  index_file_parser
  PUBLIC "${PROJECT_SOURCE_DIR}/IndexFileParser"
  PRIVATE SYSTEM "${sdsl_SOURCE_DIR}/include"
)
add_dependencies(index_file_parser sdsl)

# TODO: Add more libraries here

# Common libraries
add_library(common_libraries INTERFACE)
target_link_libraries(
  common_libraries
  INTERFACE query_file_parser
  INTERFACE raw_sequences_parser
  INTERFACE index_file_parser
  # TODO: Link more libraries here
)

# Build Cpu Libraries
if (BUILD_CPU)
  # Combine Libaries
  add_library(libraries_cpu INTERFACE)
  target_link_libraries(
    libraries_cpu
    INTERFACE common_options
    INTERFACE common_libraries
    # TODO: Combine more libraries here which are cpu specific
  )
endif()

# Build CUDA Libraries
if (CMAKE_CUDA_COMPILER AND BUILD_CUDA)
  # Combine Libaries
  add_library(libraries_cuda INTERFACE)
  target_link_libraries(
    libraries_cuda
    INTERFACE common_options
    INTERFACE common_libraries
    # TODO: Combine more libraries here which are cuda specific
  )
endif()
