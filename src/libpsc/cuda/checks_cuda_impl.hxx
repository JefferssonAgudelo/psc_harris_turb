
#pragma once

#include "checks.hxx"

#include "fields_item_moments_1st_cuda.hxx"

// FIXME!!! need to get Dim from Mparticles directly!

template <typename BS>
struct BS_to_Dim;

template <>
struct BS_to_Dim<BS144>
{
  using Dim = dim_yz;
};

template <>
struct BS_to_Dim<BS444>
{
  using Dim = dim_xyz;
};

template <typename Mparticles>
struct ChecksCuda
  : ChecksBase
  , ChecksParams
{
  using MfieldsState = MfieldsStateSingle;
  using Mfields = MfieldsSingle;
  using BS = typename Mparticles::BS;
  using Dim = typename BS_to_Dim<BS>::Dim;
  using Moment_t = Moment_rho_1st_nc_cuda<Mparticles, Dim>;

  ChecksCuda(const Grid_t& grid, MPI_Comm comm, const ChecksParams& params)
    : ChecksParams(params),
      item_rho_{grid},
      item_rho_m_{grid},
      item_rho_p_{grid}
  {}

  void continuity_before_particle_push(Mparticles& mprts)
  {
    const auto& grid = mprts.grid();
    if (continuity_every_step <= 0 ||
        grid.timestep() % continuity_every_step != 0) {
      return;
    }

    item_rho_m_(mprts);
  }

  void continuity_after_particle_push(Mparticles& mprts,
                                      MfieldsStateCuda& mflds)
  {
    const auto& grid = mprts.grid();
    if (continuity_every_step <= 0 ||
        grid.timestep() % continuity_every_step != 0) {
      return;
    }

    item_rho_p_(mprts);

    auto& h_mflds = mflds.get_as<MfieldsState>(0, mflds._n_comps());
    auto item_divj = Item_divj<MfieldsState>(h_mflds);

    auto& dev_rho_p = item_rho_p_.result();
    auto& dev_rho_m = item_rho_m_.result();
    auto& rho_p = dev_rho_p.template get_as<Mfields>(0, 1);
    auto& rho_m = dev_rho_m.template get_as<Mfields>(0, 1);

    auto&& d_rho = view_interior(rho_p.gt(), rho_p.ibn()) -
                   view_interior(rho_m.gt(), rho_m.ibn());
    auto&& dt_divj = grid.dt * item_divj.gt();

    double eps = continuity_threshold;
    double max_err = 0.;
    for (int p = 0; p < grid.n_patches(); p++) {
      grid.Foreach_3d(0, 0, [&](int i, int j, int k) {
        double _d_rho = d_rho(i, j, k, 0, p);
        double _dt_divj = dt_divj(i, j, k, 0, p);
        max_err = std::max(max_err, std::abs(_d_rho + _dt_divj));
        if (std::abs(_d_rho + _dt_divj) > eps) {
          mprintf("p%d (%d,%d,%d): %g -- %g diff %g\n", p, i, j, k, _d_rho,
                  -_dt_divj, _d_rho + _dt_divj);
        }
      });
    }

    // find global max
    double tmp = max_err;
    MPI_Allreduce(&tmp, &max_err, 1, MPI_DOUBLE, MPI_MAX, grid.comm());

    if (continuity_verbose || max_err >= eps) {
      mpi_printf(grid.comm(), "continuity: max_err = %g (thres %g)\n", max_err,
                 eps);
    }

    if (continuity_dump_always || max_err >= eps) {
      static WriterMRC writer;
      if (!writer) {
        writer.open("continuity");
      }
      writer.begin_step(grid.timestep(), grid.timestep() * grid.dt);
      writer.write(dt_divj, grid, "div_j", {"div_j"});
      writer.write(d_rho, grid, "d_rho", {"d_rho"});
      writer.end_step();
    }

    assert(max_err < eps);
    dev_rho_p.put_as(rho_p, 0, 0);
    dev_rho_m.put_as(rho_m, 0, 0);
    mflds.put_as(h_mflds, 0, 0);
  }

  // ======================================================================
  // psc_checks: Gauss's Law

  // ----------------------------------------------------------------------
  // gauss

  void gauss(Mparticles& mprts, MfieldsStateCuda& mflds)
  {
    const auto& grid = mprts.grid();
    if (gauss_every_step <= 0 || grid.timestep() % gauss_every_step != 0) {
      return;
    }

    item_rho_(mprts);

    auto& h_mflds = mflds.get_as<MfieldsState>(0, mflds._n_comps());

    auto dive = Item_dive<MfieldsState>(h_mflds);
    auto& dev_rho = item_rho_.result();

    auto& rho = dev_rho.template get_as<Mfields>(0, 1);
    double eps = gauss_threshold;
    double max_err = 0.;
    for (int p = 0; p < dive.n_patches(); p++) {
      auto Rho = make_Fields3d<dim_xyz>(rho[p]);

      int l[3] = {0, 0, 0}, r[3] = {0, 0, 0};
      for (int d = 0; d < 3; d++) {
        if (grid.bc.fld_lo[d] == BND_FLD_CONDUCTING_WALL &&
            grid.atBoundaryLo(p, d)) {
          l[d] = 1;
        }
      }

      grid.Foreach_3d(0, 0, [&](int jx, int jy, int jz) {
        if (jy < l[1] || jz < l[2] || jy >= grid.ldims[1] - r[1] ||
            jz >= grid.ldims[2] - r[2]) {
          // nothing
        } else {
          double v_rho = Rho(0, jx, jy, jz);
          double v_dive = dive(0, {jx, jy, jz}, p);
          max_err = fmax(max_err, fabs(v_dive - v_rho));
#if 1
          if (fabs(v_dive - v_rho) > eps) {
            printf("(%d,%d,%d): %g -- %g diff %g\n", jx, jy, jz, v_dive, v_rho,
                   v_dive - v_rho);
          }
#endif
        }
      });
    }

    // find global max
    double tmp = max_err;
    MPI_Allreduce(&tmp, &max_err, 1, MPI_DOUBLE, MPI_MAX, grid.comm());

    if (gauss_verbose || max_err >= eps) {
      mpi_printf(grid.comm(), "gauss: max_err = %g (thres %g)\n", max_err, eps);
    }

    if (gauss_dump_always || max_err >= eps) {
      static WriterMRC writer;
      if (!writer) {
        writer.open("gauss");
      }
      writer.begin_step(grid.timestep(), grid.timestep() * grid.dt);
      {
        Int3 bnd = rho.ibn();
        writer.write(rho.gt().view(_s(bnd[0], -bnd[0]), _s(bnd[1], -bnd[1]),
                                   _s(bnd[2], -bnd[2])),
                     grid, "rho", {"rho"});
      }
      {
        Int3 bnd = dive.ibn();
        writer.write(dive.gt().view(_s(bnd[0], -bnd[0]), _s(bnd[1], -bnd[1]),
                                    _s(bnd[2], -bnd[2])),
                     dive.grid(), dive.name(), dive.comp_names());
      }
      writer.end_step();
    }

    assert(max_err < eps);
    dev_rho.put_as(rho, 0, 0);
    mflds.put_as(h_mflds, 0, 0);
  }

private:
  Moment_t item_rho_p_;
  Moment_t item_rho_m_;
  Moment_t item_rho_;
};
