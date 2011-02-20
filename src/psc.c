
#include "psc.h"
#include <mrc_common.h>
#include <mrc_params.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <assert.h>

struct psc psc;

// ----------------------------------------------------------------------

static struct psc_ops *psc_ops_list[] = {
  &psc_ops_generic_c,
  &psc_ops_fortran,
  &psc_ops_none,
#ifdef USE_CUDA
  &psc_ops_cuda,
#endif
#ifdef USE_SSE2
  &psc_ops_sse2,
#endif
  NULL,
};

static struct psc_push_field_ops *psc_push_field_ops_list[] = {
  &psc_push_field_ops_fortran,
  &psc_push_field_ops_c,
  &psc_push_field_ops_none,
  NULL,
};

static struct psc_randomize_ops *psc_randomize_ops_list[] = {
  &psc_randomize_ops_fortran,
  &psc_randomize_ops_none,
  NULL,
};

static struct psc_sort_ops *psc_sort_ops_list[] = {
  &psc_sort_ops_fortran,
  &psc_sort_ops_qsort,
  &psc_sort_ops_countsort,
  &psc_sort_ops_countsort2,
  &psc_sort_ops_none,
  NULL,
};

static struct psc_collision_ops *psc_collision_ops_list[] = {
  &psc_collision_ops_fortran,
  &psc_collision_ops_none,
  NULL,
};

static struct psc_output_ops *psc_output_ops_list[] = {
  &psc_output_ops_fortran,
  &psc_output_ops_c,
  NULL,
};

static struct psc_bnd_ops *psc_bnd_ops_list[] = {
  &psc_bnd_ops_fortran,
  &psc_bnd_ops_c,
  NULL,
};

static struct psc_moment_ops *psc_moment_ops_list[] = {
  &psc_moment_ops_fortran,
  &psc_moment_ops_c,
  NULL,
};

static struct psc_ops *
psc_find_ops(const char *ops_name)
{
  for (int i = 0; psc_ops_list[i]; i++) {
    if (strcasecmp(psc_ops_list[i]->name, ops_name) == 0)
      return psc_ops_list[i];
  }
  fprintf(stderr, "ERROR: psc_ops '%s' not available.\n", ops_name);
  abort();
}

static struct psc_push_field_ops *
psc_find_push_field_ops(const char *ops_name)
{
  for (int i = 0; psc_push_field_ops_list[i]; i++) {
    if (strcasecmp(psc_push_field_ops_list[i]->name, ops_name) == 0)
      return psc_push_field_ops_list[i];
  }
  fprintf(stderr, "ERROR: psc_push_field_ops '%s' not available.\n", ops_name);
  abort();
}

static struct psc_randomize_ops *
psc_find_randomize_ops(const char *ops_name)
{
  for (int i = 0; psc_randomize_ops_list[i]; i++) {
    if (strcasecmp(psc_randomize_ops_list[i]->name, ops_name) == 0)
      return psc_randomize_ops_list[i];
  }
  fprintf(stderr, "ERROR: psc_randomize_ops '%s' not available.\n", ops_name);
  abort();
}

static struct psc_sort_ops *
psc_find_sort_ops(const char *ops_name)
{
  for (int i = 0; psc_sort_ops_list[i]; i++) {
    if (strcasecmp(psc_sort_ops_list[i]->name, ops_name) == 0)
      return psc_sort_ops_list[i];
  }
  fprintf(stderr, "ERROR: psc_sort_ops '%s' not available.\n", ops_name);
  abort();
}

static struct psc_collision_ops *
psc_find_collision_ops(const char *ops_name)
{
  for (int i = 0; psc_collision_ops_list[i]; i++) {
    if (strcasecmp(psc_collision_ops_list[i]->name, ops_name) == 0)
      return psc_collision_ops_list[i];
  }
  fprintf(stderr, "ERROR: psc_collision_ops '%s' not available.\n", ops_name);
  abort();
}

static struct psc_output_ops *             // takes module name from command line
psc_find_output_ops(const char *ops_name)
{
  for (int i = 0; psc_output_ops_list[i]; i++) {
    if (strcasecmp(psc_output_ops_list[i]->name, ops_name) == 0)
      return psc_output_ops_list[i];       // module name shows up in psc_output_ops_list
  }
  fprintf(stderr, "ERROR: psc_output_ops '%s' not available.\n", ops_name);
  abort();
}

