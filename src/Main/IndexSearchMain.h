#ifndef INDEX_SEARCH_MAIN_H
#define INDEX_SEARCH_MAIN_H

/**
 * @file IndexSearchMain.h
 * @brief The main function for searching the index. The 'index' mode of the
 * main executable.
 */

#include <memory>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "ArgumentParser/IndexSearchArgumentParser.h"
#include "IndexResultsPrinter/AsciiContinuousIndexResultsPrinter.h"
#include "IndexResultsPrinter/BinaryContinuousIndexResultsPrinter.h"
#include "IndexResultsPrinter/BoolContinuousIndexResultsPrinter.h"
#include "IndexResultsPrinter/PackedIntContinuousIndexResultsPrinter.h"
#include "IndexSearcher/ContinuousIndexSearcher.h"
#include "Main/Main.h"
#include "PositionsBuilder/ContinuousPositionsBuilder.h"
#include "SbwtContainer/GpuSbwtContainer.h"
#include "SeqToBitsConverter/ContinuousSeqToBitsConverter.h"
#include "SequenceFileParser/ContinuousSequenceFileParser.h"

namespace sbwt_search {

using std::shared_ptr;
using std::string;
using std::tuple;
using std::variant;
using std::vector;

using IndexResultsPrinter = variant<
  AsciiContinuousIndexResultsPrinter,
  BinaryContinuousIndexResultsPrinter,
  BoolContinuousIndexResultsPrinter,
  PackedIntContinuousIndexResultsPrinter>;

class IndexSearchMain: public Main {
public:
  auto main(int argc, char **argv) -> int override;

private:
  u64 kmer_size = 0;
  u64 streams = 0;
  u64 max_chars_per_batch = 0;
  u64 max_seqs_per_batch = 0;
  u64 max_index;
  unique_ptr<IndexSearchArgumentParser> args;

  [[nodiscard]] auto get_args() const -> const IndexSearchArgumentParser &;
  auto get_gpu_container() -> shared_ptr<GpuSbwtContainer>;
  auto load_batch_info() -> void;
  auto get_max_chars_per_batch_cpu() -> u64;
  auto get_results_printer_bits_per_element() -> u64;
  auto get_results_printer_bits_per_seq() -> u64;
  auto get_max_chars_per_batch_gpu() -> u64;
  auto get_max_chars_per_batch() -> u64;
  auto get_components(
    const shared_ptr<GpuSbwtContainer> &gpu_container,
    const vector<vector<string>> &input_filenames,
    const vector<vector<string>> &output_filenames
  )
    -> tuple<
      vector<shared_ptr<ContinuousSequenceFileParser>>,
      vector<shared_ptr<ContinuousSeqToBitsConverter>>,
      vector<shared_ptr<ContinuousPositionsBuilder>>,
      vector<shared_ptr<ContinuousIndexSearcher>>,
      vector<shared_ptr<IndexResultsPrinter>>>;

  auto get_input_output_filenames()
    -> tuple<vector<vector<string>>, vector<vector<string>>>;
  auto get_results_printer(
    u64 stream_id,
    const shared_ptr<ContinuousIndexSearcher> &searcher,
    const shared_ptr<IntervalBatchProducer> &interval_batch_producer,
    const shared_ptr<InvalidCharsProducer> &invalid_chars_producer,
    const vector<string> &split_output_filenames
  ) -> shared_ptr<IndexResultsPrinter>;
  auto run_components(
    vector<shared_ptr<ContinuousSequenceFileParser>> &sequence_file_parsers,
    vector<shared_ptr<ContinuousSeqToBitsConverter>> &seq_to_bits_converters,
    vector<shared_ptr<ContinuousPositionsBuilder>> &positions_builders,
    vector<shared_ptr<ContinuousIndexSearcher>> &searchers,
    vector<shared_ptr<IndexResultsPrinter>> &results_printers
  ) -> void;
};

}  // namespace sbwt_search

#endif
