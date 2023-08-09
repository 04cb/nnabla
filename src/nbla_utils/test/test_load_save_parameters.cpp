// Copyright 2019,2020,2021 Sony Corporation.
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

// test_load_save_parameters.cpp

#include "gtest/gtest.h"
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include <nbla/computation_graph/computation_graph.hpp>
#include <nbla/parametric_functions.hpp>
#include <nbla/solver/adam.hpp>
#include <nbla/std.hpp>
#include <nbla_utils/nnp.hpp>
#include <nbla_utils/parameters.hpp>

namespace nbla {
namespace utils {

using namespace std;
using namespace nbla;
namespace f = nbla::functions;
namespace pf = nbla::parametric_functions;

const Context kCpuCtx{{"cpu:float"}, "CpuCachedArray", "0"};
const string filename = "params.protobuf";
const string filename_h5 = "parameters.h5";

CgVariablePtr model(CgVariablePtr x, ParameterDirectory parameters) {
  auto h = pf::convolution(x, 1, 16, {3, 3}, parameters["conv1"]);
  h = f::max_pooling(h, {2, 2}, {2, 2}, true, {0, 0}, false);
  h = f::relu(h, false);
  h = pf::convolution(h, 1, 16, {3, 3}, parameters["conv2"]);
  h = f::max_pooling(h, {2, 2}, {2, 2}, true, {0, 0}, false);
  h = f::relu(h, false);
  h = pf::affine(h, 1, 50, parameters["affine3"]);
  h = f::relu(h, false);
  h = pf::affine(h, 1, 10, parameters["affine4"]);
  return h;
}

void set_input(CgVariablePtr x) {
  float_t *x_d =
      x->variable()->cast_data_and_get_pointer<float_t>(kCpuCtx, true);
  for (int i = 0; i < 28 * 28; ++i) {
    x_d[i] = 0.1 * i; // set a arbitrarily value
  }
}

void set_t(CgVariablePtr t) {
  float_t *t_d =
      t->variable()->cast_data_and_get_pointer<float_t>(kCpuCtx, true);
  for (int i = 0; i < 10; ++i) {
    t_d[i] = i; // set value from 0 to 9
  }
}

void check_result(CgVariablePtr x, CgVariablePtr y) {
  float_t *x_d =
      x->variable()->cast_data_and_get_pointer<float_t>(kCpuCtx, true);
  float_t *y_d =
      y->variable()->cast_data_and_get_pointer<float_t>(kCpuCtx, true);

  for (int i = 0; i < 10; ++i) {
    EXPECT_FLOAT_EQ(x_d[i], y_d[i]);
  }
}

void dump_parameters(ParameterDirectory &params) {
  auto pd_param = params.get_parameters();
  for (auto it = pd_param.begin(); it != pd_param.end(); it++) {
    string name = it->first;
    VariablePtr variable = it->second;
    printf("==> %s\n", name.c_str());
    float *data = variable->template cast_data_and_get_pointer<float>(kCpuCtx);
    for (int i = 0; i < variable->size(); ++i) {
      printf("%4.2f ", data[i]);
    }
    printf("\n");
  }
}

CgVariablePtr simple_train(ParameterDirectory &params) {
  // we prepared parameters by simply running a inferring work.
  int batch_size = 1;
  auto x = make_shared<CgVariable>(Shape_t({batch_size, 1, 28, 28}), false);
  auto t = make_shared<CgVariable>(Shape_t({batch_size, 1}), false);
  auto h = model(x, params);
  auto loss = f::mean(f::softmax_cross_entropy(h, t, 1), {0, 1}, false);
  auto err = f::mean(f::top_n_error(h, t, 1, 1), {0, 1}, false);
  auto adam = create_AdamSolver(kCpuCtx, 0.001, 0.9, 0.999, 1.0e-8);
  adam->set_parameters(params.get_parameters());
  adam->zero_grad();
  set_input(x);
  set_t(t);
  loss->forward();
  loss->variable()->grad()->fill(1.0);
  loss->backward(nullptr, true);
  adam->update();
  set_input(x);
  h->forward();
  return h;
}

#define COMPARE_RESULT(dt, t)                                                  \
  case dt: {                                                                   \
    t *a_data = a_it->second->template cast_data_and_get_pointer<t>(kCpuCtx);  \
    t *b_data = b_it->second->template cast_data_and_get_pointer<t>(kCpuCtx);  \
    EXPECT_TRUE(memcmp(a_data, b_data, a_size) == 0);                          \
    printf("dt=%d\n", (int)dt);                                                \
    exist = true;                                                              \
  } break;

void expect_params_equal(vector<pair<string, VariablePtr>> &a,
                         vector<pair<string, VariablePtr>> &b) {
  for (auto a_it = a.begin(); a_it != a.end(); ++a_it) {
    bool exist = false;
    for (auto b_it = b.begin(); b_it != b.end(); ++b_it) {
      if (a_it->first == b_it->first) {
        int a_size = a_it->second->size();
        int b_size = b_it->second->size();
        EXPECT_EQ(a_size, b_size);
        dtypes dtype = a_it->second->data()->array()->dtype();

        switch (dtype) {
          COMPARE_RESULT(dtypes::BYTE, char);
          COMPARE_RESULT(dtypes::UBYTE, unsigned char);
          COMPARE_RESULT(dtypes::SHORT, short);
          COMPARE_RESULT(dtypes::USHORT, unsigned short);
          COMPARE_RESULT(dtypes::INT, int);
          COMPARE_RESULT(dtypes::UINT, unsigned int);
          COMPARE_RESULT(dtypes::LONG, long);
          COMPARE_RESULT(dtypes::ULONG, unsigned long);
          COMPARE_RESULT(dtypes::LONGLONG, long long);
          COMPARE_RESULT(dtypes::ULONGLONG, unsigned long long);
          COMPARE_RESULT(dtypes::FLOAT, float);
          COMPARE_RESULT(dtypes::DOUBLE, double);
          COMPARE_RESULT(dtypes::LONGDOUBLE, long double);
          COMPARE_RESULT(dtypes::HALF, Half);
          COMPARE_RESULT(dtypes::BOOL, bool);
        }
      }
    }
    EXPECT_TRUE(exist);
  }
}

nbla::vector<char> get_file_buffer(const string filename) {
  std::ifstream ifs(filename, ios::binary);
  const auto begin = ifs.tellg();
  ifs.seekg(0, ios::end);
  const auto end = ifs.tellg();
  const auto file_size = end - begin;
  nbla::vector<char> buf(file_size);
  ifs.seekg(0, ios::beg);
  ifs.read(buf.data(), file_size);
  return buf;
}

CgVariablePtr simple_infer(ParameterDirectory &params) {
  int batch_size = 1;
  auto x = make_shared<CgVariable>(Shape_t({batch_size, 1, 28, 28}), false);
  auto t = make_shared<CgVariable>(Shape_t({batch_size, 1}), false);
  auto h = model(x, params);
  set_input(x);
  h->forward();
  return h;
}

TEST(test_save_and_load_parameters, test_save_load_with_simple_train) {
  ParameterDirectory train_params;
  ParameterDirectory infer_params;

  CgVariablePtr x = simple_train(train_params);
  save_parameters(train_params, filename);
  load_parameters(infer_params, filename);
  CgVariablePtr y = simple_infer(infer_params);
  check_result(x, y);
}

TEST(test_save_and_load_parameters, test_save_load_without_train) {
  ParameterDirectory train_params;
  ParameterDirectory infer_params;

  CgVariablePtr x = simple_infer(train_params);
  save_parameters(train_params, filename);
  load_parameters(infer_params, filename);
  CgVariablePtr y = simple_infer(infer_params);
  check_result(x, y);
}

TEST(test_save_and_load_parameters, test_save_load_with_simple_train_pb_buf) {
  ParameterDirectory train_params;
  ParameterDirectory infer_params;

  CgVariablePtr x = simple_train(train_params);
  save_parameters(train_params, filename);
  nbla::vector<char> buf = get_file_buffer(filename);
  load_parameters_pb(infer_params, buf.data(), buf.size());
  CgVariablePtr y = simple_infer(infer_params);
  check_result(x, y);
}

TEST(test_save_and_load_parameters, test_save_load_without_train_pb_buf) {
  ParameterDirectory train_params;
  ParameterDirectory infer_params;

  CgVariablePtr x = simple_infer(train_params);
  save_parameters(train_params, filename);
  nbla::vector<char> buf = get_file_buffer(filename);
  load_parameters_pb(infer_params, buf.data(), buf.size());
  CgVariablePtr y = simple_infer(infer_params);
  check_result(x, y);
}

TEST(test_save_and_load_parameters, test_nnp_save_pb_buffer) {
  ParameterDirectory train_params;
  ParameterDirectory infer_params;
  unsigned int size = 0;

  CgVariablePtr x = simple_train(train_params);
  save_parameters_pb(train_params, NULL, size);
  nbla::vector<char> buffer(size);
  save_parameters_pb(train_params, buffer.data(), size);
  load_parameters_pb(infer_params, buffer.data(), buffer.size());
  CgVariablePtr y = simple_infer(infer_params);
  check_result(x, y);
}

#ifdef NBLA_UTILS_WITH_HDF5
TEST(test_save_and_load_parameters, test_save_load_h5) {
  ParameterDirectory train_params;
  ParameterDirectory infer_params;

  CgVariablePtr x = simple_train(train_params);
  save_parameters(train_params, filename_h5);
  load_parameters(infer_params, filename_h5);
  CgVariablePtr y = simple_infer(infer_params);
  check_result(x, y);
}

TEST(test_save_and_load_parameters, test_save_load_h5_buf) {
  ParameterDirectory train_params;
  ParameterDirectory infer_params;

  CgVariablePtr x = simple_train(train_params);
  save_parameters(train_params, filename_h5);
  nbla::vector<char> buf = get_file_buffer(filename_h5);
  load_parameters_h5(infer_params, buf.data(), buf.size());
  CgVariablePtr y = simple_infer(infer_params);
  check_result(x, y);
}

TEST(test_save_and_load_parameters, test_nnp_save_h5) {
  ParameterDirectory train_params;
  ParameterDirectory infer_params;

  CgVariablePtr x = simple_train(train_params);
  save_parameters(train_params, filename);

  nbla::utils::nnp::Nnp nnp(kCpuCtx);
  nnp.add(filename);
  nnp.save_parameters(filename_h5);
  nbla::utils::nnp::Nnp nnp_ref(kCpuCtx);
  nnp_ref.add(filename_h5);
  auto params = nnp.get_parameters();
  auto params_ref = nnp_ref.get_parameters();
  expect_params_equal(params, params_ref);

  load_parameters(infer_params, filename_h5);
  CgVariablePtr y = simple_infer(infer_params);
  check_result(x, y);
}

TEST(test_save_and_load_parameters, test_nnp_save_pb) {
  ParameterDirectory train_params;
  ParameterDirectory infer_params;

  CgVariablePtr x = simple_train(train_params);
  save_parameters(train_params, filename_h5);

  nbla::utils::nnp::Nnp nnp(kCpuCtx);
  nnp.add(filename_h5);
  nnp.save_parameters(filename);
  nbla::utils::nnp::Nnp nnp_ref(kCpuCtx);
  nnp_ref.add(filename);
  auto params = nnp.get_parameters();
  auto params_ref = nnp_ref.get_parameters();
  expect_params_equal(params, params_ref);

  load_parameters(infer_params, filename);
  CgVariablePtr y = simple_infer(infer_params);
  check_result(x, y);
}

TEST(test_save_and_load_parameters, test_nnp_save_h5_buffer) {
  ParameterDirectory train_params;
  ParameterDirectory infer_params;
  unsigned int size = 0;

  CgVariablePtr x = simple_train(train_params);
  save_parameters_h5(train_params, NULL, size);
  nbla::vector<char> buffer(size);
  save_parameters_h5(train_params, buffer.data(), size);
  load_parameters_h5(infer_params, buffer.data(), buffer.size());
  auto p1 = train_params.get_parameters();
  auto p2 = infer_params.get_parameters();
  expect_params_equal(p1, p2);

  CgVariablePtr y = simple_infer(infer_params);
  check_result(x, y);
}

template <typename T> CgVariablePtr create_cgvariable(T value) {
  auto cg_v = make_shared<CgVariable>(Shape_t({1}), true);
  T *data = cg_v->variable()->template cast_data_and_get_pointer<T>(kCpuCtx);
  *data = value;
  return cg_v;
}

void prepare_parameters_with_different_type(ParameterDirectory &pd) {
  pd.get_parameter_or_create("char", create_cgvariable<char>('c'));
  pd.get_parameter_or_create("uint8_t", create_cgvariable<uint8_t>('u'));
  pd.get_parameter_or_create("short", create_cgvariable<short>(1234));
  pd.get_parameter_or_create("int", create_cgvariable<int>(1234));
  pd.get_parameter_or_create("uint32_t", create_cgvariable<uint32_t>(1234));
  pd.get_parameter_or_create("long", create_cgvariable<long>(1234));
  pd.get_parameter_or_create("ulong", create_cgvariable<unsigned long>(1234));
  pd.get_parameter_or_create("longlong", create_cgvariable<long long>(1234));
  pd.get_parameter_or_create("ulonglong",
                             create_cgvariable<unsigned long long>(1234));
  pd.get_parameter_or_create("float16", create_cgvariable<Half>(1234.0));
  pd.get_parameter_or_create("float32", create_cgvariable<float>(1234.0));
  pd.get_parameter_or_create("double", create_cgvariable<double>(1234.0));
  pd.get_parameter_or_create("bool", create_cgvariable<bool>(true));
}

TEST(test_save_and_load_parameters, test_different_data_type) {
  ParameterDirectory params, loaded_params;

  // Step 1: prepare parameter directory
  prepare_parameters_with_different_type(params);

  // step 2: Save parameters to file
  unsigned int size = 0;
  save_parameters_h5(params, nullptr, size);
  nbla::vector<char> buffer(size);
  save_parameters_h5(params, buffer.data(), size);

  // step 3: load it back
  load_parameters_h5(loaded_params, buffer.data(), buffer.size());

  // step 4: compare result
  auto p1 = params.get_parameters();
  auto p2 = loaded_params.get_parameters();
  expect_params_equal(p1, p2);
}

#endif
} // namespace utils
} // namespace nbla
