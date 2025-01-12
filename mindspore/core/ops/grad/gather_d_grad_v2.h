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
#ifndef MINDSPORE_CORE_OPS_GATHER_D_GRAD_V2_H_
#define MINDSPORE_CORE_OPS_GATHER_D_GRAD_V2_H_
#include <vector>
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameGatherDGradV2 = "GatherDGradV2";
class MIND_API GatherDGradV2 : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(GatherDGradV2);
  GatherDGradV2() : BaseOperator(kNameGatherDGradV2) { InitIOName({"x", "index", "grad"}, {"output"}); }
  void Init(int64_t dim = 0);
  void set_dim(int64_t dim);
  int64_t get_dim() const;
};
abstract::AbstractBasePtr GatherDGradV2Infer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                             const std::vector<abstract::AbstractBasePtr> &input_args);
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_GATHER_D_GRAD_V2_H_
