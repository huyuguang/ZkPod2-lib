#pragma once

#include "./vrs_prover.h"
#include "./vrs_verifier.h"
#include "./vrs_misc.h"
#include "./vrs_cache.h"

namespace vrs {

namespace details {
template <typename Output>
void MergeOutputs(Output& output, std::vector<Output> const& outputs) {
  output.h = outputs[0].h;
  output.g = std::accumulate(
      outputs.begin(), outputs.end(), G1Zero(),
      [](G1 const& a, Output const& b) { return a + b.g; });
  output.key_com = std::accumulate(
      outputs.begin(), outputs.end(), G1Zero(),
      [](G1 const& a, Output const& b) { return a + b.key_com; });
}
}  // namespace details

class LargeProver {
 public:
  LargeProver(PublicInput const& public_input, SecretInput const& secret_input,
              std::vector<std::vector<G1>> cached_var_coms,
              std::vector<std::vector<Fr>> cached_var_coms_r)
      : public_input_(public_input),
        secret_input_(secret_input) {
    items_ = SplitLargeTask(public_input_.count);
    
    assert(cached_var_coms.size() == cached_var_coms_r.size());

    auto secret_inputs = BuildSecretInputs(cached_var_coms_r);

    if (cached_var_coms.size() != items_.size()) {
      assert(cached_var_coms.empty());
      cached_var_coms.clear();
      cached_var_coms.resize(items_.size());
      cached_var_coms_r.resize(items_.size());
    }
    provers_.resize(items_.size());
    for (int64_t i = 0; i < (int64_t)provers_.size(); ++i) {
      auto const& item = items_[i];
      PublicInput this_input(item.second - item.first,
                             [&item, this](int64_t j) {
                               return public_input_.get_p(item.first + j);
                             });

      auto& cached_var_com = cached_var_coms[i];
      auto& cached_var_com_r = cached_var_coms_r[i];
      provers_[i].reset(new Prover(this_input, secret_inputs[i],
                                   std::move(cached_var_com),
                                   std::move(cached_var_com_r)));
    }
  }

  void Evaluate() {
    v_.resize(public_input_.count);
#ifdef MULTICORE
#pragma omp parallel for
#endif
    for (int64_t i = 0; i < (int64_t)provers_.size(); ++i) {
      provers_[i]->Evaluate();
      auto const& v = provers_[i]->v();
      std::copy(v.begin(), v.end(), v_.begin() + i * kMaxUnitPerZkp);
    }
  }

  void Prove(h256_t const& rom_seed, std::function<Fr(int64_t)> get_w,
             std::vector<Proof>& proofs, ProveOutput& output) {
    auto size = (int64_t)provers_.size();
    proofs.resize(size);
    std::vector<ProveOutput> outputs(size);
    std::vector<Fr> vws(size);

#ifdef MULTICORE
#pragma omp parallel for
#endif    
    for (int64_t i = 0; i < size; ++i) {
      auto const& item = items_[i];
      auto this_get_w = [&item, &get_w](int64_t j) {
        return get_w(j + item.first);
      };
      provers_[i]->Prove(rom_seed, std::move(this_get_w), proofs[i],
                         outputs[i]);
      vws[i] = provers_[i]->vw();
      provers_[i].reset();
    }

    details::MergeOutputs(output, outputs);

    vw_ = std::accumulate(vws.begin(), vws.end(), FrZero());

#ifdef _DEBUG
    auto com_vw1 = groth09::details::ComputeCommitment(vw_,
                                                       secret_input_.vw_com_r);
    auto op = [](G1 const& a, Proof const& b) { return a + b.com_vw; };
    auto com_vw2 =
        std::accumulate(proofs.begin(), proofs.end(), G1Zero(), std::move(op));
    assert(com_vw1 == com_vw2);

    auto com_key =
        output.h * secret_input_.key_com_r + output.g * secret_input_.key;
    assert(com_key == output.key_com);
#endif              
  }

