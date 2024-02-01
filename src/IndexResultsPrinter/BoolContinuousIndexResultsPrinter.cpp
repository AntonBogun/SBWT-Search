#include <fmt/format.h>

#include "IndexResultsPrinter/BoolContinuousIndexResultsPrinter.h"
#include "Tools/TypeDefinitions.h"

namespace sbwt_search {

BoolContinuousIndexResultsPrinter::BoolContinuousIndexResultsPrinter(
  u64 stream_id,
  shared_ptr<SharedBatchesProducer<ResultsBatch>> results_producer,
  shared_ptr<SharedBatchesProducer<IntervalBatch>> interval_producer,
  shared_ptr<SharedBatchesProducer<InvalidCharsBatch>> invalid_chars_producer,
  vector<string> filenames,
  u64 kmer_size,
  u64 threads,
  u64 max_chars_per_batch,
  u64 max_seqs_per_batch,
  bool write_headers
):
    Base(
      stream_id,
      std::move(results_producer),
      std::move(interval_producer),
      std::move(invalid_chars_producer),
      std::move(filenames),
      kmer_size,
      threads,
      max_chars_per_batch,
      max_seqs_per_batch,
      1,
      1,
      write_headers
    ) {}

auto BoolContinuousIndexResultsPrinter::get_bits_per_element() -> u64 {
  return bits_in_byte;
}

auto BoolContinuousIndexResultsPrinter::get_bits_per_seq() -> u64 {
  return bits_in_byte;
}

auto BoolContinuousIndexResultsPrinter::do_with_result(
  vector<char>::iterator buffer, u64
) -> u64 {
  *buffer = '0';
  return 1;
}

auto BoolContinuousIndexResultsPrinter::do_with_not_found(
  vector<char>::iterator buffer
) const -> u64 {
  *buffer = '1';
  return 1;
}

auto BoolContinuousIndexResultsPrinter::do_with_invalid(
  vector<char>::iterator buffer
) const -> u64 {
  *buffer = '2';
  return 1;
}

auto BoolContinuousIndexResultsPrinter::do_with_newline(
  vector<char>::iterator buffer
) const -> u64 {
  *buffer = '\n';
  return 1;
}

auto BoolContinuousIndexResultsPrinter::do_get_extension() -> string {
  return ".bool";
}
auto BoolContinuousIndexResultsPrinter::do_get_format() -> string {
  return "bool";
}
auto BoolContinuousIndexResultsPrinter::do_get_version() -> string {
  return "v2.0";
}

}  // namespace sbwt_search
