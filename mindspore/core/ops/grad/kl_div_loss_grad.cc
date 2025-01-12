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

#include "ops/grad/kl_div_loss_grad.h"
#include <set>
#include <map>
#include <vector>
#include "mindapi/ir/type.h"
#include "utils/check_convert_utils.h"
#include "ops/op_utils.h"
#include "mindapi/src/helper.h"

namespace mindspore {
namespace ops {
std::string KLDivLossGrad::get_reduction() const { return GetValue<std::string>(GetAttr(ops::kReduction)); }

abstract::ShapePtr KLDivLossGradInferShape(const PrimitivePtr &primitive,
                                           const std::vector<AbstractBasePtr> &input_args) {
  auto op_name = primitive->name();
  auto x_shape = input_args[kInputIndex1]->BuildShape();
  auto target_shape = input_args[kInputIndex2]->BuildShape();
  auto x_shape_ptr = x_shape->cast<abstract::ShapePtr>();
  auto target_shape_ptr = target_shape->cast<abstract::ShapePtr>();
  if (!x_shape_ptr->IsDynamic() && !target_shape_ptr->IsDynamic()) {
    if (*x_shape != *target_shape) {
      MS_EXCEPTION(ValueError)
        << "For " << op_name
        << ", evaluator arg 'label' shape must be consistent with 'logits' shape, but got 'label' shape: "
        << target_shape->ToString() << ", 'logits' shape: " << x_shape->ToString() << ".";
    }
  }

  return x_shape_ptr;
}

TypePtr KLDivLossGradInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  auto op_name = prim->name();
  const std::set<TypePtr> valid_types = {kFloat16, kFloat32, kFloat64};
  auto input_grad_type = input_args[kInputIndex0]->BuildType();
  auto input_x_type = input_args[kInputIndex1]->BuildType();
  auto input_target_type = input_args[kInputIndex2]->BuildType();
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x", input_x_type, valid_types, op_name);

  std::map<std::string, TypePtr> types;
  (void)types.emplace("grad", input_grad_type);
  (void)types.emplace("x", input_x_type);
  (void)types.emplace("target", input_target_type);
  (void)CheckAndConvertUtils::CheckTensorTypeSame(types, valid_types, op_name);
  return input_x_type;
}

MIND_API_OPERATOR_IMPL(KLDivLossGrad, BaseOperator);
AbstractBasePtr KLDivLossGradInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                   const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t kInputsNum = 3;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kInputsNum, primitive->name());
  auto infer_shape = KLDivLossGradInferShape(primitive, input_args);
  auto infer_type = KLDivLossGradInferType(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}
REGISTER_PRIMITIVE_EVAL_IMPL(KLDivLossGrad, prim::kPrimKLDivLossGrad, KLDivLossGradInfer, nullptr, true);
}  // namespace ops
}  // namespace mindspore
