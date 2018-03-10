
#include "psc.h"
#include "psc_particles_as_double.h"
#include "psc_fields_as_c.h"

// ======================================================================
// !!! These moments are shifted to (n+.5) * dt, rather than n * dt,
// since the "double" particles are shifted that way.
//
// node-centered moments -- probably mostly useful for testing
// charge continuity

#include "psc_output_fields_item_moments_2nd_nc.cxx"

// ======================================================================
// psc_output_fields_item: subclass "n_2nd_nc_double"

FieldsItemMomentOps<Moment_n, mparticles_t> psc_output_fields_item_n_2nd_nc_double_ops;
FieldsItemMomentOps<Moment_rho, mparticles_t> psc_output_fields_item_rho_2nd_nc_double_ops;

