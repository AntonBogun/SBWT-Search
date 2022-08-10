#include <chrono>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <tuple>

#include <gtest/gtest.h>

#include "SequenceFileParser/ContinuousSequenceFileParser.h"
#include "TestUtils/GeneralTestUtils.hpp"

using std::cerr;
using std::fill;
using std::make_shared;
using std::out_of_range;
using std::shared_ptr;
using std::stringstream;
using std::tie;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace sbwt_search {

class ContinuousSequenceFileParserTest: public ::testing::Test {
  protected:
    u64 kmer_size = 3;
    u64 max_characters_per_batch = UINT_MAX;
    u32 characters_per_send = 4;
    uint readers_amount = 1;
    uint max_batches = UINT_MAX;
    vector<u64> expected_string_indexes = { 0, 0, 2, 2, 4, 5, 6, 7 };
    vector<u64> expected_character_indexes = { 0, 4, 1, 5, 1, 0, 2, 0 };
    vector<vector<string>> expected_buffers;
    vector<string> filenames
      = { "test_objects/test_query.fna", "test_objects/test_query.fna" };

    auto shared_tests() -> void {
      ContinuousSequenceFileParser host(
        filenames,
        kmer_size,
        max_characters_per_batch,
        characters_per_send,
        readers_amount,
        max_batches
      );
      shared_ptr<vector<string>> batch = make_shared<vector<string>>();
      u64 string_index, character_index;
      int i = 0;
      while (host >> tie(batch, string_index, character_index)) {
        assert_vectors_equal(expected_buffers[i], *batch);
        ASSERT_EQ(expected_string_indexes[i], string_index);
        ASSERT_EQ(expected_character_indexes[i], character_index);
        ++i;
      }
    }
};

TEST_F(ContinuousSequenceFileParserTest, GetSimple) {
  vector<string> expected_buffer
    = { "GACTG", "AA", "GATCGA", "TA", "GACTG", "AA", "GATCGA", "TA" };
  expected_buffers.resize(8);
  fill(expected_buffers.begin(), expected_buffers.end(), expected_buffer);
  shared_tests();
}

TEST_F(ContinuousSequenceFileParserTest, GetMaxCharsPerBatchEqualToFileSize) {
  max_characters_per_batch = 15;
  expected_string_indexes = { 0, 0, 2, 2, 0, 0, 2, 2 };
  expected_character_indexes = { 0, 4, 1, 5, 0, 4, 1, 5 };
  vector<string> expected_buffer = { "GACTG", "AA", "GATCGA", "TA" };
  expected_buffers.resize(8);
  fill(expected_buffers.begin(), expected_buffers.end(), expected_buffer);
  shared_tests();
}

TEST_F(ContinuousSequenceFileParserTest, TestInvalidFile) {
  filenames = { "test_objects/test_query.fna",
                "invalid_file__",
                "test_objects/test_query.fna" };
  vector<string> expected_buffer
    = { "GACTG", "AA", "GATCGA", "TA", "GACTG", "AA", "GATCGA", "TA" };
  expected_buffers.resize(8);
  fill(expected_buffers.begin(), expected_buffers.end(), expected_buffer);
  stringstream mybuffer;
  auto *old_buf = cerr.rdbuf();
  cerr.rdbuf(mybuffer.rdbuf());
  shared_tests();
  cerr.rdbuf(old_buf);
  ASSERT_EQ("The file invalid_file__ cannot be opened\n", mybuffer.str());
}

TEST_F(ContinuousSequenceFileParserTest, TestStringTooLong) {
  filenames = { "test_objects/test_query.fna" };
  max_characters_per_batch = 5;
  expected_buffers
    = { { "GACTG" }, { "GACTG" }, { "AA", "TA" }, { "AA", "TA" } };
  expected_string_indexes = { 0, 0, 0 };
  expected_character_indexes = { 0, 4, 0 };
  stringstream mybuffer;
  auto *old_buf = cerr.rdbuf();
  cerr.rdbuf(mybuffer.rdbuf());
  shared_tests();
  cerr.rdbuf(old_buf);
  ASSERT_EQ(
    "The string at position 2 in file test_objects/test_query.fna is too "
    "large\n",
    mybuffer.str()
  );
}

TEST_F(ContinuousSequenceFileParserTest, TestParallel) {
  auto sleep_time = 300;
  max_characters_per_batch = 15;
  max_batches = 2;
  vector<string> filenames = { "test_objects/test_query.fna",
                               "test_objects/test_query.fna",
                               "test_objects/test_query.fna" };
  ContinuousSequenceFileParser host(
    filenames,
    kmer_size,
    max_characters_per_batch,
    characters_per_send,
    readers_amount,
    max_batches,
    false
  );
  vector<shared_ptr<vector<string>>> batches;
  vector<u64> string_indexes, character_indexes;
  expected_string_indexes = { 0, 0, 2, 2, 0, 0, 2, 2, 0, 0, 2, 2 };
  expected_character_indexes = { 0, 4, 1, 5, 0, 4, 1, 5, 0, 4, 1, 5 };
  vector<string> expected_buffer = { "GACTG", "AA", "GATCGA", "TA" };
  expected_buffers.resize(12);
  fill(expected_buffers.begin(), expected_buffers.end(), expected_buffer);
  milliseconds::rep read_time;
#pragma omp parallel sections
  {
#pragma omp section
    {
      auto start_time = high_resolution_clock::now();
      host.read();
      auto end_time = high_resolution_clock::now();
      read_time = duration_cast<milliseconds>(end_time - start_time).count();
    }
#pragma omp section
    {
      shared_ptr<vector<string>> batch = make_shared<vector<string>>();
      u64 string_index, character_index;
      sleep_for(milliseconds(sleep_time));
      while (host >> tie(batch, string_index, character_index)) {
        batches.push_back(batch);
        string_indexes.push_back(string_index);
        character_indexes.push_back(character_index);
      }
    }
  }
  ASSERT_EQ(expected_buffers.size(), batches.size());
  for (int i = 0; i < expected_buffers.size(); ++i) {
    assert_vectors_equal(expected_buffers[i], *batches[i]);
  }
  assert_vectors_equal(expected_string_indexes, string_indexes);
  assert_vectors_equal(expected_character_indexes, character_indexes);
  ASSERT_GE(read_time, sleep_time);
}

}