static struct psc_bnd_ops *
psc_find_bnd_ops(const char *ops_name)
{
  for (int i = 0; psc_bnd_ops_list[i]; i++) {
    if (strcasecmp(psc_bnd_ops_list[i]->name, ops_name) == 0)
      return psc_bnd_ops_list[i];
  }
  fprintf(stderr, "ERROR: psc_bnd_ops '%s' not available.\n", ops_name);
  abort();
}

static struct psc_moment_ops *
psc_find_moment_ops(const char *ops_name)
{
  for (int i = 0; psc_moment_ops_list[i]; i++) {
    if (strcasecmp(psc_moment_ops_list[i]->name, ops_name) == 0)
      return psc_moment_ops_list[i];
  }
  fprintf(stderr, "ERROR: psc_moment_ops '%s' not available.\n", ops_name);
  abort();
}

#define VAR(x) (void *)offsetof(struct psc_mod_config, x)

static struct param psc_mod_config_descr[] = {
  { "mod_particle"    , VAR(mod_particle)       , PARAM_STRING(NULL)  },
  { "mod_field"       , VAR(mod_field)          , PARAM_STRING(NULL)  },
  { "mod_randomize"   , VAR(mod_randomize)      , PARAM_STRING(NULL)  },
  { "mod_sort"        , VAR(mod_sort)           , PARAM_STRING(NULL)  },
  { "mod_collision"   , VAR(mod_collision)      , PARAM_STRING(NULL)  },
  { "mod_output"      , VAR(mod_output)         , PARAM_STRING(NULL)  },
  { "mod_bnd"         , VAR(mod_bnd)            , PARAM_STRING(NULL)  },
  { "mod_moment"      , VAR(mod_moment)         , PARAM_STRING(NULL)  },
  {},
};

#undef VAR

void
psc_create(struct psc_mod_config *conf)
{
  // defaults
  if (!conf->mod_particle)
    conf->mod_particle = "fortran";
  if (!conf->mod_field)
    conf->mod_field = "fortran";
  if (!conf->mod_randomize)
    conf->mod_randomize = "none";
  if (!conf->mod_sort)
    conf->mod_sort = "none";
  if (!conf->mod_collision)
    conf->mod_collision = "none";
  if (!conf->mod_output)
    conf->mod_output = "fortran";
  if (!conf->mod_bnd)
    conf->mod_bnd = "fortran";
  if (!conf->mod_moment)
    conf->mod_moment = "fortran";

  mrc_params_parse_nodefault(conf, psc_mod_config_descr, "PSC", MPI_COMM_WORLD);
  mrc_params_print(conf, psc_mod_config_descr, "PSC", MPI_COMM_WORLD);

  memset(&psc, 0, sizeof(psc));

  psc.ops = psc_find_ops(conf->mod_particle);
  if (psc.ops->create) {
    psc.ops->create();
  }
  psc.push_field_ops = psc_find_push_field_ops(conf->mod_field);
  if (psc.push_field_ops->create) {
    psc.push_field_ops->create();
  }
  psc.randomize_ops = psc_find_randomize_ops(conf->mod_randomize);
  if (psc.randomize_ops->create) {
    psc.randomize_ops->create();
  }
  psc.sort_ops = psc_find_sort_ops(conf->mod_sort);
  if (psc.sort_ops->create) {
    psc.sort_ops->create();
  }
  psc.collision_ops = psc_find_collision_ops(conf->mod_collision);
  if (psc.collision_ops->create) {
    psc.collision_ops->create();
  }
  psc.output_ops = psc_find_output_ops(conf->mod_output);
  if (psc.output_ops->create) {
    psc.output_ops->create();
  }
  psc.bnd_ops = psc_find_bnd_ops(conf->mod_bnd);
  if (psc.bnd_ops->create) {
    psc.bnd_ops->create();
  }
  psc.moment_ops = psc_find_moment_ops(conf->mod_moment);
  if (psc.moment_ops->create) {
    psc.moment_ops->create();
  }

  psc.time_start = MPI_Wtime();
}

void
psc_fields_destroy(struct psc_mfields *flds)
{
  foreach_patch(p) {
    fields_base_free(&flds->f[p]);
  }
  free(flds->f);
}

