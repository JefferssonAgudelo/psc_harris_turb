
#include "psc_push_particles_private.h"

#include "psc_cuda2.h"
#include "psc_particles_cuda2.h"

#include <mrc_profile.h>

#include <string.h>

//#define GOLD

// ======================================================================
// psc_push_particles: subclass "1vb_cuda2"

// ----------------------------------------------------------------------
// psc_push_particles_1vbec_push_mprts_yz

static void
psc_push_particles_1vbec_push_mprts_yz(struct psc_push_particles *push,
				       struct psc_mparticles *mprts_base,	
				       struct psc_mfields *mflds_base)					
{									
  static int pr;
  if (!pr) {
    pr = prof_register("1vbec_yz", 1., 0, 0);
  }  

  struct psc_mparticles *mprts =
    psc_mparticles_get_as(mprts_base, "cuda2", 0);
  struct psc_mfields *mflds =
    psc_mfields_get_as(mflds_base, "cuda2", EX, EX + 6);

#ifdef GOLD
  cuda2_1vbec_push_mprts_gold_yz(mprts, mflds);
#else
  psc_mparticles_cuda2_copy_to_device(mprts);
  psc_mfields_cuda2_copy_to_device(mflds);

  prof_start(pr);
  cuda2_1vbec_push_mprts_yz(mprts, mflds);
  prof_stop(pr);

  psc_mparticles_cuda2_copy_to_host(mprts);
  psc_mfields_cuda2_copy_to_host(mflds);
#endif

  psc_mparticles_put_as(mprts, mprts_base, 0);
  psc_mfields_put_as(mflds, mflds_base, JXI, JXI + 3);	
}

// ----------------------------------------------------------------------
// psc_push_particles_1vbec_push_mprts_xyz

static void
psc_push_particles_1vbec_push_mprts_xyz(struct psc_push_particles *push,
					struct psc_mparticles *mprts_base,	
					struct psc_mfields *mflds_base)					
{									
  static int pr;
  if (!pr) {
    pr = prof_register("1vbec_xyz", 1., 0, 0);
  }  

  struct psc_mparticles *mprts =
    psc_mparticles_get_as(mprts_base, "cuda2", 0);
  struct psc_mfields *mflds =
    psc_mfields_get_as(mflds_base, "cuda2", EX, EX + 6);

#ifdef GOLD
  cuda2_1vbec_push_mprts_gold_xyz(mprts, mflds);
#else
  psc_mparticles_cuda2_copy_to_device(mprts);
  psc_mfields_cuda2_copy_to_device(mflds);

  prof_start(pr);
  cuda2_1vbec_push_mprts_xyz(mprts, mflds);
  prof_stop(pr);

  psc_mparticles_cuda2_copy_to_host(mprts);
  psc_mfields_cuda2_copy_to_host(mflds);
#endif

  psc_mparticles_put_as(mprts, mprts_base, 0);
  psc_mfields_put_as(mflds, mflds_base, JXI, JXI + 3);	
}

// ----------------------------------------------------------------------
// psc_push_particles: subclass "1vbec_cuda2"
									
struct psc_push_particles_ops						
psc_push_particles_1vbec_cuda2_ops = {	
  .name                  = "1vbec_cuda2",
  .push_mprts_yz         = psc_push_particles_1vbec_push_mprts_yz,
  .push_mprts_xyz        = psc_push_particles_1vbec_push_mprts_xyz,
};
