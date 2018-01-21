
#include "grid.hxx"
#include "fields.hxx"
#include "cuda_mfields.h"
#include "cuda_mparticles.h"

#include "../vpic/PscRng.h"

using Rng = PscRng;
using RngPool = PscRngPool<Rng>;

#include "mrc_profile.h"

enum IP { // FIXME, dup
  IP_STD, // standard interpolation
  IP_EC,  // energy-conserving interpolation
};

enum DEPOSIT { // FIXME, dup
  DEPOSIT_VB_2D,
  DEPOSIT_VB_3D,
};

enum CURRMEM { // FIXME, dup
  CURRMEM_SHARED,
  CURRMEM_GLOBAL,
};

struct prof_globals prof_globals; // FIXME

int
prof_register(const char *name, float simd, int flops, int bytes)
{
  return 0;
}

class TestAccel
{
  enum { // FIXME, duplicated
    JXI, JYI, JZI,
    EX , EY , EZ ,
    HX , HY , HZ ,
    N_FIELDS,
  };

public:
  TestAccel()
  {
    bs_ = { 1, 1, 1 };
    init_grid();
    init_cmflds();
    init_cmprts();
  }

  ~TestAccel()
  {
    delete cmflds_;
    delete cmprts_;
  }

  void init_grid()
  {
    grid_.dt = 1.;
    grid_.fnqs = 1.;
    grid_.eta = 1.;

    Grid_t::Patch patch{};
    grid_.ldims = { 1, 1, 1 };
    grid_.dx = { L, L, L };
    
    patch.xb = { 0., 0., 0. };
    patch.xe = { L,  L,  L  };
    grid_.patches.push_back(patch);

    grid_.kinds.push_back(Grid_t::Kind(1.,  1., "test_species"));
  }
  
  void init_cmflds()
  {
    cmflds_ = new cuda_mfields(grid_, N_FIELDS, { 0, 2, 2 });
    fields_single_t flds = cmflds_->get_host_fields();
    Fields3d<fields_single_t> F(flds);
    F(EX, 0,0,0) = 1;
    F(EX, 0,1,0) = 1;
    F(EX, 0,0,1) = 1;
    F(EX, 0,1,1) = 1;
    
    F(EY, 0,0,0) = 2;
    F(EY, 0,0,1) = 2;
    //    F(EY, 1,0,0) = 2;
    //    F(EY, 1,0,1) = 2;
    
    F(EZ, 0,0,0) = 3;
    //    F(EZ, 1,0,0) = 3;
    F(EZ, 0,1,0) = 3;
    //    F(EZ, 1,1,0) = 3;

    cmflds_->copy_to_device(0, flds, 0, N_FIELDS);
    cmflds_->dump("accel.fld.json");
    flds.dtor();

    
  };

  void init_cmprts()
  {
    RngPool rngpool;
    Rng *rng = rngpool[0];

    uint n_prts_by_patch[1] = { n_prts };
    
    cmprts_ = new cuda_mparticles(grid_, bs_);
    cmprts_->reserve_all(n_prts_by_patch);
    
    std::vector<cuda_mparticles_prt> prts;
    prts.reserve(n_prts);
    
    for (int i = 0; i < n_prts; i++) {
      cuda_mparticles_prt prt = {};
      prt.xi[0] = rng->uniform(0, L);
      prt.xi[1] = rng->uniform(0, L);
      prt.xi[2] = rng->uniform(0, L);
      prt.qni_wni = 1.;

      prts.push_back(prt);
    }
    cmprts_->inject(prts.data(), n_prts_by_patch);

    //cmprts_->dump();
  }

  void run()
  {
    int n_failed = 0;

    for (int n = 0; n < n_steps; n++) {
      printf("advancing step %d\n", n);
      cuda_push_mprts_yz(cmprts_, cmflds_, bs_, IP_EC, DEPOSIT_VB_3D, CURRMEM_GLOBAL);
      cmprts_->get_particles(0,
			     [&] (int i, const cuda_mparticles_prt &prt) {
			       if (std::abs(prt.pxi[0] - 1*(n+1)) > eps ||
				   std::abs(prt.pxi[1] - 2*(n+1)) > eps ||
				   std::abs(prt.pxi[2] - 3*(n+1)) > eps) {
				 printf("FAIL: n %d i %d px %g %g %g // exp %g %g %g\n", n, i,
					prt.pxi[0], prt.pxi[1], prt.pxi[2],
					1.*(n+1), 2.*(n+1), 3.*(n+1));
				 n_failed++;
			       }
			     });
  
      //cmprts_->dump();
    }
    assert(n_failed == 0);
  }

private:
  double L = 1e10;
  unsigned int n_prts = 131;
  int n_steps = 10;
  cuda_mparticles::real_t eps = 1e-5;
  Int3 bs_;
  
  Grid_t grid_;
  cuda_mfields* cmflds_;
  cuda_mparticles* cmprts_;
};

// ----------------------------------------------------------------------
// main

int
main(void)
{
  TestAccel test;
  test.run();
}
