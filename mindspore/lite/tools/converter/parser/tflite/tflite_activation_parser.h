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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_PARSER_TFLITE_ACTIVATION_PARSER_H
#define MINDSPORE_LITE_TOOLS_CONVERTER_PARSER_TFLITE_ACTIVATION_PARSER_H

#include <vector>
#include <memory>
#include <map>
#include "tools/converter/parser/tflite/tflite_node_parser.h"
#include "tools/converter/parser/tflite/tflite_node_parser_registry.h"

namespace mindspore::lite {
class TfliteActivationParser : public TfliteNodeParser {
 public:
  TfliteActivationParser() : TfliteNodeParser("node_name") {}

  STATUS Parse(TfliteTensorsInfo *tensors_info, const std::unique_ptr<tflite::OperatorT> &tflite_op,
               const std::unique_ptr<tflite::ModelT> &tflite_model,
               const std::unique_ptr<tflite::SubGraphT> &tflite_subgraph, schema::CNodeT *op) override;

  lite::PrimitiveC *ParseLitePrimitive(const std::unique_ptr<tflite::OperatorT> &tflite_op,
                                       const std::unique_ptr<tflite::ModelT> &tflite_model) override;
};
}  // namespace mindspore::lite

#endif  // MINDSPORE_LITE_TOOLS_CONVERTER_PARSER_TFLITE_ACTIVATION_PARSER_H
