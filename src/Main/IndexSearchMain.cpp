#include <omp.h>

#include "ArgumentParser/IndexSearchArgumentParser.h"
#include "FilenamesParser/FilenamesParser.h"
#include "FilesizeLoadBalancer/FilesizeLoadBalancer.h"
#include "Global/GlobalDefinitions.h"
#include "Main/IndexSearchMain.h"
#include "Presearcher/Presearcher.h"
#include "SbwtBuilder/SbwtBuilder.h"
#include "SbwtContainer/CpuSbwtContainer.h"
#include "SbwtContainer/GpuSbwtContainer.h"
#include "Tools/GpuUtils.h"
#include "Tools/Logger.h"
#include "Tools/MathUtils.hpp"
#include "Tools/MemoryUtils.h"
#include "Tools/StdUtils.hpp"
#include "fmt/core.h"

namespace sbwt_search {

using fmt::format;
using gpu_utils::get_free_gpu_memory;
using log_utils::Logger;
using math_utils::bits_to_gB;
using math_utils::round_down;
using memory_utils::get_total_system_memory;
using std::cerr;
using std::endl;
using std::min;
using std::runtime_error;

const u64 string_sequence_batch_producer_max_batches = 2;
const u64 string_break_batch_producer_max_batches = 2;
const u64 interval_batch_producer_max_batches = 2;
const u64 sequence_file_parser_max_batches = 2;
const u64 invalid_chars_producer_max_batches = 2;
const u64 bits_producer_max_batches = 2;
const u64 positions_builder_max_batches = 2;
const u64 searcher_max_batches = 2;

auto IndexSearchMain::main(int argc, char **argv) -> int {
  const string program_name = "index";
  const string program_description = "sbwt_search";
  Logger::log_timed_event("main", Logger::EVENT_STATE::START);
  args = make_unique<IndexSearchArgumentParser>(
    program_name, program_description, argc, argv
  );
  Logger::log(Logger::LOG_LEVEL::INFO, "Loading components into memory");
  auto gpu_container = get_gpu_container();
  kmer_size = gpu_container->get_kmer_size();
  max_index = gpu_container->get_max_index();
  auto [split_input_filenames, split_output_filenames]
    = get_input_output_filenames();
  load_batch_info();
  omp_set_nested(1);
  load_threads();
  Logger::log(
    Logger::LOG_LEVEL::INFO,
    format("Running OpenMP with {} threads", get_threads())
  );
  auto
    [sequence_file_parsers,
     seq_to_bits_converters,
     positions_builders,
     searchers,
     results_printers]
    = get_components(
      gpu_container, split_input_filenames, split_output_filenames
    );
  Logger::log(Logger::LOG_LEVEL::INFO, "Running queries");
  run_components(
    sequence_file_parsers,
    seq_to_bits_converters,
    positions_builders,
    searchers,
    results_printers
  );
  Logger::log(Logger::LOG_LEVEL::INFO, "Finished");
  Logger::log_timed_event("main", Logger::EVENT_STATE::STOP);
  return 0;
}

auto IndexSearchMain::get_args() const -> const IndexSearchArgumentParser & {
  return *args;
}

auto IndexSearchMain::get_gpu_container() -> shared_ptr<GpuSbwtContainer> {
  Logger::log_timed_event("SBWTLoader", Logger::EVENT_STATE::START);
  Logger::log_timed_event("SBWTParserAndIndex", Logger::EVENT_STATE::START);
  auto builder
    = SbwtBuilder(get_args().get_index_file(), args->get_colors_file());
  auto cpu_container = builder.get_cpu_sbwt();
  Logger::log_timed_event("SBWTParserAndIndex", Logger::EVENT_STATE::STOP);
  Logger::log_timed_event("SbwtGpuTransfer", Logger::EVENT_STATE::START);
  auto gpu_container = cpu_container->to_gpu();
  Logger::log_timed_event("SbwtGpuTransfer", Logger::EVENT_STATE::STOP);
  auto presearcher = Presearcher(gpu_container);
  Logger::log_timed_event("Presearcher", Logger::EVENT_STATE::START);
  presearcher.presearch();
  Logger::log_timed_event("Presearcher", Logger::EVENT_STATE::STOP);
  Logger::log_timed_event("SBWTLoader", Logger::EVENT_STATE::STOP);
  return gpu_container;
}

auto IndexSearchMain::load_batch_info() -> void {
  max_chars_per_batch = get_max_chars_per_batch();
  max_seqs_per_batch
    = max_chars_per_batch / get_args().get_base_pairs_per_seq();
  if (max_chars_per_batch == 0) { throw runtime_error("Not enough memory"); }
  Logger::log(
    Logger::LOG_LEVEL::INFO,
    format(
      "Using {} max characters per batch and {} max seqs per batch",
      max_chars_per_batch,
      max_seqs_per_batch
    )
  );
}

auto IndexSearchMain::get_max_chars_per_batch() -> u64 {
  if (streams == 0) {
    cerr << "ERROR: Initialise batches before max_chars_per_batch" << endl;
    std::quick_exit(1);
  }
  auto cpu_chars = get_max_chars_per_batch_cpu();
#if defined(__HIP_CPU_RT__)
  auto gpu_chars = numeric_limits<u64>::max();
#else
  auto gpu_chars = get_max_chars_per_batch_gpu();
#endif
  return round_down<u64>(min(cpu_chars, gpu_chars), threads_per_block);
}

auto IndexSearchMain::get_max_chars_per_batch_gpu() -> u64 {
  u64 free = static_cast<u64>(
    static_cast<double>(get_free_gpu_memory() * bits_in_byte)
    * get_args().get_gpu_memory_percentage()
  );
  const u64 bits_required_per_character
    = ContinuousIndexSearcher::get_bits_per_element_gpu();
  auto max_chars_per_batch = free / bits_required_per_character / streams;
  Logger::log(
    Logger::LOG_LEVEL::DEBUG,
    format(
      "Free gpu memory: {} bits ({:.2f}GB). This allows for {} characters per "
      "batch",
      free,
      bits_to_gB(free),
      max_chars_per_batch
    )
  );
  return max_chars_per_batch;
}

auto IndexSearchMain::get_max_chars_per_batch_cpu() -> u64 {
  if (get_args().get_unavailable_ram() > get_total_system_memory() * bits_in_byte) {
    throw runtime_error("Not enough memory. Please specify a lower number of "
                        "unavailable-main-memory.");
  }
  u64 available_ram = min(
    get_total_system_memory() * bits_in_byte, get_args().get_max_cpu_memory()
  );
  u64 unavailable_ram = get_args().get_unavailable_ram();
  u64 free_bits = (unavailable_ram > available_ram) ?
    0 :
    static_cast<u64>(
      static_cast<double>(available_ram - unavailable_ram)
      * get_args().get_cpu_memory_percentage()
    );
  const double bits_required_per_character
    = static_cast<double>(
        // bits per element
        StringSequenceBatchProducer::get_bits_per_element()
          * string_sequence_batch_producer_max_batches
        + InvalidCharsProducer::get_bits_per_element()
          * invalid_chars_producer_max_batches
        + BitsProducer::get_bits_per_element() * bits_producer_max_batches
        + ContinuousPositionsBuilder::get_bits_per_element()
          * positions_builder_max_batches
        + ContinuousIndexSearcher::get_bits_per_element_cpu()
          * searcher_max_batches
        + get_results_printer_bits_per_element()
      )
    // bits per seq
    + static_cast<double>(
        IntervalBatchProducer::get_bits_per_seq()
          * string_break_batch_producer_max_batches
        + get_results_printer_bits_per_seq()
      )
      / static_cast<double>(get_args().get_base_pairs_per_seq())
#if defined(__HIP_CPU_RT__)  // include gpu required memory as well
    + static_cast<double>(ContinuousIndexSearcher::get_bits_per_element_gpu())
#endif
    ;
  u64 max_chars_per_batch = static_cast<u64>(std::floor(
    static_cast<double>(free_bits) / bits_required_per_character
    / static_cast<double>(streams)
  ));
  Logger::log(
    Logger::LOG_LEVEL::DEBUG,
    format(
      "Free main memory: {} bits ({:.2f}GB). This allows for {} "
      "characters per batch",
      free_bits,
      bits_to_gB(free_bits),
      max_chars_per_batch
    )
  );
  return max_chars_per_batch;
}

auto IndexSearchMain::get_results_printer_bits_per_element() -> u64 {
  if (get_args().get_print_mode() == "ascii") {
    return AsciiContinuousIndexResultsPrinter::get_bits_per_element(max_index);
  }
  if (get_args().get_print_mode() == "binary") {
    return BinaryContinuousIndexResultsPrinter::get_bits_per_element();
  }
  if (get_args().get_print_mode() == "bool") {
    return BoolContinuousIndexResultsPrinter::get_bits_per_element();
  }
    if (get_args().get_print_mode() == "packedint") {
    return PackedIntContinuousIndexResultsPrinter::get_bits_per_element(max_index);
  }
  throw runtime_error("Invalid value passed by user for argument print_mode");
}

auto IndexSearchMain::get_results_printer_bits_per_seq() -> u64 {
  if (get_args().get_print_mode() == "ascii") { return 0; }
  if (get_args().get_print_mode() == "binary") {
    return BinaryContinuousIndexResultsPrinter::get_bits_per_seq();
  }
  if (get_args().get_print_mode() == "bool") {
    return BinaryContinuousIndexResultsPrinter::get_bits_per_seq();
  }
  if (get_args().get_print_mode() == "packedint") {
    return PackedIntContinuousIndexResultsPrinter::get_bits_per_seq();
  }
  throw runtime_error("Invalid value passed by user for argument print_mode");
}

auto IndexSearchMain::get_components(
  const shared_ptr<GpuSbwtContainer> &gpu_container,
  const vector<vector<string>> &input_filenames,
  const vector<vector<string>> &output_filenames
)
  -> std::tuple<
    vector<shared_ptr<ContinuousSequenceFileParser>>,
    vector<shared_ptr<ContinuousSeqToBitsConverter>>,
    vector<shared_ptr<ContinuousPositionsBuilder>>,
    vector<shared_ptr<ContinuousIndexSearcher>>,
    vector<shared_ptr<IndexResultsPrinter>>> {
  Logger::log_timed_event("MemoryAllocator", Logger::EVENT_STATE::START);
  vector<shared_ptr<ContinuousSequenceFileParser>> sequence_file_parsers(streams
  );
  vector<shared_ptr<ContinuousSeqToBitsConverter>> seq_to_bits_converters(
    streams
  );
  vector<shared_ptr<ContinuousPositionsBuilder>> positions_builders(streams);
  vector<shared_ptr<ContinuousIndexSearcher>> searchers(streams);
  vector<shared_ptr<IndexResultsPrinter>> results_printers(streams);
#pragma omp parallel for
  for (u64 i = 0; i < streams; ++i) {
    Logger::log_timed_event(
      format("SequenceFileParserAllocator_{}", i), Logger::EVENT_STATE::START
    );
    sequence_file_parsers[i] = make_shared<ContinuousSequenceFileParser>(
      i,
      input_filenames[i],
      kmer_size,
      max_chars_per_batch,
      max_seqs_per_batch,
      string_sequence_batch_producer_max_batches,
      string_break_batch_producer_max_batches,
      interval_batch_producer_max_batches
    );
    Logger::log_timed_event(
      format("SequenceFileParserAllocator_{}", i), Logger::EVENT_STATE::STOP
    );

    Logger::log_timed_event(
      format("SeqToBitsConverterAllocator_{}", i), Logger::EVENT_STATE::START
    );
    seq_to_bits_converters[i] = make_shared<ContinuousSeqToBitsConverter>(
      i,
      sequence_file_parsers[i]->get_string_sequence_batch_producer(),
      get_threads(),
      kmer_size,
      max_chars_per_batch,
      invalid_chars_producer_max_batches,
      bits_producer_max_batches
    );
    Logger::log_timed_event(
      format("SeqToBitsConverterAllocator_{}", i), Logger::EVENT_STATE::STOP
    );

    Logger::log_timed_event(
      format("PositionsBuilderAllocator_{}", i), Logger::EVENT_STATE::START
    );
    positions_builders[i] = make_shared<ContinuousPositionsBuilder>(
      i,
      sequence_file_parsers[i]->get_string_break_batch_producer(),
      kmer_size,
      max_chars_per_batch,
      positions_builder_max_batches
    );
    Logger::log_timed_event(
      format("PositionsBuilderAllocator_{}", i), Logger::EVENT_STATE::STOP
    );

    Logger::log_timed_event(
      format("SearcherAllocator_{}", i), Logger::EVENT_STATE::START
    );
    searchers[i] = make_shared<ContinuousIndexSearcher>(
      i,
      gpu_container,
      seq_to_bits_converters[i]->get_bits_producer(),
      positions_builders[i],
      searcher_max_batches,
      max_chars_per_batch,
      !args->get_colors_file().empty()
    );
    Logger::log_timed_event(
      format("SearcherAllocator_{}", i), Logger::EVENT_STATE::STOP
    );

    Logger::log_timed_event(
      format("ResultsPrinterAllocator_{}", i), Logger::EVENT_STATE::START
    );
    results_printers[i] = get_results_printer(
      i,
      searchers[i],
      sequence_file_parsers[i]->get_interval_batch_producer(),
      seq_to_bits_converters[i]->get_invalid_chars_producer(),
      output_filenames[i]
    );
    Logger::log_timed_event(
      format("ResultsPrinterAllocator_{}", i), Logger::EVENT_STATE::STOP
    );
  }
  Logger::log_timed_event("MemoryAllocator", Logger::EVENT_STATE::STOP);

  return {
    std::move(sequence_file_parsers),
    std::move(seq_to_bits_converters),
    std::move(positions_builders),
    std::move(searchers),
    std::move(results_printers)};
}

auto IndexSearchMain::get_results_printer(
  u64 stream_id,
  const shared_ptr<ContinuousIndexSearcher> &searcher,
  const shared_ptr<IntervalBatchProducer> &interval_batch_producer,
  const shared_ptr<InvalidCharsProducer> &invalid_chars_producer,
  const vector<string> &output_filenames
) -> shared_ptr<IndexResultsPrinter> {
  if (get_args().get_print_mode() == "ascii") {
    return make_shared<IndexResultsPrinter>(AsciiContinuousIndexResultsPrinter(
      stream_id,
      searcher,
      interval_batch_producer,
      invalid_chars_producer,
      output_filenames,
      kmer_size,
      get_threads(),
      max_chars_per_batch,
      max_seqs_per_batch,
      get_args().get_write_headers(),
      max_index
    ));
  }
  if (get_args().get_print_mode() == "binary") {
    return make_shared<IndexResultsPrinter>(BinaryContinuousIndexResultsPrinter(
      stream_id,
      searcher,
      interval_batch_producer,
      invalid_chars_producer,
      output_filenames,
      kmer_size,
      get_threads(),
      max_chars_per_batch,
      max_seqs_per_batch,
      get_args().get_write_headers()
    ));
  }
  if (get_args().get_print_mode() == "bool") {
    return make_shared<IndexResultsPrinter>(BoolContinuousIndexResultsPrinter(
      stream_id,
      searcher,
      interval_batch_producer,
      invalid_chars_producer,
      output_filenames,
      kmer_size,
      get_threads(),
      max_chars_per_batch,
      max_seqs_per_batch,
      get_args().get_write_headers()
    ));
  }
  if (get_args().get_print_mode() == "packedint") {
    return make_shared<IndexResultsPrinter>(PackedIntContinuousIndexResultsPrinter(
      stream_id,
      searcher,
      interval_batch_producer,
      invalid_chars_producer,
      output_filenames,
      kmer_size,
      get_threads(),
      max_chars_per_batch,
      max_seqs_per_batch,
      get_args().get_write_headers(),
      max_index
    ));
  }
  throw runtime_error("Invalid value passed by user for argument print_mode");
}

auto IndexSearchMain::get_input_output_filenames()
  -> std::tuple<vector<vector<string>>, vector<vector<string>>> {
  FilenamesParser filenames_parser(
    get_args().get_query_file(), get_args().get_output_file()
  );
  auto input_filenames = filenames_parser.get_input_filenames();
  auto output_filenames = filenames_parser.get_output_filenames();
  if (input_filenames.size() != output_filenames.size()) {
    throw runtime_error("Input and output file sizes differ");
  }
  streams = min(input_filenames.size(), args->get_streams());
  Logger::log(Logger::LOG_LEVEL::DEBUG, format("Using {} streams", streams));
  return FilesizeLoadBalancer(input_filenames, output_filenames)
    .partition(streams);
}

auto IndexSearchMain::run_components(
  vector<shared_ptr<ContinuousSequenceFileParser>> &sequence_file_parsers,
  vector<shared_ptr<ContinuousSeqToBitsConverter>> &seq_to_bits_converters,
  vector<shared_ptr<ContinuousPositionsBuilder>> &positions_builders,
  vector<shared_ptr<ContinuousIndexSearcher>> &searchers,
  vector<shared_ptr<IndexResultsPrinter>> &results_printers
) -> void {
  Logger::log_timed_event("Querier", Logger::EVENT_STATE::START);
  const u64 num_components = 5;
#pragma omp parallel sections num_threads(num_components)
  {
#pragma omp section
#pragma omp parallel for num_threads(streams)
    for (auto &element : sequence_file_parsers) {
      element->read_and_generate();
    }
#pragma omp section
#pragma omp parallel for num_threads(streams)
    for (auto &element : seq_to_bits_converters) {
      element->read_and_generate();
    }
#pragma omp section
#pragma omp parallel for num_threads(streams)
    for (auto &element : positions_builders) { element->read_and_generate(); }
#pragma omp section
#pragma omp parallel for num_threads(streams)
    for (auto &element : searchers) { element->read_and_generate(); }
#pragma omp section
#pragma omp parallel for num_threads(streams)
    for (auto &element : results_printers) {
      std::visit([](auto &arg) -> void { arg.read_and_generate(); }, *element);
    }
  }
  Logger::log_timed_event("Querier", Logger::EVENT_STATE::STOP);
}

}  // namespace sbwt_search
