#include "misc.h"
#include "scheme_misc.h"
#include "tick.h"

namespace scheme::atomic_swap {

template <typename BobData>
Bob<BobData>::Bob(std::shared_ptr<BobData> b, h256_t const& self_id,
                  h256_t const& peer_id, std::vector<Range> demands)
    : b_(b),
      self_id_(self_id),
      peer_id_(peer_id),
      n_(b_->bulletin().n),
      s_(b_->bulletin().s),
      demands_(std::move(demands)) {

  if (!CheckDemands(n_, demands_)) {
    throw std::invalid_argument("invalid demands");
  }

  demands_count_ =
      std::accumulate(demands_.begin(), demands_.end(), 0ULL,
                      [](uint64_t a, Range const& b) { return a + b.count; });

  BuildMapping();

  bob_nonce_ = misc::RandH256();
}

template <typename BobData>
void Bob<BobData>::BuildMapping() {
  Tick _tick_(__FUNCTION__);
  mappings_.resize(demands_count_);
  size_t index = 0;
  for (auto const& d : demands_) {
    for (size_t i = d.start; i < (d.start + d.count); ++i) {
      auto& map = mappings_[index];
      map.global_index = i;
      ++index;
    }
  }
}

template <typename BobData>
void Bob<BobData>::GetRequest(Request& request) {
  request.bob_nonce = bob_nonce_;
  request.demands = demands_;
}

template <typename BobData>
bool Bob<BobData>::OnResponse(Response response, Receipt& receipt) {
  Tick _tick_(__FUNCTION__);

  if (response.k.size() != (demands_count_ + 1)) {
    assert(false);
    return false;
  }
  if (response.m.size() != demands_count_ * s_) {
    assert(false);
    return false;
  }
  if (response.vw.size() != s_) {
    assert(false);
    return false;
  }

  k_ = std::move(response.k);

  vw_ = std::move(response.vw);

  alice_nonce_ = response.alice_nonce;

  std::vector<h256_t> seed2_h{{self_id_, peer_id_, alice_nonce_, bob_nonce_,
                               CalcRangesDigest(demands_), CalcRootOfK(k_)}};
  seed2_ = CalcSeed2(seed2_h);

  ChainKeccak256(seed2_, demands_count_, w_);
  w_.push_back(FrOne());

  encrypted_m_ = std::move(response.m);

  if (!CheckEncryptedM()) {
    assert(false);
    return false;
  }

  if (!CheckKVW()) {
    assert(false);
    return false;
  }

  sigma_vw_ = std::accumulate(vw_.begin(), vw_.end(), FrZero());

  receipt.count = demands_count_;
  receipt.seed2 = seed2_;
  receipt.sigma_vw = sigma_vw_;

  return true;
}

template <typename BobData>
bool Bob<BobData>::CheckEncryptedM() {
  Tick _tick_(__FUNCTION__);

  auto const& sigmas = b_->sigmas();

  // uint64_t phantom_offset = phantom_.start - demand_.start;
  std::vector<int64_t> rets(mappings_.size());
  auto parallel_f = [this, &rets, &sigmas](int64_t i) mutable {
    auto const& mapping = mappings_[i];
    G1 const& sigma = sigmas[mapping.global_index];
    G1 left = sigma * w_[i] + k_[i];
    G1 right = MultiExpU1(s_, [i, this](uint64_t j) -> Fr const& {
      return encrypted_m_[i * s_ + j];
    });

    rets[i] = left == right;
  };
  parallel::For((int64_t)mappings_.size(), parallel_f);


  if (std::any_of(rets.begin(), rets.end(), [](int64_t r) { return !r; })) {
    assert(false);
    std::cerr << "ASSERT: " << __FUNCTION__ << ": " << __LINE__ << "\n";
    return false;
  }
  return true;
}

template <typename BobData>
bool Bob<BobData>::CheckKVW() {
  Tick _tick_(__FUNCTION__);

  G1 right = MultiExpU1(s_, [this](uint64_t j) -> Fr const& { return vw_[j]; });

  G1 left = MultiExpBdlo12(k_, w_);

  if (right != left) {
    assert(false);
    std::cerr << "ASSERT: " << __FUNCTION__ << ": " << __LINE__ << "\n";
    return false;
  }
  return true;
}

template <typename BobData>
bool Bob<BobData>::OnSecret(Secret const& secret) {
  Tick _tick_(__FUNCTION__);

  // compute v
  std::vector<Fr> v;
  ChainKeccak256(secret.seed0, (demands_count_ + 1) * s_, v);

  if (!VerifySecret(s_, demands_count_, sigma_vw_, v, w_)) {
    // assert(false);
    return false;
  }

  DecryptM(v);

  return true;
}

template <typename BobData>
void Bob<BobData>::DecryptM(std::vector<Fr> const& v) {
  Tick _tick_(__FUNCTION__);

  std::vector<Fr> inv_w = w_;
  FrInv(inv_w.data(), inv_w.size());

  auto parallel_f = [this, &inv_w, &v](int64_t i) mutable {
    auto is = i * s_;
    for (uint64_t j = 0; j < s_; ++j) {
      auto ij = is + j;
      encrypted_m_[ij] = (encrypted_m_[ij] - v[ij]) * inv_w[i];
    }
  };
  parallel::For((int64_t)mappings_.size(), parallel_f);

  decrypted_m_ = std::move(encrypted_m_);
}

template <typename BobData>
bool Bob<BobData>::SaveDecrypted(std::string const& file) {
  Tick _tick_(__FUNCTION__);
  return b_->SaveDecryped(file, demands_, decrypted_m_);
}
}  // namespace scheme::atomic_swap
