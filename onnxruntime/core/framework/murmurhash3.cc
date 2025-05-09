// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/framework/murmurhash3.h"

// Original source: https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

// Note - The x86 and x64 versions do _not_ produce the same results, as the
// algorithms are optimized for their respective platforms. You can still
// compile and run any of them on any platform, but your performance with the
// non-native version will be less than optimal.

/* Modifications Copyright (c) Microsoft. */

#include "core/framework/endian.h"

#include "core/util/force_inline.h"

//-----------------------------------------------------------------------------
// Platform-specific functions and macros

// Microsoft Visual Studio

#if defined(_MSC_VER)

#include <stdlib.h>

#define ROTL32(x, y) _rotl(x, y)
#define ROTL64(x, y) _rotl64(x, y)

#define BIG_CONSTANT(x) (x)

// Other compilers

#else  // defined(_MSC_VER)

inline uint32_t rotl32(uint32_t x, int8_t r) {
  return (x << r) | (x >> (32 - r));
}

inline uint64_t rotl64(uint64_t x, int8_t r) {
  return (x << r) | (x >> (64 - r));
}

#define ROTL32(x, y) rotl32(x, y)
#define ROTL64(x, y) rotl64(x, y)

#define BIG_CONSTANT(x) (x##LLU)

#endif  // !defined(_MSC_VER)
#include <cstddef>
//-----------------------------------------------------------------------------
// Block read - on little-endian machines this is a single load,
// while on big-endian or unknown machines the byte accesses should
// still get optimized into the most efficient instruction.
//
// Changes to support big-endian from https://github.com/explosion/murmurhash/pull/27/
// were manually applied to original murmurhash3 source code.
ORT_FORCEINLINE uint32_t getblock32(const uint32_t* p, ptrdiff_t i) {
  if constexpr (onnxruntime::endian::native == onnxruntime::endian::little) {
    return p[i];
  } else {
    const uint8_t* c = (const uint8_t*)&p[i];
    return (uint32_t)c[0] |
           (uint32_t)c[1] << 8 |
           (uint32_t)c[2] << 16 |
           (uint32_t)c[3] << 24;
  }
}

ORT_FORCEINLINE uint64_t getblock64(const uint64_t* p, ptrdiff_t i) {
  if constexpr (onnxruntime::endian::native == onnxruntime::endian::little) {
    return p[i];
  } else {
    const uint8_t* c = (const uint8_t*)&p[i];
    return (uint64_t)c[0] |
           (uint64_t)c[1] << 8 |
           (uint64_t)c[2] << 16 |
           (uint64_t)c[3] << 24 |
           (uint64_t)c[4] << 32 |
           (uint64_t)c[5] << 40 |
           (uint64_t)c[6] << 48 |
           (uint64_t)c[7] << 56;
  }
}

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

ORT_FORCEINLINE constexpr uint32_t fmix32(uint32_t h) {
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

  return h;
}

//----------

ORT_FORCEINLINE constexpr uint64_t fmix64(uint64_t k) {
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xff51afd7ed558ccd);
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
  k ^= k >> 33;

  return k;
}

//-----------------------------------------------------------------------------

namespace onnxruntime {
void MurmurHash3::x86_32(const void* key, size_t len,
                         uint32_t seed, void* out) {
  const uint8_t* data = (const uint8_t*)key;
  const auto nblocks = static_cast<ptrdiff_t>(len / 4U);

  uint32_t h1 = seed;

  constexpr uint32_t c1 = 0xcc9e2d51;
  constexpr uint32_t c2 = 0x1b873593;

  //----------
  // body

  const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);

  for (auto i = -nblocks; i; i++) {
    uint32_t k1 = getblock32(blocks, i);

    k1 *= c1;
    k1 = ROTL32(k1, 15);
    k1 *= c2;

    h1 ^= k1;
    h1 = ROTL32(h1, 13);
    h1 = h1 * 5 + 0xe6546b64;
  }

  //----------
  // tail

  const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);

  uint32_t k1 = 0;

  switch (len & 3) {
    case 3:
      k1 ^= tail[2] << 16;
      [[fallthrough]];
    case 2:
      k1 ^= tail[1] << 8;
      [[fallthrough]];
    case 1:
      k1 ^= tail[0];
      k1 *= c1;
      k1 = ROTL32(k1, 15);
      k1 *= c2;
      h1 ^= k1;
  };

  //----------
  // finalization

  h1 ^= len;

  h1 = fmix32(h1);

  *(uint32_t*)out = h1;
}

//-----------------------------------------------------------------------------

