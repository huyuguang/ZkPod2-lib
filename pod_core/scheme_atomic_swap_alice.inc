#include "chain.h"
#include "misc.h"
#include "scheme_misc.h"
#include "tick.h"

namespace scheme::atomic_swap {

template <typename AliceData>
Alice<AliceData>::Alice(std::shared_ptr<AliceData> a, h256_t const& self_id,
                        h256_t const& peer_id)
    : a_(a),
      self_id_(self_id),
      peer_id_(peer_id),
      n_(a_->bulletin().n),
      s_(a_->bulletin().s) {
  seed0_ = misc::RandH256();
  alice_nonce_ = misc::RandH256();
}

template <typename AliceData>
void Alice<AliceData>::BuildMapping() {
  Tick _tick_(__FUNCTION__);
  mappings_.resize(demands_count_);
  size_t index = 0;
  for (auto const& p : demands_) {
    for (size_t i = p.start; i < (p.start + p.count); ++i) {
      mappings_[index++].global_index = i;
    }
  }
}

template <typename AliceData>
bool Alice<AliceData>::OnRequest(Request request, Response& response) {
  Tick _tick_(__FUNCTION__);

  if (!CheckDemands(n_, request.demands)) {
    assert(false);
    return false;
  }

  demands_count_ =
      std::accumulate(request.demands.begin(), request.demands.end(), 0ULL,
                      [](uint64_t t, Range const& i) { return t + i.count; });

  demands_ = std::move(request.demands);
  bob_nonce_ = request.bob_nonce;
  response.alice_nonce = alice_nonce_;

  BuildMapping();

  ChainKeccak256(seed0_, (demands_count_ + 1) * s_, v_);

  if (evil_) {
    // NOTE: use rand() for test
    uint64_t evil_i = rand() % demands_count_;
    uint64_t evil_j = s_ - 1;  // last col
    v_[evil_i * s_ + evil_j] = FrRand();
    std::cout << "evil: " << evil_i << "," << evil_j << "\n";
  }  

  BuildK(v_, response.k, s_);

  std::vector<h256_t> seed2_h{{peer_id_, self_id_, alice_nonce_, bob_nonce_,
                               CalcRangesDigest(demands_),
                               CalcRootOfK(response.k)}};
  seed2_ = CalcSeed2(seed2_h);

  ChainKeccak256(seed2_, demands_count_, w_);
  w_.push_back(FrOne());

  // compute mij' = vij + wi * mij
  auto const& m = a_->m();
  response.m.resize(demands_count_ * s_);

  auto parallel_f = [this, &response,&m](int64_t i) mutable {
    auto const& map = mappings_[i];
    auto is = i * s_;
    auto m_is = map.global_index * s_;
    for (uint64_t j = 0; j < s_; ++j) {
      auto ij = is + j;
      auto m_ij = m_is + j;
      response.m[ij] = v_[ij] + w_[i] * m[m_ij];
    }
  };
  parallel::For((int64_t)mappings_.size(), parallel_f);

  response.vw.resize(s_);
  for (size_t j = 0; j < s_; ++j) {
    response.vw[j] = FrZero();
    for (size_t i = 0; i <= demands_count_; ++i) {
      response.vw[j] += v_[i * s_ + j] * w_[i];
    }
  }

  sigma_vw_ = std::accumulate(response.vw.begin(), response.vw.end(), FrZero());

  return true;
}

template <typename AliceData>
bool Alice<AliceData>::OnReceipt(Receipt const& receipt, Secret& secret) {
  if (receipt.seed2 != seed2_) {
    assert(false);
    return false;
  }
  if (receipt.sigma_vw != sigma_vw_) {
    assert(false);
    return false;
  }
  if (receipt.count != demands_count_) {
    assert(false);
    return false;
  }
  secret.seed0 = seed0_;
  return true;
}

}  // namespace scheme::atomic_swap
