#include <chrono>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "SeqToBitsConverter/ContinuousSeqToBitsConverter.hpp"
#include "TestUtils/GeneralTestUtils.hpp"
#include "Utils/TypeDefinitions.h"

using std::make_shared;
using std::string;
using std::unique_ptr;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace sbwt_search {

class DummyParser {
  private:
    int counter = 0;

  public:
    vector<vector<u64>> string_indexes, char_indexes, cumulative_char_indexes;
    vector<vector<string>> buffers;
    DummyParser(
      vector<vector<string>> &buffers,
      vector<vector<u64>> &string_indexes,
      vector<vector<u64>> &char_indexes,
      vector<vector<u64>> &cumulative_char_indexes
    ):
        buffers(buffers),
        string_indexes(string_indexes),
        cumulative_char_indexes(cumulative_char_indexes),
        char_indexes(char_indexes) {}

    auto operator>>(shared_ptr<StringSequenceBatch> &batch) -> bool {
      if (counter < string_indexes.size()) {
        auto result_batch = make_shared<StringSequenceBatch>();
        result_batch->buffer = buffers[counter];
        result_batch->string_indexes = string_indexes[counter];
        result_batch->char_indexes = char_indexes[counter];
        result_batch->cumulative_char_indexes
          = cumulative_char_indexes[counter];
        batch = result_batch;
        ++counter;
        return true;
      }
      return false;
    }
};

class ContinuousSeqToBitsConverterTest: public ::testing::Test {
  protected:
    vector<string> buffer_example_1 = {
      "ACgT",  // 00011011
      "gn",  // 1000
      "GAt",  // 100011 // n will be 99
      "GtCa",  // 10110100
      "AAAAaAAaAAAAAAAaAAAAAAAAAAAAAAAA",  // 32 As = 64 0s
      "GC"  // 1001
    };
    // 1st 64b: 0001101110001000111011010000000000000000000000000000000000000000
    // 2nd 64b: 0000000000000000000000000010010000000000000000000000000000000000
    // We apply 0 padding on the right to get decimal equivalent
    // Using some online converter, we get the following decimal equivalents:
    vector<u64> expected_bits_1 = { 1984096220112486400, 154618822656 };

    vector<string> buffer_example_2 = {
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAT",  // 63A+T
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAG",  // 63A+G
    };

    const uint kmer_size = 30;
    const uint max_ints_per_batch = 999;

    vector<char> expected_invalid_1, expected_invalid_2;
    ContinuousSeqToBitsConverterTest() {
      expected_invalid_1 = vector<char>(max_ints_per_batch + kmer_size);
      expected_invalid_1[5] = 1;
      expected_invalid_2 = vector<char>(max_ints_per_batch + kmer_size);
    }

    vector<u64> expected_bits_2 = { 0, 3, 0, 2 };
    vector<vector<string>> buffers;
    vector<vector<u64>> expected_bits;
    vector<vector<char>> expected_invalid;
    vector<vector<u64>> string_indexes = { { 0, 6 } };
    vector<vector<u64>> char_indexes = { { 0, 0 } };
    vector<vector<u64>> cumulative_char_indexes = { { 0, 47 } };

    auto shared_tests() -> void {
      auto parser = make_shared<DummyParser>(
        buffers, string_indexes, char_indexes, cumulative_char_indexes
      );
      auto host = ContinuousSeqToBitsConverter<DummyParser>(parser, 1, kmer_size, max_ints_per_batch);
      host.read_and_generate();
      shared_ptr<vector<u64>> bit_output;
      shared_ptr<vector<char>> invalid_output;
      for (int i = 0; (host >> bit_output) & (host >> invalid_output); ++i) {
        assert_vectors_equal(expected_bits[i], *bit_output, __FILE__, __LINE__);
        assert_vectors_equal(expected_invalid[i], *invalid_output, __FILE__, __LINE__);
      }
    }
};

TEST_F(ContinuousSeqToBitsConverterTest, SingleBatch) {
  buffers = { { buffer_example_1 } };
  expected_bits = { { expected_bits_1 } };
  expected_invalid = { { expected_invalid_1 } };
  shared_tests();
}

TEST_F(ContinuousSeqToBitsConverterTest, MultipleBatches) {
  buffers = { buffer_example_1, buffer_example_1 };
  expected_bits = { expected_bits_1, expected_bits_1 };
  expected_invalid = { expected_invalid_1, expected_invalid_1 };
  string_indexes = { { 0, 6 }, { 0, 6 } };
  char_indexes = { { 0, 0 }, { 0, 0 } };
  cumulative_char_indexes = { { 0, 47 }, { 0, 47 } };
  shared_tests();
}

TEST_F(ContinuousSeqToBitsConverterTest, TestParallel) {
  omp_set_nested(1);
  // create 2 big buffers (example by generating random chars. Buffers can
  // be a single large string) give the buffers and indexes to the test
  const uint threads = 2, iterations = 60;
  auto sleep_amount = 200;
  auto max_ints_per_batch = 99;
  auto max_batches = 3;
  milliseconds::rep read_time;
  buffers = {};
  string_indexes = {};
  char_indexes = {};
  cumulative_char_indexes = {};
  for (uint i = 0; i < iterations / 2; ++i) {
    buffers.push_back(buffer_example_1);
    expected_bits.push_back(expected_bits_1);
    string_indexes.push_back({ 0, 4, 6 });
    char_indexes.push_back({ 0, 32 - 13, 0 });
    cumulative_char_indexes.push_back({ 0, 32, 47 });

    buffers.push_back(buffer_example_2);
    expected_bits.push_back(expected_bits_2);
    string_indexes.push_back({ 0, 1, 2 });
    char_indexes.push_back({ 0, 0, 0 });
    cumulative_char_indexes.push_back({ 0, 64, 120 });
  }
  auto parser = make_shared<DummyParser>(
    buffers, string_indexes, char_indexes, cumulative_char_indexes
  );
  auto host = ContinuousSeqToBitsConverter<DummyParser>(
    parser, threads, kmer_size, max_ints_per_batch, max_batches
  );
  vector<vector<u64>> outputs;
  int counter = 0;
#pragma omp parallel sections
  {
#pragma omp section
    {
      auto start_time = high_resolution_clock::now();
      host.read_and_generate();
      auto end_time = high_resolution_clock::now();
      read_time = duration_cast<milliseconds>(end_time - start_time).count();
    }
#pragma omp section
    {
      sleep_for(milliseconds(sleep_amount));
      shared_ptr<vector<u64>> seq_output;
      shared_ptr<vector<char>> invalid_output;
      while (host >> seq_output & host >> invalid_output) {
        outputs.push_back(*seq_output);
      };
    }
  }
  ASSERT_EQ(outputs.size(), iterations);
  for (uint i = 0; i < iterations; ++i) {
    assert_vectors_equal(expected_bits[i], outputs[i], __FILE__, __LINE__);
  }
  ASSERT_GE(read_time, sleep_amount);
}

}
