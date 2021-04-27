/**
 * Copyright 2019 Huawei Technologies Co., Ltd
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
#include "tools/converter/parser/tflite/tflite_model_parser.h"
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <utility>
#include "tools/converter/converter_flags.h"
#include "src/common/file_utils.h"
#include "ops/return.h"
#include "ops/make_tuple.h"
#include "ops/tuple_get_item.h"
#include "ops/primitive_c.h"
#include "ir/func_graph.h"

namespace mindspore::lite {
std::unique_ptr<tflite::ModelT> TfliteModelParser::ReadTfliteModel(const char *model_path) {
  size_t size = 0;
  tflite_model_buf_ = ReadFile(model_path, &size);
  if (tflite_model_buf_ == nullptr) {
    MS_LOG(ERROR) << "the file buffer is nullptr";
    return nullptr;
  }
  flatbuffers::Verifier verify((const uint8_t *)tflite_model_buf_, size);
  if (!tflite::VerifyModelBuffer(verify)) {
    MS_LOG(ERROR) << "the buffer is invalid and fail to create graph";
    return nullptr;
  }
  return tflite::UnPackModel(tflite_model_buf_);
}

int TfliteModelParser::ParseToFuncGraph(const std::string &model_file, const std::string &weight_file,
                                        const QuantType &quant_type) {
  // load graph
  tflite_model_ = ReadTfliteModel(model_file.c_str());
  if (tflite_model_ == nullptr) {
    MS_LOG(ERROR) << "read tflite model failed";
    ReturnCode::GetSingleReturnCode()->UpdateReturnCode(RET_GRAPH_FILE_ERR);
    return RET_GRAPH_FILE_ERR;
  }

  if (tflite_model_->subgraphs.size() != 1) {
    MS_LOG(ERROR) << "read tflite model subgraphs failed";
    ReturnCode::GetSingleReturnCode()->UpdateReturnCode(RET_GRAPH_FILE_ERR);
    return RET_GRAPH_FILE_ERR;
  }
  res_graph_ = std::make_shared<FuncGraph>();
  res_graph_->set_attr("fmk", MakeValue(static_cast<int>(converter::FmkType_TFLITE)));

  auto status = ConvertGraphInputs();
  if (status != RET_OK) {
    MS_LOG(ERROR) << "Convert graph inputs failed.";
    ReturnCode::GetSingleReturnCode()->UpdateReturnCode(status);
    return status;
  }

  status = ConvertOps();
  if (status != RET_OK) {
    MS_LOG(ERROR) << "Convert ops failed.";
    ReturnCode::GetSingleReturnCode()->UpdateReturnCode(status);
    return status;
  }

  status = ConvertGraphOutputs();
  if (status != RET_OK) {
    MS_LOG(ERROR) << "Convert graph outputs failed.";
    ReturnCode::GetSingleReturnCode()->UpdateReturnCode(status);
    return status;
  }
  res_graph_->set_attr("graph_name", MakeValue("main_graph"));
  return RET_OK;
}

std::string GetTensorName(size_t index, const tflite::BuiltinOperator &op_type, const std::string &op_name) {
  std::string tensor_name = op_name + "/input-" + std::to_string(index);
  if (op_type == tflite::BuiltinOperator_CONV_2D || op_type == tflite::BuiltinOperator_TRANSPOSE_CONV ||
      op_type == tflite::BuiltinOperator_DEPTHWISE_CONV_2D || op_type == tflite::BuiltinOperator_FULLY_CONNECTED) {
    if (index == 1) {
      tensor_name = op_name + "/weight";
    }
    if (index == 2) {
      tensor_name = op_name + "/bias";
    }
  }
  return tensor_name;
}

STATUS TfliteModelParser::ConvertOps() {
  const auto &tflite_subgraph = tflite_model_->subgraphs.front();
  NotSupportOp::GetInstance()->set_fmk_type("TFLITE");
  STATUS status = RET_OK;
  int op_idx = 0;
  for (auto &op : tflite_subgraph->operators) {
    auto tflite_op_type = (tflite_model_->operator_codes[op->opcode_index])->builtin_code;
    auto op_type = GetMSOpType(tflite_op_type);
    auto op_name = op_type + "-" + std::to_string(op_idx);
    op_idx++;
    // parse primitive
    MS_LOG(INFO) << "parse node :" << op_name;
    auto node_parser = TfliteNodeParserRegistry::GetInstance()->GetNodeParser(tflite_op_type);
    if (node_parser == nullptr) {
      NotSupportOp::GetInstance()->InsertOp(op_type);
      status = (status == RET_OK ? RET_NOT_FIND_OP : status);
      continue;
    }
    if (status != RET_OK) {
      continue;
    }

    std::vector<AnfNodePtr> op_inputs;
    auto primitiveC = node_parser->Parse(op, tflite_model_);
    if (primitiveC != nullptr) {
      op_inputs = {NewValueNode(std::shared_ptr<ops::PrimitiveC>(primitiveC))};
    } else {
      MS_LOG(ERROR) << "parse failed for node: " << op_name;
      return RET_ERROR;
    }

    status = ConvertOpQuantParams(op.get(), primitiveC);
    if (status != RET_OK) {
      MS_LOG(ERROR) << "convert " << op_name << " quant param failed.";
      continue;
    }

    // parse inputs
    for (int i = 0; i < static_cast<int>(op->inputs.size()); i++) {
      auto input_idx = op->inputs.at(i);
      if (tflite_op_type == tflite::BuiltinOperator_FULLY_CONNECTED && input_idx == -1) {
        continue;
      }
      if (input_idx < 0) {
        input_idx += tflite_subgraph->tensors.size();
      }
      const auto &input_tensor = tflite_subgraph->tensors[input_idx];
      if (nodes_.find(input_idx) != nodes_.end()) {
        op_inputs.emplace_back(nodes_.at(input_idx));
        continue;
      }

      // const tensor
      std::string tensor_name;
      if (!input_tensor->name.empty()) {
        tensor_name = input_tensor->name;
      } else {
        tensor_name = GetTensorName(i, tflite_op_type, op_name);
      }
      auto parameter = res_graph_->add_parameter();
      status = ConvertConstTensor(input_tensor.get(), parameter, tensor_name);
      if (status != RET_OK) {
        MS_LOG(ERROR) << "convert " << op_name << " node: " << input_idx << " const node failed.";
        continue;
      }
      parameter->set_name(tensor_name);
      op_inputs.emplace_back(parameter);
      nodes_.insert(std::pair(input_idx, parameter));
    }
    auto new_cnode = res_graph_->NewCNode(op_inputs);
    new_cnode->set_fullname_with_scope(op_name);

    // parse outputs
    status = ConvertOutputTensor(op.get(), new_cnode);
    if (status != RET_OK) {
      MS_LOG(ERROR) << "Convert output tensors for " << new_cnode->fullname_with_scope() << " failed.";
      continue;
    }
  }
  return status;
}

STATUS TfliteModelParser::SetTensorQuantParam(const tflite::TensorT *tflite_tensor,
                                              std::vector<QuantParamT> *quant_params, int round_type) {
  if (tflite_tensor == nullptr) {
    MS_LOG(ERROR) << "tflite_tensor is null, set tensor quant params failed.";
    return RET_NULL_PTR;
  }
  quant_params->clear();

  if (tflite_tensor->quantization == nullptr ||
      (tflite_tensor->quantization->scale.empty() && tflite_tensor->quantization->zero_point.empty() &&
       tflite_tensor->quantization->min.empty() && tflite_tensor->quantization->max.empty())) {
    std::vector<schema::QuantParamT> notinited_quant_params(1);
    *quant_params = notinited_quant_params;
    return RET_OK;
  }

  for (size_t i = 0; i < tflite_tensor->quantization->scale.size(); i++) {
    std::unique_ptr<schema::QuantParamT> quant_param = std::make_unique<QuantParamT>();
    if (quant_param == nullptr) {
      MS_LOG(ERROR) << "new quant_param failed";
      return RET_NULL_PTR;
    }

    if (!tflite_tensor->quantization->scale.empty()) {
      quant_param->scale = tflite_tensor->quantization->scale[i];
    }

    if (!tflite_tensor->quantization->zero_point.empty()) {
      quant_param->zeroPoint = tflite_tensor->quantization->zero_point[i];
    }

    if (!tflite_tensor->quantization->min.empty()) {
      quant_param->min = tflite_tensor->quantization->min[i];
    }

    if (!tflite_tensor->quantization->max.empty()) {
      quant_param->max = tflite_tensor->quantization->max[i];
    }
    quant_param->inited = true;
    quant_param->roundType = round_type;
    quant_param->multiplier = 1;
    quant_params->emplace_back(*std::move(quant_param));
  }
  return RET_OK;
}

STATUS TfliteModelParser::ConvertOpQuantParams(const tflite::OperatorT *op, ops::PrimitiveC *primitive_c) {
  if (op == nullptr) {
    MS_LOG(ERROR) << "tflite op is null, get quant params failed.";
    return RET_NULL_PTR;
  }

  if (primitive_c == nullptr) {
    MS_LOG(ERROR) << "primitive_c is null, get quant params failed.";
    return RET_NULL_PTR;
  }

  int round_type = 1;
  if (primitive_c->name() == "Conv2D" || primitive_c->name() == "Conv2DFusion") {
    round_type = 2;
  }
  const auto &tflite_subgraph = tflite_model_->subgraphs.front();
  auto quant_params_holder = std::make_shared<QuantParamHolder>(op->inputs.size(), op->outputs.size());
  size_t idx = 0;
  for (auto input_idx : op->inputs) {
    if (input_idx < 0) {
      input_idx += tflite_subgraph->tensors.size();
    }
    const auto &input_tensor = tflite_subgraph->tensors[input_idx];
    std::vector<schema::QuantParamT> quant_params;
    auto status = SetTensorQuantParam(input_tensor.get(), &quant_params, round_type);
    if (status != RET_OK) {
      MS_LOG(ERROR) << "set input tensor quant param failed.";
      return status;
    }
    quant_params_holder->set_input_quant_param(idx, quant_params);
    idx++;
  }
  idx = 0;
  for (auto output_idx : op->outputs) {
    if (output_idx < 0) {
      output_idx += tflite_subgraph->tensors.size();
    }
    const auto &output_tensor = tflite_subgraph->tensors.at(output_idx);
    std::vector<schema::QuantParamT> quant_params;
    auto status = SetTensorQuantParam(output_tensor.get(), &quant_params, round_type);
    if (status != RET_OK) {
      MS_LOG(ERROR) << "set output tensor quant param failed.";
      return status;
    }
    quant_params_holder->set_output_quant_param(idx, quant_params);
    idx++;
  }
  primitive_c->AddAttr("quant_params", quant_params_holder);
  return RET_OK;
}

STATUS TfliteModelParser::ConvertGraphInputs() {
  const auto &tflite_subgraph = tflite_model_->subgraphs.front();
  for (auto tflite_graph_input : tflite_subgraph->inputs) {
    if (tflite_graph_input < 0) {
      tflite_graph_input = tflite_graph_input + tflite_subgraph->tensors.size();
    }
    auto parameter = res_graph_->add_parameter();
    const auto &tensor = tflite_subgraph->tensors.at(tflite_graph_input);
    std::vector<int64_t> shape_vector;
    (void)std::transform(tensor->shape.begin(), tensor->shape.end(), std::back_inserter(shape_vector),
                         [](const int32_t &value) { return static_cast<int64_t>(value); });
    auto abstract_tensor = CreateTensorAbstract(shape_vector, GetTfliteDataType(tensor->type));
    if (abstract_tensor == nullptr) {
      MS_LOG(ERROR) << "Create tensor abstarct failed";
      return RET_ERROR;
    }
    parameter->set_abstract(abstract_tensor);
    parameter->set_name("graph_input-" + std::to_string(tflite_graph_input));
    nodes_.insert(std::pair(tflite_graph_input, parameter));
  }
  return RET_OK;
}

STATUS TfliteModelParser::ConvertGraphOutputs() {
  const auto &tflite_subgraph = tflite_model_->subgraphs.front();
  if (tflite_subgraph->outputs.size() > 1) {
    std::vector<AnfNodePtr> make_tuple_inputs;
    auto make_tuple_prim_ptr = std::make_shared<ops::MakeTuple>();
    if (make_tuple_prim_ptr == nullptr) {
      MS_LOG(ERROR) << "new MakeTuple failed";
      return RET_NULL_PTR;
    }
    auto make_tuple_prim = NewValueNode(make_tuple_prim_ptr);
    make_tuple_inputs.emplace_back(make_tuple_prim);
    for (auto outputNode : tflite_subgraph->outputs) {
      outputNode = outputNode < 0 ? outputNode + tflite_subgraph->tensors.size() : outputNode;
      auto cnode = nodes_.at(outputNode);
      if (nullptr == cnode) {
        MS_LOG(ERROR) << "Can't find input node.";
        return RET_NOT_FIND_OP;
      }
      make_tuple_inputs.emplace_back(cnode);
    }
    auto make_tuple_cnode = res_graph_->NewCNode(make_tuple_inputs);
    make_tuple_cnode->set_fullname_with_scope("return tuple");

    std::vector<AnfNodePtr> op_inputs;
    auto return_prim_ptr = std::make_shared<ops::Return>();
    if (return_prim_ptr == nullptr) {
      MS_LOG(ERROR) << "new Return failed";
      return RET_NULL_PTR;
    }
    auto value_node = NewValueNode(return_prim_ptr);
    op_inputs.emplace_back(value_node);
    op_inputs.emplace_back(make_tuple_cnode);
    auto cnode = res_graph_->NewCNode(op_inputs);
    cnode->set_fullname_with_scope("Return");
    res_graph_->set_return(cnode);
  } else {
    auto returnPrim = std::make_shared<ops::Return>();
    if (returnPrim == nullptr) {
      MS_LOG(ERROR) << "new Return failed";
      return RET_NULL_PTR;
    }
    int outputNode = tflite_subgraph->outputs.front() < 0
                       ? static_cast<int>(tflite_subgraph->outputs.front() + tflite_subgraph->tensors.size())
                       : static_cast<int>(tflite_subgraph->outputs.front());
    auto valueNode = NewValueNode(returnPrim);
    std::vector<AnfNodePtr> op_inputs{valueNode};
    auto cnode = nodes_.at(outputNode);
    if (nullptr == cnode) {
      MS_LOG(ERROR) << "Can't find input node.";
      return RET_NOT_FIND_OP;
    }
    op_inputs.emplace_back(cnode);
    auto returnCnode = res_graph_->NewCNode(op_inputs);
    returnCnode->set_fullname_with_scope("Return");
    res_graph_->set_return(returnCnode);
  }
  return RET_OK;
}

STATUS TfliteModelParser::ConvertConstTensor(const tflite::TensorT *tensor, const ParameterPtr &parameter,
                                             const std::string &tensor_name) {
  if (tensor == nullptr) {
    MS_LOG(ERROR) << "tensor is null, get const tensor failed.";
    return RET_NULL_PTR;
  }

  if (parameter == nullptr) {
    MS_LOG(ERROR) << "parameter is null, get const tensor failed.";
    return RET_NULL_PTR;
  }
  const auto &tflite_model_buffers = tflite_model_->buffers;
  auto type_id = GetTfliteDataType(tensor->type);
  std::vector<int64_t> shape_vector;

  const auto &data = tflite_model_buffers.at(tensor->buffer)->data;
  std::string shape_str;
  if (type_id == kObjectTypeString) {
    shape_str += std::to_string(tensor->shape.size()) + ",";
    for (auto &dim : tensor->shape) {
      shape_str += std::to_string(dim) + ",";
    }
    shape_vector = {static_cast<int64_t>(shape_str.size() + data.size())};
  } else {
    (void)std::transform(tensor->shape.begin(), tensor->shape.end(), std::back_inserter(shape_vector),
                         [](const int32_t &value) { return static_cast<int64_t>(value); });
  }

  auto tensor_info = CreateTensorInfo(nullptr, 0, shape_vector, type_id);
  if (tensor_info == nullptr) {
    MS_LOG(ERROR) << "init tensor info failed";
    return RET_NULL_PTR;
  }
  if (!data.empty()) {
    auto tensor_data = reinterpret_cast<uint8_t *>(tensor_info->data_c());
    if (type_id == kObjectTypeString) {
      if (memcpy_s(tensor_data, shape_str.size(), shape_str.data(), shape_str.size()) != EOK) {
        MS_LOG(ERROR) << "memcpy failed.";
        return RET_ERROR;
      }
      if (memcpy_s(tensor_data + shape_str.size(), data.size(), data.data(), data.size()) != EOK) {
        MS_LOG(ERROR) << "memcpy failed.";
        return RET_ERROR;
      }
    } else {
      if (memcpy_s(tensor_data, tensor_info->Size(), data.data(), data.size()) != EOK) {
        MS_LOG(ERROR) << "memcpy failed.";
        return RET_ERROR;
      }
    }
  }
  auto status = InitParameterFromTensorInfo(parameter, tensor_info);
  if (status != RET_OK) {
    MS_LOG(ERROR) << "init parameter from tensor info failed.";
    return RET_ERROR;
  }
  parameter->set_name(tensor_name);
  return RET_OK;
}

STATUS TfliteModelParser::ConvertOutputTensor(const tflite::OperatorT *op, const CNodePtr &dst_cnode) {
  if (op == nullptr) {
    MS_LOG(ERROR) << "op is null, get output tensor failed.";
    return RET_NULL_PTR;
  }

  if (dst_cnode == nullptr) {
    MS_LOG(ERROR) << "parameter is null, get output tensor failed.";
    return RET_NULL_PTR;
  }

  const auto &tflite_subgraph = tflite_model_->subgraphs.front();
  if (op->outputs.size() == 1) {
    int output_idx =
      op->outputs.front() < 0 ? tflite_subgraph->tensors.size() + op->outputs.front() : op->outputs.front();
    const auto &tensor = tflite_subgraph->tensors.at(output_idx);
    std::vector<int64_t> shape_vector;
    (void)std::transform(tensor->shape.begin(), tensor->shape.end(), std::back_inserter(shape_vector),
                         [](const int32_t &value) { return static_cast<int64_t>(value); });
    auto abstract_tensor = CreateTensorAbstract(shape_vector, GetTfliteDataType(tensor->type));
    if (abstract_tensor == nullptr) {
      MS_LOG(ERROR) << "Create tensor abstarct failed";
      return RET_ERROR;
    }
    dst_cnode->set_abstract(abstract_tensor);
    nodes_.insert(std::pair(op->outputs.front(), dst_cnode));
  } else {
    AbstractBasePtrList abstract_list;
    int op_idx = 0;
    for (auto output_idx : op->outputs) {
      if (output_idx < 0) {
        output_idx = output_idx + tflite_subgraph->tensors.size();
      }
      const auto &tensor = tflite_subgraph->tensors.at(output_idx);
      std::vector<int64_t> shape_vector;
      (void)std::transform(tensor->shape.begin(), tensor->shape.end(), std::back_inserter(shape_vector),
                           [](const int32_t &value) { return static_cast<int64_t>(value); });
      auto abstract_tensor = CreateTensorAbstract(shape_vector, GetTfliteDataType(tensor->type));
      if (abstract_tensor == nullptr) {
        MS_LOG(ERROR) << "Create tensor abstarct failed";
        return RET_ERROR;
      }
      abstract_list.emplace_back(abstract_tensor);
      auto tuple_get_item_prim_ptr = std::make_shared<ops::TupleGetItem>();
      if (tuple_get_item_prim_ptr == nullptr) {
        MS_LOG(ERROR) << "new TupleGetItem failed";
        return RET_NULL_PTR;
      }
      auto tuple_get_item_prim = NewValueNode(tuple_get_item_prim_ptr);
      auto get_item_value = NewValueNode(MakeValue<int>(op_idx));
      std::vector<AnfNodePtr> inputs{tuple_get_item_prim, dst_cnode, get_item_value};
      CNodePtr get_item_cnode = res_graph_->NewCNode(inputs);
      get_item_cnode->set_fullname_with_scope(dst_cnode->fullname_with_scope() + "_getitem_" + std::to_string(op_idx));
      nodes_.insert(std::pair(output_idx, get_item_cnode));
      op_idx++;
    }
    dst_cnode->set_abstract(std::make_shared<abstract::AbstractTuple>(abstract_list));
  }
  return RET_OK;
}

int TfliteModelParser::PostAdjust() { return 0; }
}  // namespace mindspore::lite
