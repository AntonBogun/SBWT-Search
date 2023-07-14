# Builds items which are commonly used between the main program and the tests.
# Usually these are classes, files and options which are used by the main program but
# are also tested individually.
# Any common options are put as an interface
# rather than putting it with each file individually

include_directories("${PROJECT_SOURCE_DIR}")

# External Dependencies
include(ExternalProject)
include(FetchContent)

## Fetch kseqpp_read
FetchContent_Declare(
  reklibpp
  QUIET
  GIT_REPOSITORY       "https://github.com/CowKeyMan/kseqpp_REad"
  GIT_TAG              v1.6.0
  GIT_SHALLOW          TRUE
)
FetchContent_MakeAvailable(reklibpp)


## Fetch cxxopts
FetchContent_Declare(
  cxxopts
  GIT_REPOSITORY       https://github.com/jarro2783/cxxopts
  GIT_TAG              v3.0.0
  GIT_SHALLOW          TRUE
)
set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CXXOPTS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(CXXOPTS_ENABLE_WARNINGS OFF CACHE BOOL "" FORCE)
include_directories(SYSTEM "${CMAKE_BINARY_DIR}/_deps/cxxopts-src/include")
FetchContent_MakeAvailable(cxxopts)

# Fetch OpenMP
find_package(OpenMP REQUIRED)

