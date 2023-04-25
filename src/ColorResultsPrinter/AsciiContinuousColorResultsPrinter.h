#ifndef ASCII_CONTINUOUS_COLOR_RESULTS_PRINTER_H
#define ASCII_CONTINUOUS_COLOR_RESULTS_PRINTER_H

/**
 * @file AsciiContinuousColorResultsPrinter.h
 * @brief Outputs ascii results. Color indexes are ordered and space separated,
 * and each seq is placed on a new line. When calculating memory reservations
 * for this class, we use the max_index to see how many characters we really
 * need per index, rather than the maximum needed for the maximum u64. This
 * saves us a lot of space.
 */

#include "ColorResultsPrinter/ContinuousColorResultsPrinter.hpp"
#include "Tools/TypeDefinitions.h"

namespace sbwt_search {

class AsciiContinuousColorResultsPrinter:
    public ContinuousColorResultsPrinter<
      AsciiContinuousColorResultsPrinter,
      char> {
  using Base
    = ContinuousColorResultsPrinter<AsciiContinuousColorResultsPrinter, char>;
  friend Base;

public:
  AsciiContinuousColorResultsPrinter(
    u64 stream_id_,
    shared_ptr<SharedBatchesProducer<SeqStatisticsBatch>>
      seq_statistics_batch_producer_,
    shared_ptr<SharedBatchesProducer<ColorsBatch>> colors_batch_producer_,
    const vector<string> &filenames_,
    u64 num_colors_,
    double threshold_,
    bool include_not_found_,
    bool include_invalid_,
    u64 threads,
    u64 max_seqs_per_batch,
    bool write_headers
  );

  auto static get_bits_per_seq(u64 num_colors) -> u64;

protected:
  auto do_get_extension() -> string;
  auto do_get_format() -> string;
  auto do_get_version() -> string;

  auto do_with_newline(vector<char>::iterator buffer) -> u64;
  auto do_with_space(vector<char>::iterator buffer) -> u64;
  auto do_with_result(vector<char>::iterator buffer, u64 result) -> u64;
};

}  // namespace sbwt_search

#endif
