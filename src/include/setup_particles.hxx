
#pragma once

#include "centering.hxx"

struct psc_particle_npt
{
  int kind;    ///< particle kind
  double n;    ///< density
  double p[3]; ///< momentum
  double T[3]; ///< temperature
  psc::particle::Tag tag;
};

// ======================================================================
// SetupParticles

namespace
{
const Centering::Centerer defaultCenterer(Centering::CC);
}

template <typename MP>
struct SetupParticles
{
  using Mparticles = MP;
  using real_t = typename MP::real_t;

  SetupParticles(const Grid_t& grid, int n_populations = 0)
    : kinds_{grid.kinds},
      norm_{grid.norm},
      n_populations_{n_populations},
      centerer(defaultCenterer)
  {
    if (n_populations_ == 0) {
      n_populations_ = kinds_.size();
    }
  }

  // ----------------------------------------------------------------------
  // get_n_in_cell
  //
  // helper function for partition / particle setup

  int get_n_in_cell(const psc_particle_npt& npt)
  {
    if (npt.n == 0) {
      return 0;
    }
    if (fractional_n_particles_per_cell) {
      int n_prts = npt.n / norm_.cori;
      float rmndr = npt.n / norm_.cori - n_prts;
      float ran = random() / ((float)RAND_MAX + 1);
      if (ran < rmndr) {
        n_prts++;
      }
      return n_prts;
    }
    return npt.n / norm_.cori + .5;
  }

  // ----------------------------------------------------------------------
  // op_cellwise
  // Performs a given operation in each cell.
  // init_npt signature: (int kind, Double3 pos, int p, Int3 index, npt) -> void
  // op signature: (int n_in_cell, npt, Double3 pos) -> void

  template <typename InitFunc, typename OpFunc>
  void op_cellwise(const Grid_t& grid, int patch, InitFunc&& init_npt,
                   OpFunc&& op)
  {
    Int3 ilo = Int3{}, ihi = grid.ldims;

    for (int jz = ilo[2]; jz < ihi[2]; jz++) {
      for (int jy = ilo[1]; jy < ihi[1]; jy++) {
        for (int jx = ilo[0]; jx < ihi[0]; jx++) {
          Int3 index{jx, jy, jz};

          Double3 pos = centerer.getPos(grid.patches[patch], index);
          // FIXME, the issue really is that (2nd order) particle pushers
          // don't handle the invariant dim right
          for (int a = 0; a < 3; ++a) {
            if (grid.isInvar(a) == 1) {
              pos[a] = grid.patches[patch].get_nc(index[a], a);
            }
          }

          int n_q_in_cell = 0;
          for (int pop = 0; pop < n_populations_; pop++) {
            psc_particle_npt npt{};
            if (pop < kinds_.size()) {
              npt.kind = pop;
            }
            init_npt(pop, pos, patch, index, npt);

            int n_in_cell;
            if (pop != neutralizing_population) {
              n_in_cell = get_n_in_cell(npt);
              n_q_in_cell += kinds_[npt.kind].q * n_in_cell;
            } else {
              // FIXME, should handle the case where not the last population
              // is neutralizing
              assert(neutralizing_population == n_populations_ - 1);
              n_in_cell = -n_q_in_cell / kinds_[npt.kind].q;
            }
            op(n_in_cell, npt, pos);
          }
        }
      }
    }
  }

  // ----------------------------------------------------------------------
  // setupParticle

