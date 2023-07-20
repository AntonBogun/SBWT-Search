#include <stdexcept>
#include <string>

#include "IndexFileParser/PackedIntIndexFileParser.h"
#include "Tools/IOUtils.h"

namespace sbwt_search {

using std::runtime_error;
using std::string;

PackedIntIndexFileParser::PackedIntIndexFileParser(
  shared_ptr<ThrowingIfstream> in_stream_,
  u64 max_indexes_,
  u64 max_seqs_,
  u64 warp_size_,
  u64 buffer_size_
):
    IndexFileParser(std::move(in_stream_), max_indexes_, max_seqs_, warp_size_),
    buffer_size(buffer_size_) {
  assert_version();
  buffer.resize(buffer_size);
  load_buffer();
}

auto PackedIntIndexFileParser::assert_version() -> void {
  auto version = get_istream().read_string_with_size();
  if (version != "v1.0") {
    throw runtime_error("The file has an incompatible version number");
  }
}

auto PackedIntIndexFileParser::generate_batch(
  shared_ptr<SeqStatisticsBatch> seq_statistics_batch_,
  shared_ptr<IndexesBatch> indexes_batch_
) -> bool {
  IndexFileParser::generate_batch(
    std::move(seq_statistics_batch_), std::move(indexes_batch_)
  );
  const u64 initial_size = get_indexes_batch()->warped_indexes.size()
    + get_seq_statistics_batch()->colored_seq_id.size();
  char c = '\0';
  while (get_indexes().size() < get_max_indexes()
         && get_num_seqs() < get_max_seqs()
         && (!get_istream().eof() || buffer_index != buffer_size)) {
    c = getc();
    if (buffer_size == 0) { break; }  // EOF
    if (c == 0b01000000){
      ++get_seq_statistics_batch()->not_found_idxs.back();
    } else if (c == 0b01000001) {
      ++get_seq_statistics_batch()->invalid_idxs.back();
    } else if (c == 0b01000010) {
      end_seq();
    } else if (!(c & 0x80)){
      ++get_seq_statistics_batch()->found_idxs.back();
      get_indexes().push_back((u64) c);
    } else {
      ++get_seq_statistics_batch()->found_idxs.back();
      get_indexes().push_back(parse_number(c&0x7f));
    }
  }
  add_warp_interval();
  return (get_indexes_batch()->warped_indexes.size()
          + get_seq_statistics_batch()->colored_seq_id.size())
    > initial_size;
}

inline auto PackedIntIndexFileParser::load_buffer() -> void {
  get_istream().read(buffer.data(), static_cast<std::streamsize>(buffer_size));
  buffer_size = get_istream().gcount();
  buffer_index = 0;
}

inline auto PackedIntIndexFileParser::getc() -> char {
  if (buffer_index >= buffer_size) { load_buffer(); }
  if (buffer_size == 0) { return 0; }
  return buffer[buffer_index++];
}


inline auto PackedIntIndexFileParser::parse_number(u64 starting_number) -> u64 {
  auto result = starting_number;
  u32 i=1;
  char c = '\0';
  while ((c = getc()) & 0x80) {
    result |= ((c & 0x7f) << (i*7));
    i++;
  }
  return result | (c << (i*7));
}

}  // namespace sbwt_search
