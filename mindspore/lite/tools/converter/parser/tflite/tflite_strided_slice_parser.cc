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

#include "tools/converter/parser/tflite/tflite_strided_slice_parser.h"
#include <vector>
#include <memory>

namespace mindspore {
namespace lite {
STATUS TfliteStridedSliceParser::Parse(TfliteTensorsInfo *tensors_info,
                                       const std::unique_ptr<tflite::OperatorT> &tflite_op,
                                       const std::unique_ptr<tflite::ModelT> &tflite_model,
                                       const std::unique_ptr<tflite::SubGraphT> &tflite_subgraph, schema::CNodeT *op) {
  MS_LOG(DEBUG) << "parse TfliteStridedSliceParser";
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

  std::unique_ptr<schema::StridedSliceT> attr = std::make_unique<schema::StridedSliceT>();
  if (attr == nullptr) {
    MS_LOG(ERROR) << "new op failed";
    return RET_NULL_PTR;
  }
  const auto &tflite_attr = tflite_op->builtin_options.AsStridedSliceOptions();
  if (tflite_attr == nullptr) {
    MS_LOG(ERROR) << "get op: %s attr failed", op->name.c_str();
    return RET_NULL_PTR;
  }
  attr->beginMask = tflite_attr->begin_mask;
  attr->endMask = tflite_attr->end_mask;
  attr->ellipsisMask = tflite_attr->ellipsis_mask;
  attr->newAxisMask = tflite_attr->new_axis_mask;
  attr->shrinkAxisMask = tflite_attr->shrink_axis_mask;

  int status = GetTfliteData(tflite_op->inputs[1], tflite_subgraph->tensors, tflite_model->buffers, attr->begin);
  if (status != RET_OK && status != RET_NO_CHANGE) {
    MS_LOG(ERROR) << "stridedSlice -> begin get failed";
    return RET_ERROR;
  } else if (status == RET_OK) {
    status = GetTfliteData(tflite_op->inputs[2], tflite_subgraph->tensors, tflite_model->buffers, attr->end);
    if (status != RET_OK && status != RET_NO_CHANGE) {
      MS_LOG(ERROR) << "stridedSlice -> end get failed";
      return RET_ERROR;
    } else if (status == RET_OK) {
      status = GetTfliteData(tflite_op->inputs[3], tflite_subgraph->tensors, tflite_model->buffers, attr->stride);
      if (status != RET_OK && status != RET_NO_CHANGE) {
        MS_LOG(ERROR) << "stridedSlice -> stride get failed";
        return RET_ERROR;
      }
    }
  }
  attr->isScale.assign(tflite_subgraph->tensors[tflite_op->inputs[0]]->shape.begin(),
                       tflite_subgraph->tensors[tflite_op->inputs[0]]->shape.end());

  op->primitive->value.type = schema::PrimitiveType_StridedSlice;
  op->primitive->value.value = attr.release();

  int input_num = status == RET_OK ? 1 : 4;
  for (int i = 0; i < input_num; ++i) {
    AddOpInput(op, tensors_info, tflite_op->inputs[i], tflite_subgraph->tensors.size(), schema::Format::Format_NHWC);
  }
  AddOpOutput(op, tensors_info, tflite_op->outputs[0], tflite_subgraph->tensors.size(), schema::Format::Format_NHWC);
  return RET_OK;
}
PrimitiveC *TfliteStridedSliceParser::ParseLitePrimitive(const std::unique_ptr<tflite::OperatorT> &tflite_op,
                                                         const std::unique_ptr<tflite::ModelT> &tflite_model) {
  auto &tflite_subgraph = tflite_model->subgraphs.front();
  auto primitive = std::make_unique<schema::PrimitiveT>();
  if (primitive == nullptr) {
    MS_LOG(ERROR) << "primitive is null";
    return nullptr;
  }

  std::unique_ptr<schema::StridedSliceT> attr = std::make_unique<schema::StridedSliceT>();
  if (attr == nullptr) {
    MS_LOG(ERROR) << "new op failed";
    return nullptr;
  }

  const auto &tflite_attr = tflite_op->builtin_options.AsStridedSliceOptions();
  if (tflite_attr == nullptr) {
    MS_LOG(ERROR) << "get op strideslice attr failed";
    return nullptr;
  }
  attr->beginMask = tflite_attr->begin_mask;
  attr->endMask = tflite_attr->end_mask;
  attr->ellipsisMask = tflite_attr->ellipsis_mask;
  attr->newAxisMask = tflite_attr->new_axis_mask;
  attr->shrinkAxisMask = tflite_attr->shrink_axis_mask;

  int status = GetTfliteData(tflite_op->inputs[1], tflite_subgraph->tensors, tflite_model->buffers, attr->begin);
  if (status != RET_OK && status != RET_NO_CHANGE) {
    MS_LOG(ERROR) << "stridedSlice -> begin get failed";
    return nullptr;
  } else if (status == RET_OK) {
    status = GetTfliteData(tflite_op->inputs[2], tflite_subgraph->tensors, tflite_model->buffers, attr->end);
    if (status != RET_OK && status != RET_NO_CHANGE) {
      MS_LOG(ERROR) << "stridedSlice -> end get failed";
      return nullptr;
    } else if (status == RET_OK) {
      status = GetTfliteData(tflite_op->inputs[3], tflite_subgraph->tensors, tflite_model->buffers, attr->stride);
      if (status != RET_OK && status != RET_NO_CHANGE) {
        MS_LOG(ERROR) << "stridedSlice -> stride get failed";
        return nullptr;
      }
    }
  }
  attr->isScale.assign(tflite_subgraph->tensors[tflite_op->inputs[0]]->shape.begin(),
                       tflite_subgraph->tensors[tflite_op->inputs[0]]->shape.end());

  primitive->value.type = schema::PrimitiveType_StridedSlice;
  primitive->value.value = attr.release();
  return PrimitiveC::Create(primitive.release());
}

TfliteNodeRegister g_tfliteStridedSliceParser("StridedSlice", new TfliteStridedSliceParser());
}  // namespace lite
}  // namespace mindspore
