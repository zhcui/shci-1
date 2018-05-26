#pragma once

#include <fgpl/src/hash_map.h>
#include <hps/src/hps.h>
#include <functional>
#include <string>
#include <vector>
#include "det/det.h"

class BaseSystem {
 public:
  unsigned n_up;

  unsigned n_dn;

  unsigned n_elecs;

  double energy_hf;

  double energy_var;

  std::vector<Det> dets;

  std::vector<double> coefs;

  fgpl::HashMap<HalfDet, double, HalfDetHasher> diag_helper;

  size_t get_n_dets() const { return dets.size(); }

  virtual void setup() = 0;

  virtual void find_connected_dets(
      const Det& det,
      const double eps_max,
      const double eps_min,
      const std::function<void(const Det&, const int n_excite)>& handler) const = 0;

  virtual double get_hamiltonian_elem(
      const Det& det_i, const Det& det_j, const int n_excite) const = 0;

  virtual void update_diag_helper() = 0;

  virtual void post_variation(){};

  virtual void post_perturbation(){};

  template <class B>
  void serialize(B& buf) const {
    buf << n_up << n_dn << dets << coefs << energy_hf << energy_var;
  }

  template <class B>
  void parse(B& buf) {
    buf >> n_up >> n_dn >> dets >> coefs >> energy_hf >> energy_var;
  }
};