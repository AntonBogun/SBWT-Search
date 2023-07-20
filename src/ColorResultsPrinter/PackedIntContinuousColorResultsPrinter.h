#ifndef PACKEDINT_CONTINUOUS_COLOR_RESULTS_PRINTER_H
#define PACKEDINT_CONTINUOUS_COLOR_RESULTS_PRINTER_H

/**
 * @file PackedIntContinuousColorResultsPrinter.h
 * @brief Outputs packed int results.
 * Format is based on Variable Length Quantity (VLQ) encoding.
 * Packed int format is 7 bits of data per byte, 8th bit is 0 if last byte of number,
 * 1 otherwise. Last data bit is reserved for special values, here it is just the newline (0b01000010).
 * Color indexes are ordered and each seq is separated with newline character.
 * When calculating memory reservations for this class, we use the max_index to
 * see how many bytes we really need per index, rather than the maximum needed for 
 * the maximum u64. This saves us a lot of space.
 */

#include "ColorResultsPrinter/ContinuousColorResultsPrinter.hpp"
#include "Tools/TypeDefinitions.h"

namespace sbwt_search {

class PackedIntContinuousColorResultsPrinter:
    public ContinuousColorResultsPrinter<
      PackedIntContinuousColorResultsPrinter,
      char> {
  using Base
    = ContinuousColorResultsPrinter<PackedIntContinuousColorResultsPrinter, char>;
  friend Base;

public:
  PackedIntContinuousColorResultsPrinter(
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
  auto do_with_result(vector<char>::iterator buffer, u64 result) -> u64;
};

}  // namespace sbwt_search

#endif