void
psc_destroy()
{
  if (psc.ops->destroy) {
    psc.ops->destroy();
  }

  psc_fields_destroy(&psc.flds);
  foreach_patch(p) {
    particles_base_free(&psc.particles.p[p]);
  }
  free(psc.particles.p);
}

static void
ascii_dump_field(struct psc_mfields *flds, int m, const char *fname)
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  foreach_patch(p) {
    char *filename = malloc(strlen(fname) + 10);
    sprintf(filename, "%s-p%d-p%d.asc", fname, rank, p);
    mpi_printf(MPI_COMM_WORLD, "ascii_dump_field: '%s'\n", filename);

    fields_base_t *pf = &flds->f[p];
    FILE *file = fopen(filename, "w");
    free(filename);
    foreach_patch(patch) {
      for (int iz = -psc.ibn[2]; iz < psc.patch[patch].ldims[2] + psc.ibn[2]; iz++) {
	for (int iy = -psc.ibn[1]; iy < psc.patch[patch].ldims[1] + psc.ibn[1]; iy++) {
	  for (int ix = -psc.ibn[0]; ix < psc.patch[patch].ldims[0] +  psc.ibn[0]; ix++) {
	    fprintf(file, "%d %d %d %g\n", ix, iy, iz, F3_BASE(pf, m, ix,iy,iz));
	  }
	  fprintf(file, "\n");
	}
	fprintf(file, "\n");
      }
    }
    fclose(file);
  }
}

static void
ascii_dump_particles(const char *fname)
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  foreach_patch(p) {
    particles_base_t *pp = &psc.particles.p[p];
    char *filename = malloc(strlen(fname) + 10);
    sprintf(filename, "%s-p%d-p%d.asc", fname, rank, p);
    mpi_printf(MPI_COMM_WORLD, "ascii_dump_particles: '%s'\n", filename);
    
    FILE *file = fopen(filename, "w");
    fprintf(file, "i\txi\tyi\tzi\tpxi\tpyi\tpzi\tqni\tmni\twni\n");
    for (int i = 0; i < pp->n_part; i++) {
      particle_base_t *p = particles_base_get_one(pp, i);
      fprintf(file, "%d\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\n",
	      i, p->xi, p->yi, p->zi,
	      p->pxi, p->pyi, p->pzi, p->qni, p->mni, p->wni);
    }
    fclose(file);
    free(filename);
  }
}

void
psc_dump_field(struct psc_mfields *flds, int m, const char *fname)
{
  if (psc.output_ops->dump_field) {
    psc.output_ops->dump_field(m, fname);
  } else {
    ascii_dump_field(flds, m, fname);
  }
}

void
psc_dump_particles(const char *fname)
{
  if (psc.output_ops->dump_particles) {
    psc.output_ops->dump_particles(fname);
  } else {
    ascii_dump_particles(fname);
  }
}

// ----------------------------------------------------------------------
// psc_push_part_xz

void
psc_push_part_xz()
{
  assert(psc.ops->push_part_xz);
  psc.ops->push_part_xz();
}

// ----------------------------------------------------------------------
// psc_push_part_xyz

void
psc_push_part_xyz()
{
  assert(psc.ops->push_part_xyz);
  psc.ops->push_part_xyz();
}

// ----------------------------------------------------------------------
// psc_push_part_xy

void
psc_push_part_xy()
{
  assert(psc.ops->push_part_xy);
  psc.ops->push_part_xy();
}

// ----------------------------------------------------------------------
// psc_push_part_yz

void
psc_push_part_yz()
{
  assert(psc.ops->push_part_yz);
  psc.ops->push_part_yz();
}

// ----------------------------------------------------------------------
// psc_push_part_z

void
psc_push_part_z()
{
  assert(psc.ops->push_part_z);
  psc.ops->push_part_z();
}

// ----------------------------------------------------------------------
// psc_push_part_yz_a

void
psc_push_part_yz_a()
{
  assert(psc.ops->push_part_yz_a);
  psc.ops->push_part_yz_a();
}

// ----------------------------------------------------------------------
// psc_push_part_yz_b

void
psc_push_part_yz_b()
{
  assert(psc.ops->push_part_yz_b);
  psc.ops->push_part_yz_b();
}

// ----------------------------------------------------------------------
// psc_push_particles

