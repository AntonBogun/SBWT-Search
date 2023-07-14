#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>

#include <fmt/core.h>

#include "IndexFileParser/AsciiIndexFileParser.h"
#include "IndexFileParser/BinaryIndexFileParser.h"
#include "IndexFileParser/ContinuousIndexFileParser.h"
#include "IndexFileParser/PackedIntIndexFileParser.h"
#include "Tools/IOUtils.h"
#include "Tools/Logger.h"

namespace sbwt_search {

using fmt::format;
using log_utils::Logger;
using std::ios;
using std::make_shared;
using std::numeric_limits;

ContinuousIndexFileParser::ContinuousIndexFileParser(
  u64 stream_id_,
  u64 max_indexes_per_batch_,
  u64 max_seqs_per_batch_,
  u64 warp_size_,
  vector<string> filenames_,
  u64 seq_statistics_batch_producer_max_batches,
  u64 indexes_batch_producer_max_batches
):
    max_indexes_per_batch(max_indexes_per_batch_),
    max_seqs_per_batch(max_seqs_per_batch_),
    warp_size(warp_size_),
    seq_statistics_batch_producer(make_shared<SeqStatisticsBatchProducer>(
      max_seqs_per_batch_, seq_statistics_batch_producer_max_batches
    )),
    indexes_batch_producer(make_shared<IndexesBatchProducer>(
      max_seqs_per_batch_,
      max_indexes_per_batch_,
      indexes_batch_producer_max_batches
    )),
    filenames(std::move(filenames_)),
    stream_id(stream_id_) {
  filename_iterator = filenames.begin();
}

auto ContinuousIndexFileParser::read_and_generate() -> void {
  start_next_file();
  while (!fail) {
    do_at_batch_start();
    reset_batches();
    read_next();
    do_at_batch_finish();
  }
  do_at_generate_finish();
}

auto ContinuousIndexFileParser::reset_batches() -> void {
  seq_statistics_batch_producer->current_write()->reset();
  indexes_batch_producer->current_write()->reset();
}

auto ContinuousIndexFileParser::start_next_file() -> bool {
  while (filename_iterator != filenames.end()) {
    auto filename = *filename_iterator++;
    Logger::log(
      Logger::LOG_LEVEL::INFO, format("Now reading file {}", filename)
    );
    auto seqs_statistics_batch = seq_statistics_batch_producer->current_write();
    seqs_statistics_batch->seqs_before_newfile.push_back(
      seqs_statistics_batch->colored_seq_id.size() - 1
    );
    try {
      start_new_file(filename);
      return true;
    } catch (ios::failure &e) {
      Logger::log(Logger::LOG_LEVEL::ERROR, e.what());
    }
  }
  fail = true;
  return false;
}

auto ContinuousIndexFileParser::start_new_file(const string &filename) -> void {
  auto in_stream = make_shared<ThrowingIfstream>(filename, ios::in);
  const string file_format = in_stream->read_string_with_size();
  if (file_format == "ascii") {  // NOLINT (bugprone-branch-clone)
    index_file_parser = make_unique<AsciiIndexFileParser>(
      std::move(in_stream), max_indexes_per_batch, max_seqs_per_batch, warp_size
    );
  } else if (file_format == "binary") {
    index_file_parser = make_unique<BinaryIndexFileParser>(
      std::move(in_stream), max_indexes_per_batch, max_seqs_per_batch, warp_size
    );
  } else if (file_format == "packedint") {
    index_file_parser = make_unique<PackedIntIndexFileParser>(
      std::move(in_stream), max_indexes_per_batch, max_seqs_per_batch, warp_size
    );
  } else {
    Logger::log(
      Logger::LOG_LEVEL::WARN, "Invalid file format in file: " + filename
    );
  }
}

auto ContinuousIndexFileParser::do_at_batch_start() -> void {
  seq_statistics_batch_producer->do_at_batch_start();
  indexes_batch_producer->do_at_batch_start();
  Logger::log_timed_event(
    format("ContinuousIndexFileParser_{}", stream_id),
    Logger::EVENT_STATE::START,
    format("batch {}", batch_id)
  );
}

auto ContinuousIndexFileParser::read_next() -> void {
  while (
    (indexes_batch_producer->current_write()->warped_indexes.size() < max_indexes_per_batch)
     && (seq_statistics_batch_producer->current_write()->found_idxs.size() < max_seqs_per_batch)
     && (
       index_file_parser->generate_batch(
         seq_statistics_batch_producer->current_write(),
         indexes_batch_producer->current_write()
       )
       || start_next_file()
      )
   ) {}
}

auto ContinuousIndexFileParser::do_at_batch_finish() -> void {
  auto num_indexes
    = indexes_batch_producer->current_write()->warped_indexes.size();
  auto seq_statistics_batch
    = get_seq_statistics_batch_producer()->current_write();
  auto seqs = seq_statistics_batch->found_idxs.size();
  auto num_found_idxs = std::accumulate(
    get_seq_statistics_batch_producer()->current_write()->found_idxs.begin(),
    get_seq_statistics_batch_producer()->current_write()->found_idxs.end(),
    0UL
  );
  auto num_invalid_idxs = std::accumulate(
    seq_statistics_batch->invalid_idxs.begin(),
    seq_statistics_batch->invalid_idxs.end(),
    0UL
  );
  auto num_not_found_idxs = std::accumulate(
    seq_statistics_batch->not_found_idxs.begin(),
    seq_statistics_batch->not_found_idxs.end(),
    0UL
  );
  Logger::log(
    Logger::LOG_LEVEL::DEBUG,
    format(
      "Batch {} in stream {} contains {} indexes in {} seqs, of which {} are "
      "found indexes and {} is padding. {} indexes were skipped, of which {} "
      "is not found and {} is invalids.",
      batch_id,
      stream_id,
      num_indexes,
      seqs,
      num_found_idxs,
      num_indexes - num_found_idxs,
      num_not_found_idxs + num_invalid_idxs,
      num_not_found_idxs,
      num_invalid_idxs
    )
  );
  Logger::log_timed_event(
    format("ContinuousIndexFileParser_{}", stream_id),
    Logger::EVENT_STATE::STOP,
    format("batch {}", batch_id)
  );
  ++batch_id;
  seq_statistics_batch_producer->current_write()->seqs_before_newfile.push_back(
    numeric_limits<u64>::max()
  );
  seq_statistics_batch_producer->do_at_batch_finish();
  indexes_batch_producer->do_at_batch_finish();
}

auto ContinuousIndexFileParser::do_at_generate_finish() -> void {
  seq_statistics_batch_producer->do_at_generate_finish();
  indexes_batch_producer->do_at_generate_finish();
}

auto ContinuousIndexFileParser::get_seq_statistics_batch_producer() const
  -> const shared_ptr<SeqStatisticsBatchProducer> & {
  return seq_statistics_batch_producer;
}
auto ContinuousIndexFileParser::get_indexes_batch_producer() const
  -> const shared_ptr<IndexesBatchProducer> & {
  return indexes_batch_producer;
}

}  // namespace sbwt_search