  psc::particle::Inject setupParticle(const psc_particle_npt& npt, Double3 pos,
                                      double wni)
  {
    double beta = norm_.beta;

    assert(npt.kind >= 0 && npt.kind < kinds_.size());
    double m = kinds_[npt.kind].m;

    float ran1, ran2, ran3, ran4, ran5, ran6;
    do {
      ran1 = random() / ((float)RAND_MAX + 1);
      ran2 = random() / ((float)RAND_MAX + 1);
      ran3 = random() / ((float)RAND_MAX + 1);
      ran4 = random() / ((float)RAND_MAX + 1);
      ran5 = random() / ((float)RAND_MAX + 1);
      ran6 = random() / ((float)RAND_MAX + 1);
    } while (ran1 >= 1.f || ran2 >= 1.f || ran3 >= 1.f || ran4 >= 1.f ||
             ran5 >= 1.f || ran6 >= 1.f);

    double pxi =
      npt.p[0] + sqrtf(-2.f * npt.T[0] / m * sqr(beta) * logf(1.0 - ran1)) *
                   cosf(2.f * M_PI * ran2);
    double pyi =
      npt.p[1] + sqrtf(-2.f * npt.T[1] / m * sqr(beta) * logf(1.0 - ran3)) *
                   cosf(2.f * M_PI * ran4);
    double pzi =
      npt.p[2] + sqrtf(-2.f * npt.T[2] / m * sqr(beta) * logf(1.0 - ran5)) *
                   cosf(2.f * M_PI * ran6);

    if (initial_momentum_gamma_correction) {
      double gam;
      if (sqr(pxi) + sqr(pyi) + sqr(pzi) < 1.) {
        gam = 1. / sqrt(1. - sqr(pxi) - sqr(pyi) - sqr(pzi));
        pxi *= gam;
        pyi *= gam;
        pzi *= gam;
      }
    }

    return psc::particle::Inject{pos, {pxi, pyi, pzi}, wni, npt.kind, npt.tag};
  }

  // ----------------------------------------------------------------------
  // setup_particles
  // init_npt signature: (int kind, Double3 pos, npt) -> void

  template <typename FUNC>
  void operator()(Mparticles& mprts, FUNC&& init_npt)
  {
    setupParticles(mprts,
                   [&](int kind, Double3 pos, int p, Int3 idx,
                       psc_particle_npt& npt) { init_npt(kind, pos, npt); });
  }

  // ----------------------------------------------------------------------
  // setupParticles
  // init_npt signature: (int kind, Double3 pos, int p, Int3 index, npt) -> void

  template <typename FUNC>
  void setupParticles(Mparticles& mprts, FUNC&& init_npt)
  {
    static int pr;
    if (!pr) {
      pr = prof_register("setupp", 1., 0, 0);
    }

    prof_start(pr);
    const auto& grid = mprts.grid();

    // mprts.reserve_all(n_prts_by_patch); FIXME

    auto inj = mprts.injector();

    for (int p = 0; p < mprts.n_patches(); ++p) {
      auto injector = inj[p];

      op_cellwise(grid, p, std::forward<FUNC>(init_npt),
                  [&](int n_in_cell, psc_particle_npt& npt, Double3& pos) {
                    for (int cnt = 0; cnt < n_in_cell; cnt++) {
                      real_t wni;
                      if (fractional_n_particles_per_cell) {
                        wni = 1.;
                      } else {
                        wni = npt.n / (n_in_cell * norm_.cori);
                      }
                      auto prt = setupParticle(npt, pos, wni);
                      injector(prt);
                    }
                  });
    }

    prof_stop(pr);
  }

  // ----------------------------------------------------------------------
  // partition
  // init_npt signature: (int kind, Double3 pos, npt) -> void

  template <typename FUNC>
  std::vector<uint> partition(const Grid_t& grid, FUNC&& init_npt)
  {
    return partition_general_init(
      grid, [&](int kind, Double3 pos, int, Int3, psc_particle_npt& npt) {
        init_npt(kind, pos, npt);
      });
  }

  // ----------------------------------------------------------------------
  // partition_general_init
  // init_npt signature: (int kind, Double3 pos, int p, Int3 index, npt) -> void

  template <typename FUNC>
  std::vector<uint> partition_general_init(const Grid_t& grid, FUNC&& init_npt)
  {
    std::vector<uint> n_prts_by_patch(grid.n_patches());

    for (int p = 0; p < grid.n_patches(); ++p) {
      op_cellwise(grid, p, std::forward<FUNC>(init_npt),
                  [&](int n_in_cell, psc_particle_npt&, Double3&) {
                    n_prts_by_patch[p] += n_in_cell;
                  });
    }

    return n_prts_by_patch;
  }

  // the initial number of particles in a cell for this population will be st so
  // that it achieves neutrality
  int neutralizing_population = {-1};
  bool fractional_n_particles_per_cell = {false};
  bool initial_momentum_gamma_correction = {false};

  Centering::Centerer centerer;

private:
  const Grid_t::Kinds kinds_;
  const Grid_t::Normalization norm_;
  int n_populations_;
};
