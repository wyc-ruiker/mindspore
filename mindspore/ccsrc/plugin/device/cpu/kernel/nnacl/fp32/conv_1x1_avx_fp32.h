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
#ifndef MINDSPORE_NNACL_FP32_CONV_1X1_AVX_FP32_H_
#define MINDSPORE_NNACL_FP32_CONV_1X1_AVX_FP32_H_

#ifdef ENABLE_AVX
#include "nnacl/op_base.h"
#include "nnacl/conv_parameter.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef void (*Conv1x1SWAVXKernel)(float *dst, const float *src, const float *weight, const float *bias,
                                   size_t act_flag, size_t ow_block, size_t oc_block, size_t oc_align, size_t ic_align,
                                   size_t in_sw_step, size_t dst_flag);

void Conv1x1SWAVXFp32(const float *input_data, const float *packed_weight, const float *bias_data, float *output_data,
                      int task_id, ConvParameter *conv_param, SlidingWindowParam *sw_param);

#ifdef ENABLE_DEBUG
void Conv1x1SWOWxOCAVXKernel(float *dst, const float *src, const float *weight, const float *bias, size_t act_flag,
                             size_t ow_block, size_t oc_block, size_t oc_align, size_t ic_align, size_t in_sw_step,
                             size_t dst_flag);
#endif

#endif
#ifdef __cplusplus
}
#endif
#endif  // MINDSPORE_NNACL_FP32_CONV_1X1_AVX_FP32_H_