void
psc_push_particles()
{
  int *im = psc.domain.gdims;
  if (im[0] > 1 && im[1] > 1 && im[2] > 1) { // xyz
    psc_push_part_xyz();
  } else if (im[0] > 1 && im[2] > 1) { // xz
    psc_push_part_xz();
  } else if (im[0] > 1 && im[1] > 1) { // xy
    psc_push_part_xy();
  } else if (im[1] > 1 && im[2] > 1) { // yz
    psc_push_part_yz();
  } else if (im[2] > 1) { // z
    psc_push_part_z();
  } else {
    assert(0);
  }
}

// ----------------------------------------------------------------------
// psc_push_field_a

void
psc_push_field_a(struct psc_mfields *flds)
{
  assert(psc.push_field_ops->push_field_a);
  psc.push_field_ops->push_field_a(flds);
}

// ----------------------------------------------------------------------
// psc_push_field_b

void
psc_push_field_b(struct psc_mfields *flds)
{
  assert(psc.push_field_ops->push_field_b);
  psc.push_field_ops->push_field_b(flds);
}

// ----------------------------------------------------------------------
// psc_add_ghosts

void
psc_add_ghosts(struct psc_mfields *flds, int mb, int me)
{
  assert(psc.nr_patches == 1);
  assert(psc.bnd_ops->add_ghosts);
  psc.bnd_ops->add_ghosts(flds->f, mb, me);
}

// ----------------------------------------------------------------------
// psc_fill_ghosts

void
psc_fill_ghosts(struct psc_mfields *flds, int mb, int me)
{
  assert(psc.nr_patches == 1);
  assert(psc.bnd_ops->fill_ghosts);
  psc.bnd_ops->fill_ghosts(flds->f, mb, me);
}

// ----------------------------------------------------------------------
// psc_exchange_particles

void
psc_exchange_particles(void)
{
  assert(psc.bnd_ops->exchange_particles);
  psc.bnd_ops->exchange_particles();
}

// ----------------------------------------------------------------------
// psc_randomize

void
psc_randomize()
{
  assert(psc.randomize_ops->randomize);
  psc.randomize_ops->randomize();
}

// ----------------------------------------------------------------------
// psc_sort

void
psc_sort()
{
  assert(psc.sort_ops->sort);
  psc.sort_ops->sort();
}

// ----------------------------------------------------------------------
// psc_collision

void
psc_collision()
{
  assert(psc.collision_ops->collision);
  psc.collision_ops->collision();
}

// ----------------------------------------------------------------------
// psc_out_field

void
psc_out_field(struct psc_mfields *flds)
{
  assert(psc.output_ops->out_field);
  psc.output_ops->out_field(flds);
}

// ----------------------------------------------------------------------
// psc_out_particles

void
psc_out_particles()
{
  OUT_part();
}

// ----------------------------------------------------------------------
// psc_p_pulse_z1

real
psc_p_pulse_z1(real x, real y, real z, real t)
{
  // FIXME, create a fortran pulse instead of special casing
  if (!psc.pulse_p_z1) { // default to Fortran
    return PSC_p_pulse_z1(x, y, z, t);
  }
  return psc_pulse_field(psc.pulse_p_z1, x, y, z, t);
}

//-----------------------------------------------------------------------
// psc_s_pulse_z1

real
psc_s_pulse_z1(real x, real y, real z, real t)
{
  // FIXME, create a fortran pulse instead of special casing
  if (!psc.pulse_s_z1) { // default to Fortran
    return PSC_s_pulse_z1(x, y, z, t);
  }
  return psc_pulse_field(psc.pulse_s_z1, x, y, z, t);
}

// ----------------------------------------------------------------------
// psc_p_pulse_z2

real
psc_p_pulse_z2(real x, real y, real z, real t)
{
  // FIXME, create a fortran pulse instead of special casing
  if (!psc.pulse_p_z2) { // default to Fortran
    return PSC_p_pulse_z2(x, y, z, t);
  }
  return psc_pulse_field(psc.pulse_p_z2, x, y, z, t);
}

//-----------------------------------------------------------------------
// psc_s_pulse_z2

real
psc_s_pulse_z2(real x, real y, real z, real t)
{
  // FIXME, create a fortran pulse instead of special casing
  if (!psc.pulse_s_z2) { // default to Fortran
    return PSC_s_pulse_z2(x, y, z, t);
  }
  return psc_pulse_field(psc.pulse_s_z2, x, y, z, t);
}



