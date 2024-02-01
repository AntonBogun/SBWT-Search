#include <memory>
#include <utility>
#include <vector>

#include "SbwtContainer/GpuSbwtContainer.h"
#include "Tools/GpuPointer.h"
#include "Tools/TypeDefinitions.h"

using gpu_utils::GpuPointer;
using std::make_unique;
using std::unique_ptr;
using std::vector;

namespace sbwt_search {

GpuSbwtContainer::GpuSbwtContainer(
  const vector<vector<u64>> &cpu_acgt,
  const vector<Poppy> &cpu_poppy,
  const vector<u64> &cpu_c_map,
  u64 bits_total,
  u64 bit_vector_size,
  u32 kmer_size,
  const vector<u64> &cpu_key_kmer_marks
):
    SbwtContainer(bits_total, bit_vector_size, kmer_size),
    max_index(bits_total) {
  acgt.reserve(4);
  for (u64 i = 0; i < 4; ++i) {
    acgt.push_back(make_unique<GpuPointer<u64>>(cpu_acgt[i]));
    layer_0.push_back(make_unique<GpuPointer<u64>>(cpu_poppy[i].layer_0));
    layer_1_2.push_back(make_unique<GpuPointer<u64>>(cpu_poppy[i].layer_1_2));
  }
  c_map = make_unique<GpuPointer<u64>>(cpu_c_map);
  acgt_pointers = make_unique<GpuPointer<u64 *>>(vector<u64 *>(
    {acgt[0]->data(), acgt[1]->data(), acgt[2]->data(), acgt[3]->data()}
  ));
  layer_0_pointers = make_unique<GpuPointer<u64 *>>(vector<u64 *>(
    {layer_0[0]->data(),
     layer_0[1]->data(),
     layer_0[2]->data(),
     layer_0[3]->data()}
  ));
  layer_1_2_pointers = make_unique<GpuPointer<u64 *>>(vector<u64 *>(
    {layer_1_2[0]->data(),
     layer_1_2[1]->data(),
     layer_1_2[2]->data(),
     layer_1_2[3]->data()}
  ));
  key_kmer_marks = make_unique<GpuPointer<u64>>(cpu_key_kmer_marks);
}

auto GpuSbwtContainer::get_max_index() const -> const u64 { return max_index; }

auto GpuSbwtContainer::get_c_map() const -> const GpuPointer<u64> & {
  return *c_map;
}

auto GpuSbwtContainer::get_acgt_pointers() const -> const GpuPointer<u64 *> & {
  return *acgt_pointers;
};

auto GpuSbwtContainer::get_layer_0_pointers() const
  -> const GpuPointer<u64 *> & {
  return *layer_0_pointers;
}

auto GpuSbwtContainer::get_layer_1_2_pointers() const
  -> const GpuPointer<u64 *> & {
  return *layer_1_2_pointers;
}

auto GpuSbwtContainer::set_presearch(
  unique_ptr<GpuPointer<u64>> left, unique_ptr<GpuPointer<u64>> right
) -> void {
  presearch_left = std::move(left);
  presearch_right = std::move(right);
}

auto GpuSbwtContainer::get_presearch_left() const -> GpuPointer<u64> & {
  return *presearch_left;
}

auto GpuSbwtContainer::get_presearch_right() const -> GpuPointer<u64> & {
  return *presearch_right;
}

auto GpuSbwtContainer::get_key_kmer_marks() const -> GpuPointer<u64> & {
  return *key_kmer_marks;
}

}  // namespace sbwt_search
