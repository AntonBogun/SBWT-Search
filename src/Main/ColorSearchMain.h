#ifndef COLOR_SEARCH_MAIN_H
#define COLOR_SEARCH_MAIN_H

/**
 * @file ColorSearchMain.h
 * @brief The main function for searching for the colors. The 'color' mode of
 * the main executable
 */

#include <memory>
#include <string>
#include <variant>

#include "ArgumentParser/ColorSearchArgumentParser.h"
#include "ColorIndexContainer/GpuColorIndexContainer.h"
#include "ColorResultsPrinter/AsciiContinuousColorResultsPrinter.h"
#include "ColorResultsPrinter/BinaryContinuousColorResultsPrinter.h"
#include "ColorResultsPrinter/CsvContinuousColorResultsPrinter.h"
#include "ColorResultsPrinter/PackedIntContinuousColorResultsPrinter.h"
#include "ColorSearcher/ContinuousColorSearcher.h"
#include "IndexFileParser/ContinuousIndexFileParser.h"
#include "Main/Main.h"

namespace sbwt_search {

using std::shared_ptr;
using std::string;
using std::variant;

using ColorResultsPrinter = variant<
  AsciiContinuousColorResultsPrinter,
  BinaryContinuousColorResultsPrinter,
  CsvContinuousColorResultsPrinter,
  PackedIntContinuousColorResultsPrinter>;

class ColorSearchMain: public Main {
private:
  u64 num_colors = 0;
  u64 streams = 0;
  u64 max_indexes_per_batch = 0;
  u64 max_seqs_per_batch = 0;
  unique_ptr<ColorSearchArgumentParser> args;

public:
  auto main(int argc, char **argv) -> int override;

private:
  [[nodiscard]] auto get_args() const -> const ColorSearchArgumentParser &;
  auto get_gpu_container() -> shared_ptr<GpuColorIndexContainer>;
  auto load_batch_info() -> void;
  auto get_max_chars_per_batch_cpu() -> u64;
  auto get_max_chars_per_batch_gpu() -> u64;
  auto get_results_printer_bits_per_seq() -> u64;
  auto get_max_chars_per_batch() -> u64;
  auto get_input_output_filenames()
    -> std::tuple<vector<vector<string>>, vector<vector<string>>>;
  auto get_components(
    const shared_ptr<GpuColorIndexContainer> &gpu_container,
    const vector<vector<string>> &split_input_filenames,
    const vector<vector<string>> &split_output_filenames
  )
    -> std::tuple<
      vector<shared_ptr<ContinuousIndexFileParser>>,
      vector<shared_ptr<ContinuousColorSearcher>>,
      vector<shared_ptr<ColorResultsPrinter>>>;
  auto get_results_printer(
    u64 stream_id,
    shared_ptr<ContinuousIndexFileParser> &index_file_parser,
    shared_ptr<SharedBatchesProducer<ColorsBatch>> colors_batch_producer,
    const vector<string> &filenames,
    u64 num_colors
  ) -> shared_ptr<ColorResultsPrinter>;
  auto run_components(
    vector<shared_ptr<ContinuousIndexFileParser>> &index_file_parsers,
    vector<shared_ptr<ContinuousColorSearcher>> &color_searchers,
    vector<shared_ptr<ColorResultsPrinter>> &results_printers
  ) -> void;
};

}  // namespace sbwt_search

#endif
