// Copyright (c) 2008-2016, Wojciech Muła
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "signature.hpp"
#include <algorithm>
#include <functional>

#ifdef EPOS_USE_AVX2
#  include <immintrin.h>
#endif

namespace epos {
namespace {

#ifdef EPOS_USE_AVX2

[[maybe_unused]] constexpr bool memcmp1(const char* a, const char* b) noexcept
{
  return a[0] == b[0];
}

[[maybe_unused]] __forceinline bool memcmp2(const char* a, const char* b) noexcept
{
  const auto a16 = *reinterpret_cast<const std::uint16_t*>(a);
  const auto b16 = *reinterpret_cast<const std::uint16_t*>(b);
  return a16 == b16;
}

[[maybe_unused]] __forceinline bool memcmp3(const char* a, const char* b) noexcept
{
  const auto a32 = *reinterpret_cast<const std::uint32_t*>(a);
  const auto b32 = *reinterpret_cast<const std::uint32_t*>(b);
  return (a32 & 0x00FFFFFF) == (b32 & 0x00FFFFFF);
}

[[maybe_unused]] __forceinline bool memcmp4(const char* a, const char* b) noexcept
{
  const auto a32 = *reinterpret_cast<const std::uint32_t*>(a);
  const auto b32 = *reinterpret_cast<const std::uint32_t*>(b);
  return a32 == b32;
}

[[maybe_unused]] __forceinline bool memcmp5(const char* a, const char* b) noexcept
{
  const auto a64 = *reinterpret_cast<const std::uint64_t*>(a);
  const auto b64 = *reinterpret_cast<const std::uint64_t*>(b);
  return ((a64 ^ b64) & 0x000000FFFFFFFFFF) == 0;
}

[[maybe_unused]] __forceinline bool memcmp6(const char* a, const char* b) noexcept
{
  const auto a64 = *reinterpret_cast<const std::uint64_t*>(a);
  const auto b64 = *reinterpret_cast<const std::uint64_t*>(b);
  return ((a64 ^ b64) & 0x0000FFFFFFFFFFFF) == 0;
}

[[maybe_unused]] __forceinline bool memcmp7(const char* a, const char* b) noexcept
{
  const auto a64 = *reinterpret_cast<const std::uint64_t*>(a);
  const auto b64 = *reinterpret_cast<const std::uint64_t*>(b);
  return ((a64 ^ b64) & 0x00FFFFFFFFFFFFFF) == 0;
}

[[maybe_unused]] __forceinline bool memcmp8(const char* a, const char* b) noexcept
{
  const auto a64 = *reinterpret_cast<const std::uint64_t*>(a);
  const auto b64 = *reinterpret_cast<const std::uint64_t*>(b);
  return a64 == b64;
}

[[maybe_unused]] __forceinline bool memcmp9(const char* a, const char* b) noexcept
{
  const auto a64 = *reinterpret_cast<const std::uint64_t*>(a);
  const auto b64 = *reinterpret_cast<const std::uint64_t*>(b);
  return (a64 == b64) && (a[8] == b[8]);
}

[[maybe_unused]] __forceinline bool memcmp10(const char* a, const char* b) noexcept
{
  const auto a64 = *reinterpret_cast<const std::uint64_t*>(a);
  const auto b64 = *reinterpret_cast<const std::uint64_t*>(b);
  const auto a16 = *reinterpret_cast<const std::uint16_t*>(a + 8);
  const auto b16 = *reinterpret_cast<const std::uint16_t*>(b + 8);
  return (a64 == b64) && (a16 == b16);
}

[[maybe_unused]] __forceinline bool memcmp11(const char* a, const char* b) noexcept
{
  const auto a64 = *reinterpret_cast<const std::uint64_t*>(a);
  const auto b64 = *reinterpret_cast<const std::uint64_t*>(b);
  const auto a32 = *reinterpret_cast<const std::uint32_t*>(a + 8);
  const auto b32 = *reinterpret_cast<const std::uint32_t*>(b + 8);
  return (a64 == b64) && ((a32 & 0x00FFFFFF) == (b32 & 0x00FFFFFF));
}

[[maybe_unused]] __forceinline bool memcmp12(const char* a, const char* b) noexcept
{
  const auto a64 = *reinterpret_cast<const std::uint64_t*>(a);
  const auto b64 = *reinterpret_cast<const std::uint64_t*>(b);
  const auto a32 = *reinterpret_cast<const std::uint32_t*>(a + 8);
  const auto b32 = *reinterpret_cast<const std::uint32_t*>(b + 8);
  return (a64 == b64) && (a32 == b32);
}

__forceinline std::size_t avx2_strstr_eq2(const char* s, std::size_t n, const char* d) noexcept
{
  const __m256i broadcasted[2]{
    _mm256_set1_epi8(d[0]),
    _mm256_set1_epi8(d[1]),
  };
  auto curr = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s));
  for (std::size_t i = 0; i < n; i += 32) {
    const auto next = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + i + 32));
    auto eq = _mm256_cmpeq_epi8(curr, broadcasted[0]);

    __m256i next1{};
    next1 = _mm256_inserti128_si256(next1, _mm256_extracti128_si256(curr, 1), 0);
    next1 = _mm256_inserti128_si256(next1, _mm256_extracti128_si256(next, 0), 1);

    const auto substring = _mm256_alignr_epi8(next1, curr, 1);
    eq = _mm256_and_si256(eq, _mm256_cmpeq_epi8(substring, broadcasted[1]));
    if (const auto mask = _mm256_movemask_epi8(eq)) {
      return i + _tzcnt_u32(mask);
    }

    curr = next;
  }
  return n;
}

