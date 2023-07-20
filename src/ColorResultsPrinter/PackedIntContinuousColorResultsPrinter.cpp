#include <string>

#include "ColorResultsPrinter/PackedIntContinuousColorResultsPrinter.h"

namespace sbwt_search {

PackedIntContinuousColorResultsPrinter::PackedIntContinuousColorResultsPrinter(
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
      get_bits_per_seq(num_colors_) / bits_in_byte,
      max_seqs_per_batch,
      write_headers
    ) {}

auto PackedIntContinuousColorResultsPrinter::get_bits_per_seq(u64 num_colors)
  -> u64 {
  //(bytes taken by max color * num_colors + byte taken by newline)*8
  return (((64-__builtin_clzll(num_colors))/7+1)*num_colors+1)*8;
}



auto PackedIntContinuousColorResultsPrinter::do_get_extension() -> string {
  return ".pint";
}
auto PackedIntContinuousColorResultsPrinter::do_get_format() -> string {
  return "packedint";
}
auto PackedIntContinuousColorResultsPrinter::do_get_version() -> string {
  return "v1.0";
}

auto PackedIntContinuousColorResultsPrinter::do_with_newline(
  vector<char>::iterator buffer
) -> u64 {
  *buffer = 0b01000010;
  return 1;
}
auto PackedIntContinuousColorResultsPrinter::do_with_result(
  vector<char>::iterator buffer, u64 result
) -> u64 {
  int log2 = (64-__builtin_clzll(result));
  int bytes =log2/7+1;
  for (int i = 0; i < bytes; i++) {
    *(buffer+i) = 0x80 | (result & 0x7F);
    result >>= 7;
  }
  *(buffer+bytes-1) &= 0x7F;
  return bytes;
}

}  // namespace sbwt_search
