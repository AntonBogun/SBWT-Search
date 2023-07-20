#ifndef PACKED_INT_CONTINUOUS_INDEX_RESULTS_PRINTER_H
#define PACKED_INT_CONTINUOUS_INDEX_RESULTS_PRINTER_H

/**
 * @file PackedIntContinuousIndexResultsPrinter.h
 * @brief Inherits ContinuousIndexResultsPrinter and prints out packed int values.
 * Format is based on Variable Length Quantity (VLQ) encoding.
 * Packed int format is 7 bits of data per byte, 8th bit is 1 if this is not the
 * last byte of the number, 0 if it is. Last data bit is reserved to represent
 * special values: 0 for not-found, 1 for invalid, 2 for newline. This means
 * maximum value that can be stored is 2^63 - 1 rather than 2^64 - 1.
 */

#include "IndexResultsPrinter/ContinuousIndexResultsPrinter.hpp"

namespace sbwt_search {

class PackedIntContinuousIndexResultsPrinter:
    public ContinuousIndexResultsPrinter<
      PackedIntContinuousIndexResultsPrinter,
      char> {
  using Base
    = ContinuousIndexResultsPrinter<PackedIntContinuousIndexResultsPrinter, char>;
  friend Base;

public:
  PackedIntContinuousIndexResultsPrinter(
    u64 stream_id,
    shared_ptr<SharedBatchesProducer<ResultsBatch>> results_producer,
    shared_ptr<SharedBatchesProducer<IntervalBatch>> interval_producer,
    shared_ptr<SharedBatchesProducer<InvalidCharsBatch>> invalid_chars_producer,
    vector<string> filenames_,
    u64 kmer_size,
    u64 threads,
    u64 max_chars_per_batch,
    u64 max_seqs_per_batch,
    bool write_headers,
    u64 max_index
  );

  static auto get_bits_per_element(u64 max_index) -> u64;
  auto static get_bits_per_seq() -> u64;

protected:
  auto do_get_extension() -> string;
  auto do_get_format() -> string;
  auto do_get_version() -> string;

  [[nodiscard]] auto do_with_result(vector<char>::iterator buffer, u64 result)
    -> u64;
  [[nodiscard]] auto do_with_not_found(vector<char>::iterator buffer) const
    -> u64;
  [[nodiscard]] auto do_with_invalid(vector<char>::iterator buffer) const
    -> u64;
  [[nodiscard]] auto do_with_newline(vector<char>::iterator buffer) const
    -> u64;
};

}  // namespace sbwt_search

#endif
