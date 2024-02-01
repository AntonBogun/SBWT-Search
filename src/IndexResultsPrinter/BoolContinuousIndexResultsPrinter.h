#ifndef BOOL_CONTINUOUS_INDEX_RESULTS_PRINTER_H
#define BOOL_CONTINUOUS_INDEX_RESULTS_PRINTER_H

/**
 * @file BoolContinuousIndexResultsPrinter.h
 * @brief Inherits ContinuousIndexResultsPrinter and prints out values in ASCII
 * format. If a result is found, we print out a 0, if it is not found, we print
 * out a 1 and if it is invalid, we print 2. Different sequences are split with
 * a newline character. This output format is not suitable for pseudoalignment
 * since we lose the index value but it is the fastest and has the least memory
 * footprint.
 */

#include "IndexResultsPrinter/ContinuousIndexResultsPrinter.hpp"

namespace sbwt_search {

class BoolContinuousIndexResultsPrinter:
    public ContinuousIndexResultsPrinter<
      BoolContinuousIndexResultsPrinter,
      char> {
  using Base
    = ContinuousIndexResultsPrinter<BoolContinuousIndexResultsPrinter, char>;
  friend Base;

public:
  BoolContinuousIndexResultsPrinter(
    u64 stream_id,
    shared_ptr<SharedBatchesProducer<ResultsBatch>> results_producer,
    shared_ptr<SharedBatchesProducer<IntervalBatch>> interval_producer,
    shared_ptr<SharedBatchesProducer<InvalidCharsBatch>> invalid_chars_producer,
    vector<string> filenames_,
    u64 kmer_size,
    u64 threads,
    u64 max_chars_per_batch,
    u64 max_seqs_per_batch,
    bool write_headers
  );

  auto static get_bits_per_element() -> u64;
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
