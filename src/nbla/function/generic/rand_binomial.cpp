// Copyright (c) 2017 Sony Corporation. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/** RandBinomial
 */
#include <nbla/array.hpp>
#include <nbla/common.hpp>
#include <nbla/function/rand_binomial.hpp>
#include <nbla/variable.hpp>

#include <random>

namespace nbla {

NBLA_REGISTER_FUNCTION_SOURCE(RandBinomial, int, float, const vector<int> &,
                              int);

template <typename T>
void RandBinomial<T>::setup_impl(const Variables &inputs,
                                 const Variables &outputs) {
  outputs[0]->reshape(Shape_t(shape_.cbegin(), shape_.cend()), true);
  rgen_ = std::mt19937((seed_ == -1 ? std::random_device()() : seed_));
}

template <typename T>
void RandBinomial<T>::forward_impl(const Variables &inputs,
                                   const Variables &outputs) {
  std::binomial_distribution<int> rdist(n_, p_);
  std::mt19937 rgen =
      seed_ == -1 ? SingletonManager::<RandomManager>()->get_rand_generator()
                  : rgen_;
  T *y = outputs[0]->cast_data_and_get_pointer<T>(this->ctx_, true);
  for (int s = 0; s < outputs[0]->size(); s++) {
    y[s] = (T)rdist(rgen);
  }
}

template <typename T>
void RandBinomial<T>::backward_impl(const Variables &inputs,
                                    const Variables &outputs,
                                    const vector<bool> &propagate_down,
                                    const vector<bool> &accum) {
  // Pass
}
} // namespace nbla