// ----------------------------------------------------------------------
// psc moments

void
psc_calc_densities(int p, fields_base_t *pf_base, fields_base_t *pf)
{
  assert(psc.moment_ops->calc_densities);
  psc.moment_ops->calc_densities(p, pf_base, pf);
}

void
psc_calc_moments_v(int p, fields_base_t *pf_base, fields_base_t *pf)
{
  assert(psc.moment_ops->calc_v);
  psc.moment_ops->calc_v(p, pf_base, pf);
}

void
psc_calc_moments_vv(int p, fields_base_t *pf_base, fields_base_t *pf)
{
  assert(psc.moment_ops->calc_vv);
  psc.moment_ops->calc_vv(p, pf_base, pf);
}

// ----------------------------------------------------------------------
// psc_init

#define INIT_param_fortran_F77 F77_FUNC_(init_param_fortran, INIT_PARAM_FORTRAN)

void INIT_param_fortran_F77(void);

void
psc_init(const char *case_name)
{
  psc_init_param(case_name);

  SET_param_domain();
  SET_param_psc();
  SET_param_coeff();
  INIT_basic();
  INIT_param_fortran_F77();

  int particle_label_offset;
  psc_init_partition(&particle_label_offset);
  SET_subdomain();

  psc_init_particles(particle_label_offset);

  psc_mfields_alloc(&psc.flds, NR_FIELDS);
  psc_init_field(&psc.flds);
}

// ----------------------------------------------------------------------
// psc_set_n_particles

void
psc_set_n_particles(int p, int n_part)
{
  psc.particles.p[p].n_part = n_part;
  SET_niloc(n_part);
}

// ----------------------------------------------------------------------
// psc_write_checkpoint

void
psc_read_checkpoint(void)
{
  mpi_printf(MPI_COMM_WORLD, "INFO: Reading checkpoint.\n");
  
  int n_part;
  SERV_read_1(&psc.timestep, &n_part);
  particles_base_realloc(&psc.particles.p[0], n_part);
  psc_set_n_particles(0, n_part);
  
  particles_fortran_t pp;
  particles_fortran_get(&pp);
  fields_fortran_t pf;
  fields_fortran_get(&pf, 0, 0);

  SERV_read_2(&pp, &pf);

  particles_fortran_put(&pp);
  fields_fortran_put(&pf, JXI, HZ + 1);
}

// ----------------------------------------------------------------------
// psc_write_checkpoint

void
psc_write_checkpoint(void)
{
  mpi_printf(MPI_COMM_WORLD, "INFO: Writing checkpoint.\n");
  
  particles_fortran_t pp;
  particles_fortran_get(&pp);
  fields_fortran_t pf;
  fields_fortran_get(&pf, JXI, HZ + 1);

  SERV_write(&pp, &pf);

  particles_fortran_put(&pp);
  fields_fortran_put(&pf, 0, 0);
}

// ======================================================================

void
psc_mfields_alloc(struct psc_mfields *flds, int nr_fields)
{
  flds->f = calloc(psc.nr_patches, sizeof(*flds->f));
  foreach_patch(p) {
    int ilg[3] = { -psc.ibn[0], -psc.ibn[1], -psc.ibn[2] };
    int ihg[3] = { psc.patch[p].ldims[0] + psc.ibn[0],
		   psc.patch[p].ldims[1] + psc.ibn[1],
		   psc.patch[p].ldims[2] + psc.ibn[2] };
    fields_base_alloc(&flds->f[p], ilg, ihg, nr_fields);
  }
}

// ======================================================================

const char *fldname[NR_FIELDS] = {
  [NE]  = "ne",
  [NI]  = "ni",
  [NN]  = "nn",
  [JXI] = "jx",
  [JYI] = "jy",
  [JZI] = "jz",
  [EX]  = "ex",
  [EY]  = "ey",
  [EZ]  = "ez",
  [HX]  = "hx",
  [HY]  = "hy",
  [HZ]  = "hz",
  [DX]  = "dx",
  [DY]  = "dy",
  [DZ]  = "dz",
  [BX]  = "bx",
  [BY]  = "by",
  [BZ]  = "bz",
  [EPS] = "eps",
  [MU]  = "mu",
};
