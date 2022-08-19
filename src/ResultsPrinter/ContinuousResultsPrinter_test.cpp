#include <climits>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "gtest/gtest_pred_impl.h"
#include <ext/alloc_traits.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest-test-part.h>

#include "BatchObjects/CumulativePropertiesBatch.h"
#include "BatchObjects/IntervalBatch.h"
#include "ResultsPrinter/ContinuousResultsPrinter.hpp"
#include "Utils/TypeDefinitions.h"

using std::getline;
using std::ifstream;
using std::make_shared;
using std::string;
using std::filesystem::remove;

namespace sbwt_search {

template <typename T>
class DummyVectorProducer {
    vector<vector<T>> v;
    uint counter = 0;

  public:
    DummyVectorProducer(vector<vector<T>> v): v(v) {}

    auto operator>>(shared_ptr<vector<T>> &out) -> bool {
      if (counter == v.size()) { return false; }
      out = make_shared<vector<T>>(v[counter]);
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

    auto operator>>(shared_ptr<IntervalBatch> &out) -> bool {
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
    vector<vector<char>> invalid_chars = { {
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
      = { { "", "", "10 -1", "", "", "30 40 -2 -2", "" },
          {},
          { "", "" },
          { "70", "", "-2 -2 100" } };
    // NOTE: at the end of each string there is a linefeed (\n) character

    auto get_results_producer() -> shared_ptr<DummyVectorProducer<u64>> {
      return make_shared<DummyVectorProducer<u64>>(results);
    }
    auto get_invalid_producer() -> shared_ptr<DummyVectorProducer<char>> {
      return make_shared<DummyVectorProducer<char>>(invalid_chars);
    }
    auto get_interval_producer() -> shared_ptr<DummyIntervalProducer> {
      return make_shared<DummyIntervalProducer>(
        string_lengths, strings_before_newfile
      );
    }
    auto shared_tests() {
      auto host = ContinuousResultsPrinter<
        DummyVectorProducer<u64>,
        DummyIntervalProducer,
        DummyVectorProducer<char>>(
        get_results_producer(),
        get_interval_producer(),
        get_invalid_producer(),
        filenames,
        kmer_size
      );
      host.read_and_generate();
      for (uint file_index = 0; file_index < filenames.size(); ++file_index) {
        ifstream stream(filenames[file_index]);
        string line;
        for (uint line_index = 0; getline(stream, line); ++line_index) {
          ASSERT_EQ(expected_file_lines[file_index][line_index], line)
            << " unequal at index " << file_index << ":" << line_index << '\n';
        }
      }
    }
    auto TearDown() -> void override {
      for (auto &filename: filenames) { remove(filename); }
    }
};

TEST_F(ContinuousResultsPrinterTest, SingleBatch) { shared_tests(); }

TEST_F(ContinuousResultsPrinterTest, MultipleBatches) {
  results = { { 10, ULLONG_MAX }, { 30, 40, 50, 60, 70 }, { 80, 90, 100 } };
  invalid_chars = { { 0, 0, 0, 0 },  // end of first string
                    { 0, 0, 0, 0, 1, 0, 0, 0, 0 },  // end of third string
                    { 0, 1, 0, 0, 0 } };  // end of last string
  string_lengths
    = { { 0, 0, 2 + 2, 0, 0 }, { 4 + 2, 0, 0, 0, 1 + 2, 0 }, { 3 + 2 } };
  strings_before_newfile
    = { { ULLONG_MAX }, { 2, 0, 2, ULLONG_MAX }, { 1, ULLONG_MAX } };
  shared_tests();
}
}  // namespace sbwt_search
