#include <chrono>
#include <memory>
#include <thread>

#include "gtest/gtest.h"

#include "BatchObjects/StringBreakBatch.h"
#include "PositionsBuilder/ContinuousPositionsBuilder.hpp"
#include "Utils/RNGUtils.h"

using rng_utils::get_uniform_generator;
using std::make_shared;
using std::shared_ptr;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace sbwt_search {

class DummyStringBreakBatchProducer {
  private:
    int counter = 0;
    vector<vector<size_t>> string_breaks;
    vector<size_t> string_sizes;

  public:
    DummyStringBreakBatchProducer(
      vector<vector<size_t>> &_string_breaks, vector<size_t> _string_sizes
    ):
        string_breaks(_string_breaks), string_sizes(_string_sizes) {}

    auto operator>>(shared_ptr<StringBreakBatch> &batch) -> bool {
      if (counter < string_breaks.size()) {
        batch = make_shared<StringBreakBatch>();
        batch->string_breaks = &string_breaks[counter];
        batch->string_size = string_sizes[counter];
        ++counter;
        return true;
      }
      return false;
    }
};

class ContinuousPositionsBuilderTest: public ::testing::Test {
  protected:
    vector<vector<u64>> cumsum_string_lengths, cumsum_positions_per_string,
      expected_positions;

    auto run_test(
      uint kmer_size,
      vector<vector<size_t>> string_breaks,
      vector<size_t> string_sizes,
      vector<vector<size_t>> expected_positions,
      uint max_batches = 7
    ) {
      auto max_chars_per_batch = 999;
      auto producer = make_shared<DummyStringBreakBatchProducer>(
        string_breaks, string_sizes
      );
      auto host = ContinuousPositionsBuilder<DummyStringBreakBatchProducer>(
        producer, kmer_size, max_chars_per_batch, max_batches
      );
      size_t expected_batches = string_breaks.size();
      size_t batches = 0;
#pragma omp parallel sections private(batches)
      {
#pragma omp section
        {
          auto rng = get_uniform_generator(0, 200);
          sleep_for(milliseconds(rng()));
          host.read_and_generate();
        }
#pragma omp section
        {
          auto rng = get_uniform_generator(0, 200);
          shared_ptr<PositionsBatch> positions_batch;
          for (batches = 0; host >> positions_batch; ++batches) {
            sleep_for(milliseconds(rng()));
            EXPECT_EQ(expected_positions[batches], positions_batch->positions);
          }
          EXPECT_EQ(batches, expected_batches);
        }
      }
    }
};

TEST_F(ContinuousPositionsBuilderTest, Basic) {
  vector<vector<size_t>> string_breaks = { { 7, 8, 10, 15 }, { 7, 8, 10, 15 } };
  vector<size_t> string_sizes = { 20, 15 };
  vector<vector<size_t>> expected_positions
    = { { 0, 1, 2, 3, 4, 5, 11, 12, 13, 16, 17 },
        { 0, 1, 2, 3, 4, 5, 11, 12, 13 } };
  uint kmer_size = 3;
  for (auto max_batches: { 1, 2, 3, 7 }) {
    run_test(
      kmer_size, string_breaks, string_sizes, expected_positions, max_batches
    );
  }
}

}  // namespace sbwt_search
