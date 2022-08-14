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
using std::make_unique;
using std::string;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace sbwt_search {

vector<string> buffer_example_1 = {
  "ACgT",  // 00011011
  "gA",  // 1000
  "GAT",  // 100011
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
vector<u64> expected_bits_2 = { 0, 3, 0, 2 };

class DummyParser {
  public:
  private:
    int counter = 0;

  public:
    vector<vector<u64>> string_indexes, character_indexes,
      cumulative_character_indexes;
    vector<vector<string>> buffers;
    DummyParser(
      vector<vector<string>> &buffers,
      vector<vector<u64>> &string_indexes,
      vector<vector<u64>> &character_indexes,
      vector<vector<u64>> &cumulative_character_indexes
    ):
        buffers(buffers),
        string_indexes(string_indexes),
        cumulative_character_indexes(cumulative_character_indexes),
        character_indexes(character_indexes) {}

    auto operator>>(unique_ptr<StringSequenceBatch> &batch) -> bool {
      if (counter < string_indexes.size()) {
        batch = make_unique<StringSequenceBatch>();
        batch->buffer = buffers[counter];
        batch->string_indexes = string_indexes[counter];
        batch->character_indexes = character_indexes[counter];
        batch->cumulative_character_indexes
          = cumulative_character_indexes[counter];
        ++counter;
        return true;
      }
      return false;
    }
};

class ContinuousSeqToBitsConverterTest: public ::testing::Test {
  protected:
    vector<vector<string>> buffers;
    vector<vector<u64>> expected_bits;
    vector<vector<u64>> string_indexes = { { 0, 6 } };
    vector<vector<u64>> character_indexes = { { 0, 0 } };
    vector<vector<u64>> cumulative_character_indexes = { { 0, 47 } };

    auto shared_tests() -> void {
      auto parser = make_shared<DummyParser>(
        buffers, string_indexes, character_indexes, cumulative_character_indexes
      );
      auto host = ContinuousSeqToBitsConverter<DummyParser>(parser, 1);
      host.read_and_generate();
      vector<u64> output;
      for (int i = 0; host >> output; ++i) {
        assert_vectors_equal(expected_bits[i], output);
      }
    }
};

TEST_F(ContinuousSeqToBitsConverterTest, SingleBatch) {
  buffers = { { buffer_example_1 } };
}

TEST_F(ContinuousSeqToBitsConverterTest, MultipleBatches) {
  buffers = { buffer_example_1, buffer_example_1 };
  expected_bits = { expected_bits_1, expected_bits_1 };
  string_indexes = { { 0, 6 }, { 0, 6 } };
  character_indexes = { { 0, 0 }, { 0, 0 } };
  cumulative_character_indexes = { { 0, 47 }, { 0, 47 } };
  shared_tests();
}

TEST_F(ContinuousSeqToBitsConverterTest, TestParallel) {
  // create 2 big buffers (example by generating random characters. Buffers can
  // be a single large string) give the buffers and indexes to the test
  const uint threads = 2, iterations = 60;
  auto sleep_amount = 200;
  milliseconds::rep read_time;
  buffers = {};
  string_indexes = {};
  character_indexes = {};
  cumulative_character_indexes = {};
  for (uint i = 0; i < iterations / 2; ++i) {
    buffers.push_back(buffer_example_1);
    expected_bits.push_back(expected_bits_1);
    string_indexes.push_back({ 0, 4, 6 });
    character_indexes.push_back({ 0, 32 - 13, 0 });
    cumulative_character_indexes.push_back({ 0, 32, 47 });

    buffers.push_back(buffer_example_2);
    expected_bits.push_back(expected_bits_2);
    string_indexes.push_back({ 0, 1, 2 });
    character_indexes.push_back({ 0, 0, 0 });
    cumulative_character_indexes.push_back({ 0, 64, 120 });
  }
  auto parser = make_shared<DummyParser>(
    buffers, string_indexes, character_indexes, cumulative_character_indexes
  );
  auto host = ContinuousSeqToBitsConverter<DummyParser>(parser, 2);
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
      vector<u64> output;
      for (uint i = 0; host >> output; ++i) {
        outputs.push_back(move(output));
      };
    }
  }
  ASSERT_EQ(outputs.size(), iterations);
  for (uint i = 0; i < iterations; ++i) {
    assert_vectors_equal(expected_bits[i], outputs[i]);
  }
  ASSERT_GE(read_time, sleep_time);
}

}
