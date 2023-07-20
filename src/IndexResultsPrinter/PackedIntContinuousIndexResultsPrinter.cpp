#include "IndexResultsPrinter/PackedIntContinuousIndexResultsPrinter.h"
#include "Tools/TypeDefinitions.h"

namespace sbwt_search {

PackedIntContinuousIndexResultsPrinter::PackedIntContinuousIndexResultsPrinter(
  u64 stream_id,
  shared_ptr<SharedBatchesProducer<ResultsBatch>> results_producer,
  shared_ptr<SharedBatchesProducer<IntervalBatch>> interval_producer,
  shared_ptr<SharedBatchesProducer<InvalidCharsBatch>> invalid_chars_producer,
  vector<string> filenames,
  u64 kmer_size,
  u64 threads,
  u64 max_chars_per_batch,
  u64 max_seqs_per_batch,
  bool write_headers,
  u64 max_index
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
      get_bits_per_element(max_index) / bits_in_byte,
      1,
      write_headers
    ) {}

auto PackedIntContinuousIndexResultsPrinter::get_bits_per_element(u64 max_index)
  -> u64 {
  // should be ceil((log2(max_index)+1)/7), but +1 gets removed with ceil
  return ((64-__builtin_clzll(max_index))/7+1)*8;
}

auto PackedIntContinuousIndexResultsPrinter::get_bits_per_seq() -> u64 {
  return bits_in_byte;
}

//uses log2(result)+1 bits of data, since last bit is supposed to be 0
auto PackedIntContinuousIndexResultsPrinter::do_with_result(
  vector<char>::iterator buffer, u64 result
) -> u64 {
  int log2 = (64-__builtin_clzll(result));
  int bytes = log2/7+1;
  for (int i = 0; i < bytes; i++) {
    *(buffer+i) = 0x80 | (result & 0x7F);
    result >>= 7;
  }
  *(buffer+bytes-1) &= 0x7F;
  return bytes;
}

auto PackedIntContinuousIndexResultsPrinter::do_with_not_found(
  vector<char>::iterator buffer
) const -> u64 {
  *buffer = 0b01000000;
  return 1;
}

auto PackedIntContinuousIndexResultsPrinter::do_with_invalid(
  vector<char>::iterator buffer
) const -> u64 {
  *buffer = 0b01000001;
  return 1;
}


auto PackedIntContinuousIndexResultsPrinter::do_with_newline(
  vector<char>::iterator buffer
) const -> u64 {
  *buffer = 0b01000010;
  return 1;
}

auto PackedIntContinuousIndexResultsPrinter::do_get_extension() -> string {
  return ".pint";
}
auto PackedIntContinuousIndexResultsPrinter::do_get_format() -> string {
  return "packedint";
}
auto PackedIntContinuousIndexResultsPrinter::do_get_version() -> string {
  return "v1.0";
}

}  // namespace sbwt_search