  Fr const& vw() const { return vw_; }

  std::vector<Fr> const& v() const { return v_; }

  std::vector<Fr>&& TakeV() { return std::move(v_); }

 private:
  std::vector<SecretInput> BuildSecretInputs(
    std::vector<std::vector<Fr>> const& cached_var_coms_r) {
    auto size = (int64_t)items_.size();
    std::vector<SecretInput> ret(size);
    auto vw_com_rs = SplitFr(secret_input_.vw_com_r, size);
    for (int64_t i = 0; i < size; ++i) {
      ret[i].key = secret_input_.key;
      ret[i].vw_com_r = vw_com_rs[i];
    }

    if (cached_var_coms_r.empty()) {
      auto key_com_rs = SplitFr(secret_input_.key_com_r, size);
      for (int64_t i = 0; i < size; ++i) {
        ret[i].key_com_r = key_com_rs[i];
      }
    } else {
      assert((int64_t)cached_var_coms_r.size() == size);
      for (int64_t i = 0; i < size; ++i) {
        ret[i].key_com_r = cached_var_coms_r[i][primary_input_size_];
      }
    }
    
    return ret;
  }
 private:
   int64_t const primary_input_size_ = 1;
  PublicInput public_input_;
  SecretInput secret_input_;
  std::vector<std::unique_ptr<Prover>> provers_;
  std::vector<std::pair<int64_t, int64_t>> items_;
  Fr vw_;
  std::vector<Fr> v_;
};

class LargeVerifier {
 public:
  LargeVerifier(PublicInput const& public_input)
      : public_input_(public_input) {
    auto count = public_input_.count;
    items_.resize((count + kMaxUnitPerZkp - 1) / kMaxUnitPerZkp);
    for (int64_t i = 0; i < (int64_t)items_.size(); ++i) {
      auto& item = items_[i];
      item.first = i * kMaxUnitPerZkp;
      item.second = item.first + kMaxUnitPerZkp;
      if (item.second > public_input.count) {
        item.second = public_input.count;
      }
    }
    auto pair_size = [](std::pair<int64_t, int64_t> const& p) {
      return p.second - p.first;
    };

    verifiers_.resize(items_.size());
    for (int64_t i = 0; i < (int64_t)verifiers_.size(); ++i) {
      auto const& item = items_[i];
      PublicInput this_input(pair_size(item), [&item, this](int64_t j) {
        return public_input_.get_p(item.first + j);
      });
      verifiers_[i].reset(new Verifier(this_input));
    }
  }

  bool Verify(h256_t const& rom_seed, std::function<Fr(int64_t)> get_w,
             std::vector<Proof> const& proofs, VerifyOutput& output) {
    if (proofs.size() != verifiers_.size()) return false;
    auto size = (int64_t)verifiers_.size();
    std::vector<VerifyOutput> outputs(size);

    std::vector<int64_t> rets(size);

#ifdef MULTICORE
#pragma omp parallel for
#endif
    for (int64_t i = 0; i < size; ++i) {
      auto const& item = items_[i];
      auto this_get_w = [&item, &get_w](int64_t j) {
        return get_w(j + item.first);
      };
      rets[i] = verifiers_[i]->Verify(rom_seed, std::move(this_get_w),
                                      proofs[i], outputs[i]);
      verifiers_[i].reset();
    }

    if (std::any_of(rets.begin(), rets.end(), [](int64_t r) { return !r; }))
      return false;

    details::MergeOutputs(output, outputs);

    com_vw_ = std::accumulate(
        proofs.begin(), proofs.end(), G1Zero(),
        [](G1 const& a, Proof const& b) { return a + b.com_vw; });

    return true;
  }

  G1 const& com_vw() const { return com_vw_; }

 private:
   PublicInput public_input_;
  std::vector<std::unique_ptr<Verifier>> verifiers_;
  std::vector<std::pair<int64_t, int64_t>> items_;
  G1 com_vw_;
};
}  // namespace vrs