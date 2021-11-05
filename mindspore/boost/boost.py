# Copyright 2021 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
"""boost"""
import threading
from .less_batch_normalization import LessBN
from .grad_freeze import GradientFreeze
from .base import OptimizerProcess, ParameterProcess


__all__ = ["AutoBoost"]

_boost_config_mode = ["auto", "manual", "enable_all", "disable_all"]
_boost_config_level = {
    "O0": {
        "less_bn": False,
        "grad_freeze": False,
        "adasum": False,
        "grad_accumulation": False},
    "O1": {
        "less_bn": True,
        "grad_freeze": True,
        "adasum": False,
        "grad_accumulation": False},
    "O2": {
        "less_bn": True,
        "grad_freeze": True,
        "adasum": True,
        "grad_accumulation": False}}


class AutoBoost:
    r"""
    Provide auto accelerating for network.

    Args:
        level (str): Boost config level. Default: "O0".
        boost_config_dict (dict): User config hyperparameter dict, recommended config format:
            {
                "boost": {
                    "//": "suggest mode: ["auto", "manual", "enable_all", "disable_all"]",
                    "mode": "auto",
                    "less_bn": false,
                    "grad_freeze": false,
                    "adasum": false,
                    "grad_accumulation": false
                },
                "common": {
                    "gradient_split_groups": [50, 100]
                },
                "less_bn": {
                    "fn_flag": true,
                    "gc_flag": true
                },
                "grad_freeze": {
                    "param_groups": 10,
                    "freeze_type": 1,
                    "freeze_p": 0.7,
                    "total_steps": 65536
                },
                "adasum": {
                    "device_number": 8
                },
                "grad_accumulation": {
                    "grad_accumulation_step": 1
                }
            }
            User can load the config through the JSON file or use the dictionary directly.
            The unconfigured parameters will adopt the default values. Default: "".

    Raises:
        ValueError: The boost mode not in ["auto", "manual", "enable_all", "disable_all"].

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> from mindspore.boost import AutoBoost
        >>> #1) when configuring the dict directly:
        >>> boost_config_dict = {"boost": {"mode": "auto"}}
        >>> boost = AutoBoost("O1", boost_config_dict)
        >>>
        >>> #2) when loading the dict from a json file:
        >>> import json
        >>> boost_json = "/path/boost_config.json"
        >>> with open(boost_json, 'r') as fp:
        >>>     boost_config_dict = json.load(fp)
        >>> boost = AutoBoost("O1", boost_config_dict)
    """
    _instance_lock = threading.Lock()
    _instance = None

    def __init__(self, level="O0", boost_config_dict=""):
        if level not in _boost_config_level.keys():
            level = "O0"
        if self._instance.level is None:
            self.level = level
            self.boost_config_dict = boost_config_dict
            self._fn_flag = True
            self._gc_flag = True
            self._param_groups = 10
            self._freeze_type = 1
            self._freeze_p = 0.7
            self._total_steps = 65536
            self.gradient_groups = None
            self.device_number = 8
            self.grad_accumulation_step = 1
            self.boost_config = self._get_configuration(level, self.boost_config_dict)
            self._param_processer = ParameterProcess()

    # pylint: disable=unused-argument
    def __new__(cls, *args, **kwargs):
        if AutoBoost._instance is None:
            with AutoBoost._instance_lock:
                if AutoBoost._instance is None:
                    AutoBoost._instance = object.__new__(cls)
                    AutoBoost._instance.level = None
                    AutoBoost._instance.boost_config_dict = None
        return AutoBoost._instance

    def network_auto_process_train(self, network, optimizer):
        r"""
        Boost network train.

        Args:
            network (Cell) - The training network.
            optimizer (Cell) - Optimizer for updating the weights.
        """
        if self.boost_config["less_bn"]:
            network = LessBN(network, fn_flag=self._fn_flag)
            optimizer_process = OptimizerProcess(optimizer)
            group_params = self._param_processer.assign_parameter_group(network.trainable_params(),
                                                                        self.gradient_groups)
            optimizer_process.origin_params = \
                self._param_processer.generate_group_params(group_params, optimizer_process.origin_params)
            if self._gc_flag:
                optimizer_process.add_grad_centralization(network)
            optimizer = optimizer_process.generate_new_optimizer()

        if self.boost_config["grad_freeze"]:
            freeze_processer = GradientFreeze(self._param_groups, self._freeze_type,
                                              self._freeze_p, self._total_steps)
            network, optimizer = freeze_processer.freeze_generate(network, optimizer)

        if self.boost_config["adasum"]:
            setattr(optimizer, "adasum", True)
        return network, optimizer

    def network_auto_process_eval(self, network):
        r"""
        Boost network eval.

        Args:
            network (Cell) - The inference network.
        """
        if self.boost_config["less_bn"]:
            network = LessBN(network)

        return network

    def set_fn_flag(self, fn_flag):
        self._fn_flag = fn_flag

    def set_gc_flag(self, gc_flag):
        self._gc_flag = gc_flag

    def set_param_groups(self, param_groups):
        self._param_groups = param_groups

    def set_freeze_type(self, freeze_type):
        self._freeze_type = freeze_type

    def set_freeze_p(self, freeze_p):
        self._freeze_p = freeze_p

    def set_total_steps(self, total_steps):
        self._total_steps = total_steps

    def set_device_number(self, device_number):
        self.device_number = device_number

    def set_grad_accumulation_step(self, grad_accumulation_step):
        self.grad_accumulation_step = grad_accumulation_step

    def set_gradient_split_groups(self, gradient_groups):
        if not isinstance(gradient_groups, (list, int)):
            raise ValueError(f"gradient_groups `{gradient_groups}` is not in (list, int)")
        if isinstance(gradient_groups, int):
            gradient_groups = list(gradient_groups)
        self.gradient_groups = gradient_groups

    def _get_configuration(self, level, boost_config_dict):
        """Get configuration."""
        level_config = _boost_config_level[level]
        if not boost_config_dict:
            return level_config
        mode = "auto"
        if 'boost' in boost_config_dict and 'mode' in boost_config_dict['boost']:
            mode = boost_config_dict['boost']['mode']
        if mode not in _boost_config_mode:
            raise ValueError("The boost mode must be in {}, but got {}".format(_boost_config_mode, mode))
        if mode == "manual":
            for key, value in boost_config_dict["boost"].items():
                if key in level_config:
                    level_config[key] = value
        elif mode == "enable_all":
            level_config = {key: True for key in level_config}
        elif mode == "disable_all":
            level_config = {key: False for key in level_config}
        for key, boost_each_mode_config in boost_config_dict.items():
            if key in level_config.keys() and level_config[key] or key == "common":
                for key_s in boost_each_mode_config.keys():
                    if key_s in self._boost_config_func_map:
                        self._boost_config_func_map[key_s](self, boost_each_mode_config[key_s])
        return level_config


    _boost_config_func_map = {
        "fn_flag": set_fn_flag,
        "gc_flag": set_gc_flag,
        "param_groups": set_param_groups,
        "freeze_type": set_freeze_type,
        "freeze_p": set_freeze_p,
        "total_steps": set_total_steps,
        "device_number": set_device_number,
        "gradient_split_groups": set_gradient_split_groups,
        "grad_accumulation_step": set_grad_accumulation_step
    }
