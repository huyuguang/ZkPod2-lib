#pragma once

#include "../fst.h"
#include "./details.h"

// a: public vector<Fr>, size = n
// x: secret vector<Fr>, size = n
// y: secret Fr, = <x,a>
// open: com(a), com(y)
// prove: y=<x,a>
// proof size: 2 G1 and n+2 Fr
// prove cost: mulexp(n)
// verify cost: mulexp(n)
namespace hyrax::a2 {

struct ProverInput {
  ProverInput(std::vector<Fr> const& x, std::vector<Fr> const& a) : x(x), a(a) {
    assert(x.size() == a.size() && !a.empty());
    y = InnerProduct(x, a);
  }
  ProverInput(std::vector<Fr> const& x, std::vector<Fr> const& a, Fr const& y)
      : x(x), a(a), y(y) {
    assert(y == InnerProduct(x, a));
  }
  int64_t n() const { return (int64_t)x.size(); }
  std::vector<Fr> const& x;  // x.size = n
  std::vector<Fr> const& a;  // a.size = n
  Fr y;                      // y = <x, a>
};

struct CommitmentPub {
  CommitmentPub() {}
  CommitmentPub(G1 const& xi, G1 const& tau) : xi(xi), tau(tau) {}
  G1 xi;   // com(x,r_xi)
  G1 tau;  // com(y,r_tau)
};

struct CommitmentSec {
  Fr r_xi;
  Fr r_tau;
};

struct CommitmentExtPub {
  CommitmentExtPub() {}
  CommitmentExtPub(G1 const& delta, G1 const& beta)
      : delta(delta), beta(beta) {}
  G1 delta;  // com(d, r_delta)
  G1 beta;   // com(<a,d>, r_beta)
};

inline bool operator==(CommitmentExtPub const& left,
                       CommitmentExtPub const& right) {
  return left.delta == right.delta && left.beta == right.beta;
}

inline bool operator!=(CommitmentExtPub const& left,
                       CommitmentExtPub const& right) {
  return !(left == right);
}

struct CommitmentExtSec {
  std::vector<Fr> d;  // size = n
  Fr r_beta;
  Fr r_delta;
};

struct Proof {
  std::vector<Fr> z;  // z.size = n
  Fr z_delta;
  Fr z_beta;
  int64_t n() const { return (int64_t)z.size(); }
};

inline bool operator==(Proof const& left, Proof const& right) {
  return left.z == right.z && left.z_delta == right.z_delta &&
         left.z_beta == right.z_beta;
}

inline bool operator!=(Proof const& left, Proof const& right) {
  return !(left == right);
}

struct RomProof {
  CommitmentExtPub com_ext_pub;  // 2 G1
  Proof proof;                   // n+2 Fr
  int64_t n() const { return proof.n(); }
};

inline bool operator==(RomProof const& left, RomProof const& right) {
  return left.com_ext_pub == right.com_ext_pub && left.proof == right.proof;
}

inline bool operator!=(RomProof const& left, RomProof const& right) {
  return !(left == right);
}

struct VerifierInput {
  VerifierInput(std::vector<Fr> const& a, CommitmentPub const& com_pub)
      : a(a), com_pub(com_pub) {}
  std::vector<Fr> a;  // a.size = n
  CommitmentPub const& com_pub;
};

// com(n) + com(1) + ip(n)
inline bool VerifyInternal(VerifierInput const& input, Fr const& challenge,
                           CommitmentExtPub const& com_ext_pub,
                           Proof const& proof) {
  // Tick tick(__FUNCTION__);
  using details::ComputeCommitment;

  // std::cout << Tick::GetIndentString() << "multiexp(" << proof.n() << ")\n";

  auto const& com_pub = input.com_pub;

  std::array<parallel::Task, 2> tasks;
  bool ret1 = false;
  tasks[0] = [&ret1, &com_pub, &com_ext_pub, &challenge, &proof]() {
    auto const& xi = com_pub.xi;
    auto const& delta = com_ext_pub.delta;
    G1 left = xi * challenge + delta;
    G1 right = ComputeCommitment(proof.z, proof.z_delta);
    ret1 = left == right;
  };

  bool ret2 = false;
  tasks[1] = [&ret2, &com_pub, &com_ext_pub, &challenge, &proof, &input]() {
    auto const& tau = com_pub.tau;
    auto const& beta = com_ext_pub.beta;
    G1 left = tau * challenge + beta;
    auto ip_za = InnerProduct(proof.z, input.a);
    G1 right = ComputeCommitment(ip_za, proof.z_beta);
    ret2 = left == right;
  };
  parallel::Invoke(tasks, true);

  assert(ret1 && ret2);
  return ret1 && ret2;
}

// com(n) + com(1)
inline void ComputeCom(CommitmentPub& com_pub, CommitmentSec& com_sec,
                       ProverInput const& input) {
  // Tick tick(__FUNCTION__);
  using details::ComputeCommitment;
  com_sec.r_xi = FrRand();
  com_sec.r_tau = FrRand();
  std::array<parallel::Task, 2> tasks;
  tasks[0] = [&com_pub, &input, &com_sec]() {
    com_pub.xi = ComputeCommitment(input.x, com_sec.r_xi);
  };
  tasks[1] = [&com_pub, &input, &com_sec]() {
    com_pub.tau = ComputeCommitment(input.y, com_sec.r_tau);
  };
  parallel::Invoke(tasks, true);
  // std::cout << Tick::GetIndentString() << "multiexp(" << input.n() << ")\n";
}

// com(n) + com(1) + ip(n)
inline void ComputeCommitmentExt(CommitmentExtPub& com_ext_pub,
                                 CommitmentExtSec& com_ext_sec,
                                 ProverInput const& input) {
  // Tick tick(__FUNCTION__);
  using details::ComputeCommitment;
  auto n = input.n();
  com_ext_sec.d.resize(n);
  FrRand(com_ext_sec.d.data(), n);
  com_ext_sec.r_beta = FrRand();
  com_ext_sec.r_delta = FrRand();

  std::array<parallel::Task, 2> tasks;
  tasks[0] = [&com_ext_pub, &com_ext_sec]() {
    com_ext_pub.delta = ComputeCommitment(com_ext_sec.d, com_ext_sec.r_delta);
  };
  tasks[1] = [&com_ext_pub, &input, &com_ext_sec]() {
    com_ext_pub.beta = ComputeCommitment(InnerProduct(input.a, com_ext_sec.d),
                                         com_ext_sec.r_beta);
  };
  parallel::Invoke(tasks, true);

  // std::cout << Tick::GetIndentString() << "multiexp(" << input.n() << ")\n";
}

inline void UpdateSeed(h256_t& seed, CommitmentPub const& com_pub,
                       CommitmentExtPub const& com_ext_pub) {
  CryptoPP::Keccak_256 hash;
  HashUpdate(hash, seed);
  HashUpdate(hash, com_pub.xi);
  HashUpdate(hash, com_pub.tau);
  HashUpdate(hash, com_ext_pub.beta);
  HashUpdate(hash, com_ext_pub.delta);
  hash.Final(seed.data());
}

inline void ComputeProof(Proof& proof, ProverInput const& input,
                         CommitmentSec const& com_sec,
                         CommitmentExtSec const& com_ext_sec,
                         Fr const& challenge) {
  // z = c * x + d
  proof.z = input.x * challenge + com_ext_sec.d;
  proof.z_delta = challenge * com_sec.r_xi + com_ext_sec.r_delta;
  proof.z_beta = challenge * com_sec.r_tau + com_ext_sec.r_beta;
}

inline void RomProve(RomProof& rom_proof, h256_t const& common_seed,
                     ProverInput input, CommitmentPub com_pub,
                     CommitmentSec com_sec) {
  // Tick tick(__FUNCTION__);

  assert(PdsPub::kGSize >= input.n());

  CommitmentExtSec com_ext_sec;
  ComputeCommitmentExt(rom_proof.com_ext_pub, com_ext_sec, input);

  auto seed = common_seed;
  UpdateSeed(seed, com_pub, rom_proof.com_ext_pub);
  Fr challenge = H256ToFr(seed);

  ComputeProof(rom_proof.proof, input, com_sec, com_ext_sec, challenge);
}

inline bool RomVerify(RomProof const& rom_proof, h256_t const& common_seed,
                      VerifierInput const& input) {
  // Tick tick(__FUNCTION__);
  assert(PdsPub::kGSize >= rom_proof.n());
  if (input.a.size() != rom_proof.proof.z.size() || input.a.empty())
    return false;

  auto seed = common_seed;
  UpdateSeed(seed, input.com_pub, rom_proof.com_ext_pub);
  Fr challenge = H256ToFr(seed);

  return VerifyInternal(input, challenge, rom_proof.com_ext_pub,
                        rom_proof.proof);
}

inline bool TestRom(int64_t n) {
  std::vector<Fr> x(n);
  FrRand(x.data(), n);
  std::vector<Fr> a(n);
  FrRand(a.data(), n);

  h256_t UpdateSeed = misc::RandH256();

  ProverInput prover_input(x, a);

  CommitmentPub com_pub;
  CommitmentSec com_sec;
  ComputeCom(com_pub, com_sec, prover_input);

  RomProof rom_proof;
  RomProve(rom_proof, UpdateSeed, prover_input, com_pub, com_sec);

  VerifierInput verifier_input(a, com_pub);
  return RomVerify(rom_proof, UpdateSeed, verifier_input);
}
}  // namespace hyrax::a2