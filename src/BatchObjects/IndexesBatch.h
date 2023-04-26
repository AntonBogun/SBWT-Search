#ifndef INDEXES_BATCH_H
#define INDEXES_BATCH_H

/**
 * @file IndexesBatch.h
 * @brief Contains the indexes of the search function which are output to disk
 * in the 'index' phase
 */

#include "Tools/PinnedVector.h"
#include "Tools/TypeDefinitions.h"

namespace sbwt_search {

using gpu_utils::PinnedVector;
using std::vector;

class IndexesBatch {
public:
  PinnedVector<u64> warped_indexes;
  PinnedVector<u64> warps_intervals;

  IndexesBatch(u64 warped_indexes_size, u64 warps_intervals_size):
      warped_indexes(warped_indexes_size),
      warps_intervals(warps_intervals_size) {}

  auto reset() -> void {
    warped_indexes.resize(0);
    warps_intervals.resize(1);
    warps_intervals[0] = 0;
  }
};

}  // namespace sbwt_search

#endif
