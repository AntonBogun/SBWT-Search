#ifndef RAW_SEQUENCES_PARSER_H
#define RAW_SEQUENCES_PARSER_H

/**
 * @file RawSequencesParser.h
 * @brief Contains functions for parsing raw (ASCII format) sequences
 *        into bit vectors and also generating the positions which
 *        will be used as the starting kmer positions for the parallel
 *        implementation
 * */

#include <string>
#include <vector>

#include "GlobalDefinitions.h"

using std::string;
using std::vector;

namespace sbwt_search {

const u64 default_bits_hash_map_value = 99;

class RawSequencesParser {
private:
  u64 total_letters;
  u64 total_positions;
  uint kmer_size;
  bool has_parsed = false;
  void check_if_has_parsed();
  const vector<string> &seqs_strings;
  const vector<u64> bits_hash_map;
  vector<u64> seqs_bits;
  vector<u64> positions;
  void add_position(
    const size_t sequence_length,
    const u64 sequence_index,
    size_t &positions_index,
    const size_t global_index
  );
  void add_bits(
    const string &sequence,
    const u64 sequence_index,
    int &internal_shift,
    size_t &vector_index
  );

public:
  RawSequencesParser(
    const vector<string> &sequences,
    const size_t total_positions,
    const size_t total_letters,
    const u64 kmer_size
  );
  auto &get_seqs_bits() { return seqs_bits; };
  auto &get_positions() { return positions; };
  void parse_serial();
  void parse_parallel();
};

}

#endif
