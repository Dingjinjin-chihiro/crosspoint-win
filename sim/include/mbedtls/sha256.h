#pragma once

// Host stand-in for mbedTLS' SHA-256 API. The firmware uses it for stable ids
// (bookmark sync ids), so this is a real SHA-256 rather than a rolling hash:
// a fake one would change a bookmark's id whenever the hash code changed.

#include <cstddef>
#include <cstdint>
#include <cstring>

typedef struct {
  uint32_t state[8];
  uint64_t bitlen;
  uint8_t buffer[64];
  size_t buffered;
  int is224;
} mbedtls_sha256_context;

namespace sim_sha256_detail {

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

inline const uint32_t* k()
{
  static const uint32_t table[64] = {
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
      0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
      0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
      0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
      0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
      0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
      0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
      0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
  return table;
}

inline void compress(mbedtls_sha256_context* ctx, const uint8_t block[64])
{
  uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (uint32_t(block[i * 4]) << 24) | (uint32_t(block[i * 4 + 1]) << 16) |
           (uint32_t(block[i * 4 + 2]) << 8) | uint32_t(block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
  uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
  const uint32_t* kk = k();
  for (int i = 0; i < 64; ++i) {
    const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    const uint32_t ch = (e & f) ^ (~e & g);
    const uint32_t t1 = h + s1 + ch + kk[i] + w[i];
    const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t t2 = s0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
  ctx->state[4] += e;
  ctx->state[5] += f;
  ctx->state[6] += g;
  ctx->state[7] += h;
}

}  // namespace sim_sha256_detail

inline void mbedtls_sha256_init(mbedtls_sha256_context* ctx)
{
  if (!ctx) return;
  std::memset(ctx, 0, sizeof(*ctx));
}

inline void mbedtls_sha256_free(mbedtls_sha256_context*) {}

inline int mbedtls_sha256_starts(mbedtls_sha256_context* ctx, int is224)
{
  if (!ctx) return -1;
  std::memset(ctx, 0, sizeof(*ctx));
  ctx->is224 = is224 ? 1 : 0;
  static const uint32_t iv256[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
  static const uint32_t iv224[8] = {0xc1059ed8u, 0x367cd507u, 0x3070dd17u, 0xf70e5939u,
                                    0xffc00b31u, 0x68581511u, 0x64f98fa7u, 0xbefa4fa4u};
  for (int i = 0; i < 8; ++i) ctx->state[i] = ctx->is224 ? iv224[i] : iv256[i];
  return 0;
}

inline int mbedtls_sha256_update(mbedtls_sha256_context* ctx, const uint8_t* data, size_t len)
{
  if (!ctx || (!data && len != 0)) return -1;
  for (size_t i = 0; i < len; ++i) {
    ctx->buffer[ctx->buffered++] = data[i];
    if (ctx->buffered == 64) {
      sim_sha256_detail::compress(ctx, ctx->buffer);
      ctx->bitlen += 512;
      ctx->buffered = 0;
    }
  }
  return 0;
}

inline int mbedtls_sha256_finish(mbedtls_sha256_context* ctx, uint8_t out[32])
{
  if (!ctx || !out) return -1;
  const uint64_t bits = ctx->bitlen + uint64_t(ctx->buffered) * 8;
  uint8_t pad[72] = {0x80};
  size_t padlen = (ctx->buffered < 56) ? (56 - ctx->buffered) : (120 - ctx->buffered);
  uint8_t lenbytes[8];
  for (int i = 0; i < 8; ++i) lenbytes[i] = static_cast<uint8_t>(bits >> (56 - i * 8));
  mbedtls_sha256_update(ctx, pad, padlen);
  mbedtls_sha256_update(ctx, lenbytes, 8);

  const size_t words = ctx->is224 ? 7 : 8;
  for (size_t i = 0; i < words; ++i) {
    out[i * 4] = static_cast<uint8_t>(ctx->state[i] >> 24);
    out[i * 4 + 1] = static_cast<uint8_t>(ctx->state[i] >> 16);
    out[i * 4 + 2] = static_cast<uint8_t>(ctx->state[i] >> 8);
    out[i * 4 + 3] = static_cast<uint8_t>(ctx->state[i]);
  }
  return 0;
}

// One-shot helper: the firmware calls this directly (mbedTLS' convenience API).
inline int mbedtls_sha256(const unsigned char* input, size_t ilen, unsigned char output[32], int is224)
{
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, is224);
  mbedtls_sha256_update(&ctx, input, ilen);
  const int rc = mbedtls_sha256_finish(&ctx, output);
  mbedtls_sha256_free(&ctx);
  return rc;
}
