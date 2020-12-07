// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cmath>
#include <cstring>  // memcpy
#include <type_traits>

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "contrib/math/math_test.cc"
#include "hwy/foreach_target.h"

#include "contrib/math/math-inl.h"
#include "hwy/tests/test_util-inl.h"
// clang-format on

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <class Out, class In>
inline Out BitCast(const In& in) {
  static_assert(sizeof(Out) == sizeof(In), "");
  Out out;
  std::memcpy(&out, &in, sizeof(out));
  return out;
}

// Computes the difference in units of last place between x and y.
inline uint32_t ComputeUlpDelta(float x, float y) {
  const uint32_t ux = BitCast<uint32_t>(x + 0.0f);  // -0.0 -> +0.0
  const uint32_t uy = BitCast<uint32_t>(y + 0.0f);  // -0.0 -> +0.0
  return std::abs(BitCast<int32_t>(ux - uy));
}
inline uint64_t ComputeUlpDelta(double x, double y) {
  const uint64_t ux = BitCast<uint64_t>(x + 0.0);  // -0.0 -> +0.0
  const uint64_t uy = BitCast<uint64_t>(y + 0.0);  // -0.0 -> +0.0
  return std::abs(BitCast<int64_t>(ux - uy));
}

template <class T, class D>
void TestMath(const std::string name, T (*fx1)(T), Vec<D> (*fxN)(D, Vec<D>),
              D d, T min, T max, uint64_t max_error_ulp) {
  constexpr bool kIsF32 = (sizeof(T) == 4);
  using UintT = MakeUnsigned<T>;

  const UintT min_bits = BitCast<UintT>(min);
  const UintT max_bits = BitCast<UintT>(max);

  // If min is negative and max is positive, the range needs to be broken into
  // two pieces, [+0, max] and [-0, min], otherwise [min, max].
  int range_count = 1;
  UintT ranges[2][2] = {{min_bits, max_bits}, {0, 0}};
  if ((min < 0.0) && (max > 0.0)) {
    ranges[0][0] = BitCast<UintT>(static_cast<T>(+0.0));
    ranges[0][1] = max_bits;
    ranges[1][0] = BitCast<UintT>(static_cast<T>(-0.0));
    ranges[1][1] = min_bits;
    range_count = 2;
  }

  uint64_t max_ulp = 0;
  constexpr UintT kSamplesPerRange = 100000;
  for (int range_index = 0; range_index < range_count; ++range_index) {
    const UintT start = ranges[range_index][0];
    const UintT stop = ranges[range_index][1];
    const UintT step = std::max<UintT>(1, ((stop - start) / kSamplesPerRange));
    for (UintT value_bits = start; value_bits <= stop; value_bits += step) {
      const T value = BitCast<T>(std::min(value_bits, stop));
      const T actual = GetLane(fxN(d, Set(d, value)));
      const T expected = fx1(value);
      const auto ulp = ComputeUlpDelta(actual, expected);
      max_ulp = std::max<uint64_t>(max_ulp, ulp);
      ASSERT_LE(ulp, max_error_ulp)
          << name << "<" << (kIsF32 ? "F32x" : "F64x") << Lanes(d) << ">("
          << value << ") expected: " << expected << " actual: " << actual;
    }
  }
  std::cout << (kIsF32 ? "F32x" : "F64x") << Lanes(d)
            << ", Max ULP: " << max_ulp << std::endl;
}

#define DEFINE_MATH_TEST(NAME, F32x1, F32xN, F32_MIN, F32_MAX, F32_ERROR, \
                         F64x1, F64xN, F64_MIN, F64_MAX, F64_ERROR)       \
  struct Test##NAME {                                                     \
    template <class T, class D>                                           \
    HWY_NOINLINE void operator()(T, D d) {                                \
      if (sizeof(T) == 4) {                                               \
        TestMath<T, D>(HWY_STR(NAME), F32x1, F32xN, d, F32_MIN, F32_MAX,  \
                       F32_ERROR);                                        \
      } else {                                                            \
        TestMath<T, D>(HWY_STR(NAME), F64x1, F64xN, d, F64_MIN, F64_MAX,  \
                       F64_ERROR);                                        \
      }                                                                   \
    }                                                                     \
  };                                                                      \
  HWY_NOINLINE void TestAll##NAME() {                                     \
    ForFloatTypes(ForPartialVectors<Test##NAME>());                       \
  }

// clang-format off
DEFINE_MATH_TEST(Exp,
  std::exp,   Exp,   -FLT_MAX,  +104.0f,   1,
  std::exp,   Exp,   -DBL_MAX,  +104.0f,   1)
DEFINE_MATH_TEST(Expm1,
  std::expm1, Expm1, -FLT_MAX,  +104.0f,   4,
  std::expm1, Expm1, -DBL_MAX,  +104.0f,   4)
DEFINE_MATH_TEST(Sinh,
  std::sinh,  Sinh,  -88.7228f, +88.7228f, 4,
  std::sinh,  Sinh,  -709.0,    +709.0,    4)
DEFINE_MATH_TEST(Tanh,
  std::tanh,  Tanh,  -FLT_MAX,  +FLT_MAX,  4,
  std::tanh,  Tanh,  -DBL_MAX,  +DBL_MAX,  4)
// clang-format on

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {

class HwyMathTest : public hwy::TestWithParamTarget {};

HWY_TARGET_INSTANTIATE_TEST_SUITE_P(HwyMathTest);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllExp);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllExpm1);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllSinh);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllTanh);

}  // namespace hwy
#endif
