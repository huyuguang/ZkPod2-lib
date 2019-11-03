#pragma once

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include <cryptopp/keccak.h>
#include <boost/endian/conversion.hpp>

#include "ecc.h"
#include "mpz.h"
#include "tick.h"

inline Fr ChainKeccak256(uint8_t const* seed_buf, uint64_t seed_len,
                         uint64_t index) {
  uint64_t index_be = boost::endian::native_to_big(index);

  h256_t digest_be;
  CryptoPP::Keccak_256 hash;
  hash.Update(seed_buf, seed_len);
  hash.Update((uint8_t const*)&index_be, sizeof(index_be));
  hash.Final(digest_be.data());

  // setArrayMaskMod want little endian
  h256_t digest_le;
  for (size_t i = 0; i < digest_be.size(); ++i) {
    digest_le.data()[i] = digest_be.data()[digest_be.size() - i - 1];
  }

  // use setArray(Mod) instead of setArrayMaskMod because of gas limit
  Fr r;
  bool success = false;
  r.setArray(&success, digest_le.data(), digest_le.size(), mcl::fp::Mod);
  assert(success);
  // r.setArrayMaskMod(digest_le.data(), digest_le.size());

  return r;
}

inline Fr ChainKeccak256(h256_t const& seed, uint64_t index) {
  return ChainKeccak256(seed.data(), seed.size(), index);
}

inline void ChainKeccak256(h256_t const& seed, uint64_t count,
                           std::vector<Fr>& v) {
  Tick _tick_(__FUNCTION__);
  v.resize(count);

#ifdef MULTICORE
#pragma omp parallel for
#endif
  for (int64_t i = 0; i < (int64_t)count; ++i) {
    v[i] = ChainKeccak256(seed, i);
  }
}

inline void ChainKeccak256(h256_t const& seed, uint64_t begin, uint64_t end,
                           std::vector<Fr>& v) {
  Tick _tick_(__FUNCTION__);
  v.resize(end - begin);

#ifdef MULTICORE
#pragma omp parallel for
#endif
  for (int64_t i = begin; i < (int64_t)end; ++i) {
    v[i - begin] = ChainKeccak256(seed, i);
  }
}