# Fetch sdsl
ExternalProject_Add(
  sdsl
  GIT_REPOSITORY  https://github.com/simongog/sdsl-lite/
  GIT_TAG         c32874cb2d8524119f25f3b501526fe692df29f4
  GIT_SHALLOW     TRUE
  PREFIX          "${CMAKE_BINARY_DIR}/_deps/sdsl"
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
  STEP_TARGETS build
  EXCLUDE_FROM_ALL TRUE
  LOG_CONFIGURE ON
  LOG_INSTALL ON
  LOG_BUILD ON
)
ExternalProject_Get_Property(sdsl SOURCE_DIR BINARY_DIR)
add_library(libsdsl STATIC IMPORTED GLOBAL)
add_dependencies(libsdsl sdsl)
set_target_properties(libsdsl PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/lib/libsdsl.a")
include_directories("${SOURCE_DIR}/include")

FetchContent_Declare(
  jeaiii_itoa
  GIT_REPOSITORY  https://github.com/jeaiii/itoa
  GIT_TAG         69308f65e87a9954f11f952ed04d551eabeee0ae
  GIT_SHALLOW     TRUE
)
FetchContent_MakeAvailable(jeaiii_itoa)
FetchContent_GetProperties(jeaiii_itoa)
add_library(libjeaiii_itoa INTERFACE)
target_include_directories(libjeaiii_itoa INTERFACE "${jeaiii_itoa_SOURCE_DIR}/include/")

# Index
add_library(
  argument_parser
  "${PROJECT_SOURCE_DIR}/ArgumentParser/ArgumentParser.cpp"
  "${PROJECT_SOURCE_DIR}/ArgumentParser/ColorSearchArgumentParser.cpp"
  "${PROJECT_SOURCE_DIR}/ArgumentParser/IndexSearchArgumentParser.cpp"
)
target_link_libraries(argument_parser PRIVATE cxxopts memory_units_parser)
add_library(
  presearcher_cpu
  "${PROJECT_SOURCE_DIR}/Presearcher/Presearcher.cpp"
)
target_link_libraries(presearcher_cpu PRIVATE gpu_utils)
add_library(
  presearcher_gpu
  "${PROJECT_SOURCE_DIR}/Presearcher/Presearcher.cu"
)
set_source_files_properties(
  "${PROJECT_SOURCE_DIR}/Presearcher/Presearcher.cu"
  TARGET_DIRECTORY presearcher_gpu
  PROPERTIES LANGUAGE ${HIP_TARGET_LANGUAGE}
)
target_link_libraries(presearcher_gpu PRIVATE gpu_utils)
add_library(
  sequence_file_parser
  "${PROJECT_SOURCE_DIR}/SequenceFileParser/ContinuousSequenceFileParser.cpp"
  "${PROJECT_SOURCE_DIR}/SequenceFileParser/IntervalBatchProducer.cpp"
  "${PROJECT_SOURCE_DIR}/SequenceFileParser/StringBreakBatchProducer.cpp"
  "${PROJECT_SOURCE_DIR}/SequenceFileParser/StringSequenceBatchProducer.cpp"
)
target_link_libraries(
  sequence_file_parser
  PRIVATE
  kseqpp_read
  io_utils
  error_utils
  fmt::fmt
  logger
  OpenMP::OpenMP_CXX
)
add_library(
  seq_to_bits_converter
  "${PROJECT_SOURCE_DIR}/SeqToBitsConverter/ContinuousSeqToBitsConverter.cpp"
  "${PROJECT_SOURCE_DIR}/SeqToBitsConverter/InvalidCharsProducer.cpp"
  "${PROJECT_SOURCE_DIR}/SeqToBitsConverter/BitsProducer.cpp"
)
target_link_libraries(
  seq_to_bits_converter PRIVATE fmt::fmt logger OpenMP::OpenMP_CXX
)
add_library(
  filenames_parser
  "${PROJECT_SOURCE_DIR}/FilenamesParser/FilenamesParser.cpp"
)
add_library(
  filesize_load_balancer
  "${PROJECT_SOURCE_DIR}/FilesizeLoadBalancer/FilesizeLoadBalancer.cpp"
)
add_library(
  poppy_builder
  "${PROJECT_SOURCE_DIR}/PoppyBuilder/PoppyBuilder.cpp"
)
add_library(
  sbwt_builder
  "${PROJECT_SOURCE_DIR}/SbwtBuilder/SbwtBuilder.cpp"
)
target_link_libraries(
  sbwt_builder
  PRIVATE
  io_utils
  OpenMP::OpenMP_CXX
  fmt::fmt
  gpu_utils
  libsdsl
)
add_library(
  sbwt_container
  "${PROJECT_SOURCE_DIR}/SbwtContainer/SbwtContainer.cpp"
  "${PROJECT_SOURCE_DIR}/SbwtContainer/CpuSbwtContainer.cpp"
  "${PROJECT_SOURCE_DIR}/SbwtContainer/GpuSbwtContainer.cpp"
)
target_link_libraries(sbwt_container PRIVATE gpu_utils)
add_library(
  positions_builder
  "${PROJECT_SOURCE_DIR}/PositionsBuilder/PositionsBuilder.cpp"
  "${PROJECT_SOURCE_DIR}/PositionsBuilder/ContinuousPositionsBuilder.cpp"
)
target_link_libraries(
  positions_builder PRIVATE fmt::fmt logger OpenMP::OpenMP_CXX
)
add_library(
  index_searcher_cpu
  "${PROJECT_SOURCE_DIR}/IndexSearcher/IndexSearcher.cpp"
)
target_link_libraries(index_searcher_cpu PRIVATE fmt::fmt)
add_library(
  index_searcher_gpu
  "${PROJECT_SOURCE_DIR}/IndexSearcher/IndexSearcher.cu"
)
set_source_files_properties(
  "${PROJECT_SOURCE_DIR}/IndexSearcher/IndexSearcher.cu"
  TARGET_DIRECTORY index_searcher_gpu
  PROPERTIES LANGUAGE ${HIP_TARGET_LANGUAGE}
)
target_link_libraries(index_searcher_gpu PRIVATE fmt::fmt gpu_utils)
add_library(
  index_searcher
  "${PROJECT_SOURCE_DIR}/IndexSearcher/ContinuousIndexSearcher.cpp"
)
target_link_libraries(index_searcher PRIVATE fmt::fmt index_searcher_cpu index_searcher_gpu)
add_library(
  index_results_printer
  "${PROJECT_SOURCE_DIR}/IndexResultsPrinter/AsciiContinuousIndexResultsPrinter.cpp"
  "${PROJECT_SOURCE_DIR}/IndexResultsPrinter/BinaryContinuousIndexResultsPrinter.cpp"
  "${PROJECT_SOURCE_DIR}/IndexResultsPrinter/BoolContinuousIndexResultsPrinter.cpp"
  "${PROJECT_SOURCE_DIR}/IndexResultsPrinter/PackedIntContinuousIndexResultsPrinter.cpp"
)
target_link_libraries(index_results_printer PRIVATE io_utils fmt::fmt OpenMP::OpenMP_CXX libjeaiii_itoa)

# Colors
add_library(
  index_file_parser
  "${PROJECT_SOURCE_DIR}/IndexFileParser/ContinuousIndexFileParser.cpp"

  "${PROJECT_SOURCE_DIR}/IndexFileParser/IndexFileParser.cpp"
  "${PROJECT_SOURCE_DIR}/IndexFileParser/AsciiIndexFileParser.cpp"
  "${PROJECT_SOURCE_DIR}/IndexFileParser/BinaryIndexFileParser.cpp"
  "${PROJECT_SOURCE_DIR}/IndexFileParser/PackedIntIndexFileParser.cpp"

  "${PROJECT_SOURCE_DIR}/IndexFileParser/SeqStatisticsBatchProducer.cpp"
  "${PROJECT_SOURCE_DIR}/IndexFileParser/IndexesBatchProducer.cpp"
)
target_link_libraries(index_file_parser PRIVATE io_utils fmt::fmt OpenMP::OpenMP_CXX error_utils)

add_library(
  color_index_container
  "${PROJECT_SOURCE_DIR}/ColorIndexContainer/CpuColorIndexContainer.cpp"
  "${PROJECT_SOURCE_DIR}/ColorIndexContainer/GpuColorIndexContainer.cpp"
)
target_link_libraries(color_index_container PRIVATE libsdsl)

add_library(
  color_index_builder
  "${PROJECT_SOURCE_DIR}/ColorIndexBuilder/ColorIndexBuilder.cpp"
)
target_link_libraries(color_index_builder PRIVATE libsdsl poppy_builder)

add_library(
  color_searcher_cpu
  "${PROJECT_SOURCE_DIR}/ColorSearcher/ColorSearcher.cpp"
)
target_link_libraries(color_searcher_cpu PRIVATE gpu_utils fmt::fmt libsdsl)
add_library(
  color_searcher_gpu
  "${PROJECT_SOURCE_DIR}/ColorSearcher/ColorSearcher.cu"
)
set_source_files_properties(
  "${PROJECT_SOURCE_DIR}/ColorSearcher/ColorSearcher.cu"
  TARGET_DIRECTORY color_searcher_gpu
  PROPERTIES LANGUAGE ${HIP_TARGET_LANGUAGE}
)
target_link_libraries(color_searcher_gpu PRIVATE gpu_utils fmt::fmt libsdsl)
add_library(
  color_searcher
  "${PROJECT_SOURCE_DIR}/ColorSearcher/ContinuousColorSearcher.cpp"
)
target_link_libraries(color_searcher PRIVATE fmt::fmt color_searcher_cpu color_searcher_gpu libsdsl)
add_library(
  color_results_printer
  "${PROJECT_SOURCE_DIR}/ColorResultsPrinter/AsciiContinuousColorResultsPrinter.cpp"
  "${PROJECT_SOURCE_DIR}/ColorResultsPrinter/BinaryContinuousColorResultsPrinter.cpp"
  "${PROJECT_SOURCE_DIR}/ColorResultsPrinter/CsvContinuousColorResultsPrinter.cpp"
  "${PROJECT_SOURCE_DIR}/ColorResultsPrinter/PackedIntContinuousColorResultsPrinter.cpp"
)
target_link_libraries(color_results_printer PRIVATE io_utils fmt::fmt OpenMP::OpenMP_CXX libjeaiii_itoa)

# Common libraries
add_library(common_libraries INTERFACE)
target_link_libraries(
  common_libraries
  INTERFACE
  # external libraries
  fmt::fmt
  kseqpp_read
  OpenMP::OpenMP_CXX
  spdlog::spdlog
  cxxopts
  libsdsl

  # Internal libraries
  io_utils
  logger
  error_utils
  memory_units_parser
  memory_utils
  omp_lock
  semaphore
  math_utils

  ## Common libraries
  argument_parser

  ## Index search libraries
  filenames_parser
  filesize_load_balancer
  sbwt_builder
  sbwt_container
  poppy_builder
  presearcher_cpu
  presearcher_gpu

  sequence_file_parser
  seq_to_bits_converter
  positions_builder
  index_searcher
  index_results_printer

  # Color search libraries
  color_index_builder
  color_index_container
  index_file_parser
  color_searcher
  color_results_printer
)

# Link gpu items
target_link_libraries(common_libraries INTERFACE gpu_utils)
