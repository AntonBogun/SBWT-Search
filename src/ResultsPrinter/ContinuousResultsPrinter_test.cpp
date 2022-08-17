#include <climits>
#include <memory>

#include <gtest/gtest.h>

#include "BatchObjects/CumulativePropertiesBatch.hpp"
#include "BatchObjects/IntervalBatch.hpp"
#include "BatchObjects/StringSequenceBatch.hpp"
#include "ResultsPrinter/ContinuousResultsPrinter.hpp"
#include "Utils/TypeDefinitions.h"

using std::make_shared;

namespace sbwt_search {

class DummyVectorProducer {
    vector<vector<u64>> v;
    uint counter = 0;

  public:
    DummyVectorProducer(vector<vector<u64>> v): v(v) {}

    bool operator>>(shared_ptr<vector<u64>> &out) {
      if (counter == v.size()) { return false; }
      out = make_shared<vector<u64>>(v[counter]);
      ++counter;
      return true;
    }
};

class DummyIntervalProducer {
    vector<vector<u64>> string_lengths, strings_before_newfile;
    int counter = 0;

  public:
    DummyIntervalProducer(
      vector<vector<u64>> string_lengths,
      vector<vector<u64>> strings_before_newfile
    ):
        string_lengths(string_lengths),
        strings_before_newfile(strings_before_newfile) {}

    bool operator>>(shared_ptr<IntervalBatch> &out) {
      if (counter == string_lengths.size()) { return false; }
      out = make_shared<IntervalBatch>();
      out->string_lengths = string_lengths[counter];
      out->strings_before_newfile = strings_before_newfile[counter];
      ++counter;
      return true;
    }
};

/*
Simulating the following 4 files, kmer_size = 3:
  File 1:
    --empty--
    --empty--
    10 not_found | valid valid
    --empty--
    --empty--
    30 40 50 60 | invalid valid
    --empty--
  File 2 is completely empty:
  File 3:
    --empty--
    --empty--
  File 4:
    70 | valid  valid
    --empty--
    80 invalid 100 | valid valid
*/

class ContinuousResultsPrinterTest: public ::testing::Test {
  protected:
    const uint kmer_size = 3;
    vector<vector<u64>> results
      = { { 10, ULLONG_MAX, 30, 40, 50, 60, 70, 80, 90, 100 } };
    vector<vector<u64>> invalid_chars = { {
      0,
      0,
      0,
      0,  // end of first string
      0,
      0,
      0,
      0,
      1,
      0,  // end of second string
      0,
      0,
      0,  // end of third string
      0,
      1,
      0,
      0,
      0  // end of last string
    } };
    vector<vector<u64>> string_lengths
      = { { 0, 0, 2 + 2, 0, 0, 4 + 2, 0, 0, 0, 1 + 2, 0, 3 + 2 } };
    vector<vector<u64>> strings_before_newfile = { { 7, 0, 2, 3, ULLONG_MAX } };
    vector<string> filenames = { "tmp/results_file1.txt",
                                 "tmp/results_file2.txt",
                                 "tmp/results_file3.txt",
                                 "tmp/results_file4.txt" };
    vector<vector<string>> expected_file_lines
      = { { "\n", "\n", "10 -1\n", "\n", "\n", "30 40 -2 -2\n", "\n" },
          {},
          { "\n", "\n" },
          { "70\n", "\n", "-2 -2 100\n" } };

    auto get_results_producer() -> DummyVectorProducer {
      return DummyVectorProducer(results);
    }
    auto get_invalid_producer() -> DummyVectorProducer {
      return DummyVectorProducer(invalid_chars);
    }
    auto get_interval_producer() -> DummyIntervalProducer {
      return DummyIntervalProducer(string_lengths, strings_before_newfile);
    }
    auto shared_tests() {
      auto host = ContinuousResultsPrinter<
        DummyVectorProducer,
        DummyIntervalProducer,
        DummyVectorProducer>(
        get_results_producer(),
        get_interval_producer(),
        get_invalid_producer(),
        filenames,
        kmer_size
      );
      host.read_and_generate();
      // DO ASSERTS HERE
    }
};

TEST_F(ContinuousResultsPrinterTest, SingleBatch) { shared_tests(); }
}
