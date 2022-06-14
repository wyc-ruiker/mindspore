/**
 * Copyright 2022 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_SSPADDMM_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_SSPADDMM_CPU_KERNEL_H_

#include <unordered_map>
#include <vector>
#include <string>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class SspaddmmCPUKernelMod : public DeprecatedNativeCpuKernelMod {
 public:
  SspaddmmCPUKernelMod() = default;
  ~SspaddmmCPUKernelMod() override = default;
  void InitKernel(const CNodePtr &kernel_node) override;
  bool Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
              const std::vector<AddressPtr> &outputs) override;

 private:
  template <typename T>
  void LaunchKernel(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &outputs);

  void CheckParam(const CNodePtr &kernel_node);
  template <typename T, typename S>
  void CheckSparseIndicesLegal(void *indices_addr, void *shape_addr, size_t num, std::string x_name);
  template <typename T>
  void InitShape(void *input_shape, int64_t *y_shape);
  template <typename T>
  void ClearSparseValues(T *sparse_val, size_t data_num);
  template <typename T>
  T *ScalarSparseMul(T *sparse_val, void *scalar_val, size_t data_num, TypeId tid);
  template <typename T, typename S>
  void SparseAddSparse(void *input_indices, S *inut_values, size_t input_num, int64_t *y_indices, S *y_values,
                       size_t y_num);
  template <typename T, typename S>
  void SparseMulDense(void *mat1_indices, S *mat1_values, size_t mat1_vals_num, S *mat2_values_tensor,
                      int64_t *y_indices, S *y_values, size_t y_vals_num, int64_t row, int64_t mat2_col);

  TypeId output_values_dtype_{kTypeUnknown};
  TypeId input_indices_dtype_{kTypeUnknown};
  TypeId input_shape_dtype_{kTypeUnknown};
  TypeId mat1_indices_dtype_{kTypeUnknown};
  TypeId mat1_shape_dtype_{kTypeUnknown};
  TypeId alpha_dtype_{kTypeUnknown};
  TypeId beta_dtype_{kTypeUnknown};
  size_t input_values_num_{0};
  size_t mat1_values_num_{0};
  size_t y_values_num_{0};
  size_t cnt_{0};
  size_t mat2_row_{0};
  size_t mat2_col_{0};
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_SSPADDMM_CPU_KERNEL_H_
