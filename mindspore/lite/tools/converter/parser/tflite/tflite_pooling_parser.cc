/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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

#include "tools/converter/parser/tflite/tflite_pooling_parser.h"
#include <vector>
#include <memory>
#include <string>

namespace mindspore::lite {
STATUS TflitePoolingParser::Parse(TfliteTensorsInfo *tensors_info, const std::unique_ptr<tflite::OperatorT> &tflite_op,
                                  const std::unique_ptr<tflite::ModelT> &tflite_model,
                                  const std::unique_ptr<tflite::SubGraphT> &tflite_subgraph, schema::CNodeT *op) {
  MS_ASSERT(tflite_op != nullptr);
  MS_ASSERT(tflite_model != nullptr);
  MS_ASSERT(tflite_subgraph != nullptr);
  if (op == nullptr) {
    MS_LOG(ERROR) << "op is null";
    return RET_NULL_PTR;
  }
  op->primitive = std::make_unique<schema::PrimitiveT>();
  if (op->primitive == nullptr) {
    MS_LOG(ERROR) << "op->primitive is null";
    return RET_NULL_PTR;
  }

  std::unique_ptr<schema::PoolingT> attr = std::make_unique<schema::PoolingT>();
  if (attr == nullptr) {
    MS_LOG(ERROR) << "new op failed";
    return RET_NULL_PTR;
  }

  auto tflite_op_type = (tflite_model->operator_codes[tflite_op->opcode_index])->builtin_code;
  if (tflite_op_type == tflite::BuiltinOperator_AVERAGE_POOL_2D) {
    attr->poolingMode = schema::PoolMode_MEAN_POOLING;
  } else if (tflite_op_type == tflite::BuiltinOperator_MAX_POOL_2D) {
    attr->poolingMode = schema::PoolMode_MAX_POOLING;
  } else {
    MS_LOG(ERROR) << "pooling mode " << tflite_op_type << " hasn't been supported";
    return RET_NOT_FIND_OP;
  }

  const auto &tflite_attr = tflite_op->builtin_options.AsPool2DOptions();
  if (tflite_attr == nullptr) {
    MS_LOG(ERROR) << "get op: " << op->name.c_str() << " attr failed";
    return RET_NULL_PTR;
  }
  attr->windowW = tflite_attr->filter_width;
  attr->windowH = tflite_attr->filter_height;
  attr->strideW = tflite_attr->stride_w;
  attr->strideH = tflite_attr->stride_h;
  attr->padMode = GetPadMode(tflite_attr->padding);
  attr->format = schema::Format::Format_NHWC;
  attr->global = false;
  attr->roundMode = schema::RoundMode_FLOOR;
  attr->activationType = GetActivationFunctionType(tflite_attr->fused_activation_function);

  // calculate pad params
  auto data_index = tflite_op->inputs[0];
  const auto &data_tensor = tflite_subgraph->tensors[data_index];
  std::vector<int64_t> params;
  int status =
    getPaddingParam(data_tensor, attr->padMode, attr->strideH, attr->strideW, attr->windowH, attr->windowW, &params);
  if (status != RET_OK && status != RET_NO_CHANGE) {
    MS_LOG(ERROR) << "get padding params failed";
    return RET_ERROR;
  } else if (status == RET_OK) {
    attr->padUp = params.at(0);
    attr->padDown = params.at(1);
    attr->padLeft = params.at(2);
    attr->padRight = params.at(3);
  }

  op->primitive->value.type = schema::PrimitiveType_Pooling;
  op->primitive->value.value = attr.release();

  AddOpInput(op, tensors_info, tflite_op->inputs[0], tflite_subgraph->tensors.size(), schema::Format::Format_NHWC);
  AddOpOutput(op, tensors_info, tflite_op->outputs[0], tflite_subgraph->tensors.size(), schema::Format::Format_NHWC);
  return RET_OK;
}
lite::PrimitiveC *TflitePoolingParser::ParseLitePrimitive(const std::unique_ptr<tflite::OperatorT> &tflite_op,
                                                          const std::unique_ptr<tflite::ModelT> &tflite_model) {
  const auto &tflite_subgraph = tflite_model->subgraphs.front();
  std::unique_ptr<schema::PoolingT> attr = std::make_unique<schema::PoolingT>();
  if (attr == nullptr) {
    MS_LOG(ERROR) << "new op failed";
    return nullptr;
  }

  auto tflite_op_type = (tflite_model->operator_codes[tflite_op->opcode_index])->builtin_code;
  if (tflite_op_type == tflite::BuiltinOperator_AVERAGE_POOL_2D) {
    attr->poolingMode = schema::PoolMode_MEAN_POOLING;
  } else if (tflite_op_type == tflite::BuiltinOperator_MAX_POOL_2D) {
    attr->poolingMode = schema::PoolMode_MAX_POOLING;
  }
  const auto &tflite_attr = tflite_op->builtin_options.AsPool2DOptions();
  if (tflite_attr == nullptr) {
    MS_LOG(ERROR) << "get op pooling attr failed";
    return nullptr;
  }
  attr->windowW = tflite_attr->filter_width;
  attr->windowH = tflite_attr->filter_height;
  attr->strideW = tflite_attr->stride_w;
  attr->strideH = tflite_attr->stride_h;
  attr->padMode = GetPadMode(tflite_attr->padding);
  attr->format = schema::Format::Format_NHWC;

  attr->global = false;
  attr->roundMode = schema::RoundMode_FLOOR;
  attr->activationType = GetActivationFunctionType(tflite_attr->fused_activation_function);

  // calculate pad params
  auto data_index = tflite_op->inputs[0];
  const auto &data_tensor = tflite_subgraph->tensors[data_index];
  std::vector<int64_t> params;
  int status =
    getPaddingParam(data_tensor, attr->padMode, attr->strideH, attr->strideW, attr->windowH, attr->windowW, &params);
  if (status != RET_OK && status != RET_NO_CHANGE) {
    MS_LOG(ERROR) << "get padding params failed";
    return nullptr;
  } else if (status == RET_OK) {
    attr->padUp = params.at(0);
    attr->padDown = params.at(1);
    attr->padLeft = params.at(2);
    attr->padRight = params.at(3);
  }
  auto primitive = std::make_unique<schema::PrimitiveT>();
  primitive->value.type = schema::PrimitiveType_Pooling;
  primitive->value.value = attr.release();
  return PrimitiveC::Create(primitive.release());
}

TfliteNodeRegister g_tfliteMeanPoolingParser("MeanPooling", new TflitePoolingParser());
TfliteNodeRegister g_tfliteMaxPoolingParser("MaxPooling", new TflitePoolingParser());
}  // namespace mindspore::lite
