/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_LITE_MICRO_CODER_GENERATOR_CONST_BLOCKS_MICRO_TENSOR_H_
#define MINDSPORE_LITE_MICRO_CODER_GENERATOR_CONST_BLOCKS_MICRO_TENSOR_H_

const char *micro_tensor_h =
  "/**\n"
  " * Copyright 2021 Huawei Technologies Co., Ltd\n"
  " *\n"
  " * Licensed under the Apache License, Version 2.0 (the \"License\");\n"
  " * you may not use this file except in compliance with the License.\n"
  " * You may obtain a copy of the License at\n"
  " *\n"
  " * http://www.apache.org/licenses/LICENSE-2.0\n"
  " *\n"
  " * Unless required by applicable law or agreed to in writing, software\n"
  " * distributed under the License is distributed on an \"AS IS\" BASIS,\n"
  " * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
  " * See the License for the specific language governing permissions and\n"
  " * limitations under the License.\n"
  " */\n"
  "\n"
  "#ifndef MSMICRO_TENSOR_H\n"
  "#define MSMICRO_TENSOR_H\n"
  "\n"
  "#include <stdlib.h>\n"
  "#include <string.h>\n"
  "#include <stdio.h>\n"
  "#include <stdbool.h>\n"
  "#include <stdint.h>\n"
  "\n"
  "#define MICRO_INFO(content, args...) \\\n"
  "  { printf(\"[INFO] %s|%d: \" #content \"\\r\\n\", __func__, __LINE__, ##args); }\n"
  "#define MICRO_ERROR(content, args...) \\\n"
  "  { printf(\"[ERROR] %s|%d: \" #content \"\\r\\n\", __func__, __LINE__, ##args); }\n"
  "\n"
  "enum STATUS {\n"
  "  RET_OK = 0,\n"
  "  RET_ERROR = 1,\n"
  "};\n"
  "\n"
  "enum DataType {\n"
  "  DataType_DT_FLOAT = 0,\n"
  "  DataType_DT_FLOAT16 = 1,\n"
  "  DataType_DT_INT8 = 2,\n"
  "  DataType_DT_INT32 = 3,\n"
  "  DataType_DT_UINT8 = 4,\n"
  "  DataType_DT_INT16 = 5,\n"
  "  DataType_DT_UINT32 = 8,\n"
  "  DataType_DT_INT64 = 9,\n"
  "  DataType_DT_UINT16 = 10,\n"
  "  DataType_DT_UNDEFINED = 16,\n"
  "  DataType_MIN = DataType_DT_FLOAT,\n"
  "  DataType_MAX = DataType_DT_UNDEFINED\n"
  "};\n"
  "\n"
  "enum Format {\n"
  "  Format_NCHW = 0,\n"
  "  Format_NHWC = 1,\n"
  "  Format_HWKC = 2,\n"
  "  Format_HWCK = 3,\n"
  "  Format_KCHW = 4,\n"
  "  Format_CKHW = 5,\n"
  "  Format_KHWC = 6,\n"
  "  Format_CHWK = 7,\n"
  "  Format_NC4HW4 = 100,\n"
  "  Format_NUM_OF_FORMAT = 101,\n"
  "  Format_MIN = Format_NCHW,\n"
  "  Format_MAX = Format_NUM_OF_FORMAT\n"
  "};\n"
  "\n"
  "typedef struct {\n"
  "  enum DataType type;\n"
  "  enum Format format;\n"
  "  int ndim;\n"
  "  int *dim;\n"
  "  void *data;\n"
  "} MicroTensor;\n"
  "\n"
  "typedef struct {\n"
  "  int num;\n"
  "  MicroTensor *tensor;\n"
  "} MicroTensorList;\n"
  "\n"
  "typedef struct {\n"
  "  float in_scale;\n"
  "  float out_scale;\n"
  "  int in_zero_point;\n"
  "  int out_zero_point;\n"
  "} GraphQuantArgs;\n"
  "\n"
  "#endif  // MSMICRO_TENSOR_H\n";
#endif  // MINDSPORE_LITE_MICRO_CODER_GENERATOR_CONST_BLOCKS_MICRO_TENSOR_H_