template <std::size_t k>
__forceinline std::size_t avx2_strstr_memcmp(const char* s, std::size_t n, const char* d, auto memcmp) noexcept
{
  const auto s0 = _mm256_set1_epi8(d[0]);
  const auto s1 = _mm256_set1_epi8(d[k - 1]);
  for (std::size_t i = 0; i < n; i += 32) {
    const auto b0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + i));
    const auto b1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + i + k - 1));
    const auto e0 = _mm256_cmpeq_epi8(s0, b0);
    const auto e1 = _mm256_cmpeq_epi8(s1, b1);
    auto mask = _mm256_movemask_epi8(_mm256_and_si256(e0, e1));
    while (mask) {
      const auto bitpos = _tzcnt_u32(mask);
      if (memcmp(s + i + bitpos + 1, d + 1)) {
        return i + bitpos;
      }
      mask &= mask - 1;
    }
  }
  return n;
}

__forceinline std::size_t avx2_strstr_anysize(const char* s, std::size_t n, const char* d, std::size_t k) noexcept
{
  const auto s0 = _mm256_set1_epi8(d[0]);
  const auto s1 = _mm256_set1_epi8(d[k - 1]);
  for (std::size_t i = 0; i < n; i += 32) {
    const auto b0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + i));
    const auto b1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + i + k - 1));
    const auto e0 = _mm256_cmpeq_epi8(s0, b0);
    const auto e1 = _mm256_cmpeq_epi8(s1, b1);
    auto mask = _mm256_movemask_epi8(_mm256_and_si256(e0, e1));
    while (mask) {
      const auto bitpos = _tzcnt_u32(mask);
      if (std::memcmp(s + i + bitpos + 1, d + 1, k - 2) == 0) {
        return i + bitpos;
      }
      mask &= mask - 1;
    }
  }
  return n;
}

#endif

__forceinline std::size_t search(const char* s, std::size_t n, const char* d, std::size_t k) noexcept
{
  if (n < k) {
    return n;
  }
#ifdef EPOS_USE_AVX2
  auto result = n;
  switch (k) {
  case 0:
    return 0;
  case 1:
    if (const auto p = std::find(s, s + n, d[0])) {
      return p - s;
    } else {
      return n;
    }
  case 2:
    result = avx2_strstr_eq2(s, n, d);
    break;
  case 3:
    result = avx2_strstr_memcmp<3>(s, n, d, memcmp1);
    break;
  case 4:
    result = avx2_strstr_memcmp<4>(s, n, d, memcmp2);
    break;
  case 5:
    result = avx2_strstr_memcmp<5>(s, n, d, memcmp4);
    break;
  case 6:
    result = avx2_strstr_memcmp<6>(s, n, d, memcmp4);
    break;
  case 7:
    result = avx2_strstr_memcmp<7>(s, n, d, memcmp5);
    break;
  case 8:
    result = avx2_strstr_memcmp<8>(s, n, d, memcmp6);
    break;
  case 9:
    result = avx2_strstr_memcmp<9>(s, n, d, memcmp8);
    break;
  case 10:
    result = avx2_strstr_memcmp<10>(s, n, d, memcmp8);
    break;
  case 11:
    result = avx2_strstr_memcmp<11>(s, n, d, memcmp9);
    break;
  case 12:
    result = avx2_strstr_memcmp<12>(s, n, d, memcmp10);
    break;
  default:
    result = avx2_strstr_anysize(s, n, d, k);
    break;
  }
  return result <= n - k ? result : n;
#else
  return std::search(s, s + n, std::boyer_moore_horspool_searcher(d, d + k)) - s;
#endif
}

__forceinline std::size_t search(const char* s, std::size_t n, const char* d, const char* m, std::size_t k) noexcept
{
  std::size_t mask_index = 0;
  const auto compare = [&](char lhs, char rhs) noexcept {
    if ((lhs & m[mask_index++]) == rhs) {
      return true;
    }
    mask_index = 0;
    return false;
  };
  const auto searcher = std::default_searcher(d, d + k, compare);
  return std::search(s, s + n, searcher) - s;
}

}  // namespace

std::size_t signature::find(
  const std::byte* begin,
  const std::byte* end,
  const std::byte* data,
  const std::byte* mask,
  std::size_t size) noexcept
{
  assert(begin && end);
  assert(begin < end);
  assert(data);
  assert(size);
  const auto s = reinterpret_cast<const char*>(begin);
  const auto n = static_cast<std::size_t>(end - begin);
  const auto d = reinterpret_cast<const char*>(data);
  const auto m = reinterpret_cast<const char*>(mask);
  return m ? search(s, n, d, m, size) : search(s, n, d, size);
}

}  // namespace epos