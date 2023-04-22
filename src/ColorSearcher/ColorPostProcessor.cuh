#ifndef COLOR_POST_PROCESSOR_CUH
#define COLOR_POST_PROCESSOR_CUH

/**
 * @file ColorPostProcessor.cuh
 * @brief Squeezes the color results by adding the color sets of the same warp
 * together and storing the results in a new array where the color results are
 * stored contiguously
 */

#include "Global/GlobalDefinitions.h"
#include "Tools/KernelUtils.cuh"
#include "Tools/TypeDefinitions.h"
#include "hip/hip_runtime.h"

namespace sbwt_search {

using gpu_utils::get_idx;

__global__ auto d_post_process(
  const u8 *fat_results,
  const u64 *warps_before_new_read,
  const u64 num_warps,
  const u64 num_reads,
  const u64 num_colors,
  u64 *results
) -> void {
  u64 tidx = get_idx();
  if (tidx >= num_reads * num_colors) { return; }
  u64 color_idx = tidx % num_colors;
  u64 start_warp_idx = tidx / num_colors - 1;
  u64 stop_warp_idx = tidx / num_colors;
  u64 fat_results_start_idx
    = warps_before_new_read[start_warp_idx] * num_colors + color_idx;
  if (tidx / num_colors == 0) {
    start_warp_idx = 0;
    fat_results_start_idx = color_idx;
  }
  u64 fat_results_stop_idx
    = warps_before_new_read[stop_warp_idx] * num_colors + color_idx;
  if (warps_before_new_read[stop_warp_idx] == ULLONG_MAX) {
    fat_results_stop_idx = num_warps * num_colors + color_idx;
  }
  for (u64 i = fat_results_start_idx; i < fat_results_stop_idx;
       i += num_colors) {
    results[tidx] += fat_results[i];
  }
}

}  // namespace sbwt_search

#endif
