# Copyright (c) 2017 Sony Corporation. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
# *WARNING*
# THIS FILE IS AUTO-GENERATED BY CODE GENERATOR.
# 1. IMPLEMENT BACKWARD WRT INPUTS OF THE CORRESPONDING FUNCTION
# 2. IMPLEMENT BACKWARD_FUNCTION_CLASS IF NECESSARY (see e.g., affine.py)
# 3. UPDATE THE MAPPING IF NECESSARY (see function_backward_functions.py.tmpl)

import numpy as np
import nnabla as nn
import nnabla.functions as F
from .utils import get_output, no_grad
from .div2 import div2_backward
from .affine import affine_backward


def _spectral_norm_backward(dw_sn, w, u, dim=0, itr=1, eps=1e-12):
    # Forward recomputation

    w_shape = w.shape
    # Transpose if the output dimension is not the most-left dimension.
    if dim != 0:
        dims_transpose = [dim] + [i for i in range(len(w_shape)) if i != dim]
        w = F.transpose(w, dims_transpose)
        w_shape = w.shape
    d0 = w.shape[0]            # Out
    d1 = np.prod(w.shape[1:])  # In
    w = F.reshape(w, [d0, d1])
    u = F.reshape(u, [1, d0])
    # Power method
    for _ in range(itr):
        # v
        v = F.affine(u, w)
        v = v / ((F.sum(v ** 2.0, keepdims=True) + eps) ** 0.5)
        v = F.reshape(v, [d1, 1])
        # u
        u = F.affine(w, v)
        u = u / ((F.sum(u ** 2.0, keepdims=True) + eps) ** 0.5)
        u = F.reshape(u, [1, d0])
    # No grad
    u = no_grad(u)
    v = no_grad(v)
    # Spectral normalization
    wv = F.affine(w, v)
    sigma = F.affine(u, wv)
    # The fowllowing process is not necessary for gradient calculation
    # w_sn = w / sigma
    # w_sn = F.reshape(w_sn, w_shape)
    # # Transpose again if the output dimension is not the most-left dimension.
    # if dim != 0:
    #     dims_transpose = [i for i in range(1, dim + 1)] \
    #                      + [0] + [i for i in range(dim + 1, len(w_shape))]
    #     w_sn = F.transpose(w_sn, dims_transpose)

    # Backward

    # Backward for post-transpose
    if dim != 0:
        dims_transpose = [dim] + [i for i in range(len(w_shape)) if i != dim]
        dw_sn = F.transpose(dw_sn, dims_transpose)
    dw_sn = dw_sn.reshape(w.shape)
    # Backward for normalization
    dw, dsigma = div2_backward([dw_sn, w, sigma])
    du, dwv = affine_backward([dsigma, u, wv])
    dw_, dv = affine_backward([dwv, w, v])
    dw += dw_
    # Backward for pre-transpose
    dw = dw.reshape(w_shape)
    if dim != 0:
        dims_transpose = [i for i in range(1, dim + 1)] \
                         + [0] + [i for i in range(dim + 1, len(w_shape))]
        dw = F.transpose(dw, dims_transpose)

    return dw, None


def _spectral_norm_outer_most_dim_backward(dw_sn, w, u, itr=1, eps=1e-12):
    # Forward recomputation

    w_shape = w.shape
    d0 = np.prod(w.shape[0:-1])  # In
    d1 = w.shape[-1]             # Out
    w = F.reshape(w, [d0, d1])
    u = F.reshape(u, [d1, 1])
    # Power method
    for _ in range(itr):
        # v
        v = F.affine(w, u)
        v = v / ((F.sum(v ** 2.0, keepdims=True) + eps) ** 0.5)
        v = F.reshape(v, [1, d0])
        # u
        u = F.affine(v, w)
        u = u / ((F.sum(u ** 2.0, keepdims=True) + eps) ** 0.5)
        u = F.reshape(u, [d1, 1])
    # No grad
    u = no_grad(u)
    v = no_grad(v)
    # Spectral normalization
    vw = F.affine(v, w)
    sigma = F.affine(vw, u)
    # The fowllowing process is not necessary for gradient calculation
    # w_sn = w / sigma
    # w_sn = F.reshape(w_sn, w_shape)

    # Backward

    dw_sn = dw_sn.reshape(w.shape)
    dw, dsigma = div2_backward([dw_sn, w, sigma])
    dvw, du = affine_backward([dsigma, vw, u])
    dv, dw_ = affine_backward([dvw, v, w])
    dw += dw_
    dw = dw.reshape(w_shape)
    return dw, None


def spectral_norm_backward(inputs, dim=0, itr=1, eps=1e-12, test=False, output_u=False):
    """
    Args:
      inputs (list of nn.Variable): Incomming grads/inputs to/of the forward function.
      kwargs (dict of arguments): Dictionary of the corresponding function arguments.

    Return:
      list of Variable: Return the gradients wrt inputs of the corresponding function.
    """
    if not output_u:
        # We need to get original `u` from output for gradient calculation.
        raise ValueError(
            "spectral_norm_backward is supported for output_u=True.")

    dw_sn = inputs[0]
    w = inputs[2]
    u = get_output(w, "SpectralNorm", nth_output=1)

    if dim == w.ndim - 1:
        return _spectral_norm_outer_most_dim_backward(dw_sn, w, u, itr, eps)
    else:
        return _spectral_norm_backward(dw_sn, w, u, dim, itr, eps)
