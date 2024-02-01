#include <string>

#include "ColorResultsPrinter/BinaryContinuousColorResultsPrinter.h"

namespace sbwt_search {

BinaryContinuousColorResultsPrinter::BinaryContinuousColorResultsPrinter(
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
):
    Base(
      stream_id_,
      std::move(seq_statistics_batch_producer_),
      std::move(colors_batch_producer_),
      filenames_,
      num_colors_,
      threshold_,
      include_not_found_,
      include_invalid_,
      threads,
      num_colors_ + 1,
      max_seqs_per_batch,
      write_headers
    ) {}

auto BinaryContinuousColorResultsPrinter::get_bits_per_seq(u64 num_colors)
  -> u64 {
  return (num_colors + 1) * 64;
}

auto BinaryContinuousColorResultsPrinter::do_get_extension() -> string {
  return ".bin";
}
auto BinaryContinuousColorResultsPrinter::do_get_format() -> string {
  return "binary";
}
auto BinaryContinuousColorResultsPrinter::do_get_version() -> string {
  return "v1.0";
}

auto BinaryContinuousColorResultsPrinter::do_with_newline(
  vector<u64>::iterator buffer
) -> u64 {
  *buffer = static_cast<u64>(-3);
  return 1;
}
auto BinaryContinuousColorResultsPrinter::do_with_result(
  vector<u64>::iterator buffer, u64 result
) -> u64 {
  *buffer = result;
  return 1;
}

}  // namespace sbwt_search