void MurmurHash3::x86_128(const void* key, size_t len, uint32_t seed, void* out) {
  const uint8_t* data = (const uint8_t*)key;
  const auto nblocks = static_cast<ptrdiff_t>(len / 16U);

  uint32_t h1 = seed;
  uint32_t h2 = seed;
  uint32_t h3 = seed;
  uint32_t h4 = seed;

  constexpr uint32_t c1 = 0x239b961b;
  constexpr uint32_t c2 = 0xab0e9789;
  constexpr uint32_t c3 = 0x38b34ae5;
  constexpr uint32_t c4 = 0xa1e38b93;

  //----------
  // body

  const uint32_t* blocks = (const uint32_t*)(data + nblocks * 16);

  for (auto i = -nblocks; i; i++) {
    uint32_t k1 = getblock32(blocks, i * 4 + 0);
    uint32_t k2 = getblock32(blocks, i * 4 + 1);
    uint32_t k3 = getblock32(blocks, i * 4 + 2);
    uint32_t k4 = getblock32(blocks, i * 4 + 3);

    k1 *= c1;
    k1 = ROTL32(k1, 15);
    k1 *= c2;
    h1 ^= k1;

    h1 = ROTL32(h1, 19);
    h1 += h2;
    h1 = h1 * 5 + 0x561ccd1b;

    k2 *= c2;
    k2 = ROTL32(k2, 16);
    k2 *= c3;
    h2 ^= k2;

    h2 = ROTL32(h2, 17);
    h2 += h3;
    h2 = h2 * 5 + 0x0bcaa747;

    k3 *= c3;
    k3 = ROTL32(k3, 17);
    k3 *= c4;
    h3 ^= k3;

    h3 = ROTL32(h3, 15);
    h3 += h4;
    h3 = h3 * 5 + 0x96cd1c35;

    k4 *= c4;
    k4 = ROTL32(k4, 18);
    k4 *= c1;
    h4 ^= k4;

    h4 = ROTL32(h4, 13);
    h4 += h1;
    h4 = h4 * 5 + 0x32ac3b17;
  }

  //----------
  // tail

  const uint8_t* tail = (const uint8_t*)(data + nblocks * 16);

  uint32_t k1 = 0;
  uint32_t k2 = 0;
  uint32_t k3 = 0;
  uint32_t k4 = 0;

  switch (len & 15) {
    case 15:
      k4 ^= tail[14] << 16;
      [[fallthrough]];
    case 14:
      k4 ^= tail[13] << 8;
      [[fallthrough]];
    case 13:
      k4 ^= tail[12] << 0;
      k4 *= c4;
      k4 = ROTL32(k4, 18);
      k4 *= c1;
      h4 ^= k4;
      [[fallthrough]];
    case 12:
      k3 ^= tail[11] << 24;
      [[fallthrough]];
    case 11:
      k3 ^= tail[10] << 16;
      [[fallthrough]];
    case 10:
      k3 ^= tail[9] << 8;
      [[fallthrough]];
    case 9:
      k3 ^= tail[8] << 0;
      k3 *= c3;
      k3 = ROTL32(k3, 17);
      k3 *= c4;
      h3 ^= k3;
      [[fallthrough]];
    case 8:
      k2 ^= tail[7] << 24;
      [[fallthrough]];
    case 7:
      k2 ^= tail[6] << 16;
      [[fallthrough]];
    case 6:
      k2 ^= tail[5] << 8;
      [[fallthrough]];
    case 5:
      k2 ^= tail[4] << 0;
      k2 *= c2;
      k2 = ROTL32(k2, 16);
      k2 *= c3;
      h2 ^= k2;
      [[fallthrough]];
    case 4:
      k1 ^= tail[3] << 24;
      [[fallthrough]];
    case 3:
      k1 ^= tail[2] << 16;
      [[fallthrough]];
    case 2:
      k1 ^= tail[1] << 8;
      [[fallthrough]];
    case 1:
      k1 ^= tail[0] << 0;
      k1 *= c1;
      k1 = ROTL32(k1, 15);
      k1 *= c2;
      h1 ^= k1;
  };

  //----------
  // finalization

  h1 ^= len;
  h2 ^= len;
  h3 ^= len;
  h4 ^= len;

  h1 += h2;
  h1 += h3;
  h1 += h4;
  h2 += h1;
  h3 += h1;
  h4 += h1;

  h1 = fmix32(h1);
  h2 = fmix32(h2);
  h3 = fmix32(h3);
  h4 = fmix32(h4);

  h1 += h2;
  h1 += h3;
  h1 += h4;
  h2 += h1;
  h3 += h1;
  h4 += h1;

  ((uint32_t*)out)[0] = h1;
  ((uint32_t*)out)[1] = h2;
  ((uint32_t*)out)[2] = h3;
  ((uint32_t*)out)[3] = h4;
}

}  // namespace onnxruntime
