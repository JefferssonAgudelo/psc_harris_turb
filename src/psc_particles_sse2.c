
#include "psc.h"
#include "psc_particles_sse2.h"

#include <math.h>
#include <stdlib.h>
#include <assert.h>

#if PARTICLES_BASE == PARTICLES_SSE2

static size_t __arr_size;

void
particles_sse2_alloc(particles_sse2_t *pp, int n_part)
{
  __arr_size = ((size_t)(n_part * 1.2) + VEC_SIZE - 1) & ~(VEC_SIZE - 1);
  pp->particles = calloc(__arr_size, sizeof(*pp->particles));
}

void
particles_sse2_realloc(particles_sse2_t *pp, int new_n_part)
{
  if (__arr_size <= new_n_part)
    return;

  __arr_size = ((size_t)(new_n_part * 1.2) + VEC_SIZE - 1) & ~(VEC_SIZE - 1);
  pp->particles = realloc(pp->particles, __arr_size * sizeof(*pp->particles));
}

void
particles_sse2_free(particles_sse2_t *pp)
{
  free(pp->particles);
  pp->particles = NULL;
}

void
particles_sse2_get(particles_sse2_t *pp)
{
  pp->particles = psc.pp.particles;

  pp->n_part = psc.pp.n_part;
}

void
particles_sse2_put(particles_sse2_t *pp)
{
}

#else

static size_t __sse2_part_allocated;
static particle_sse2_t *__sse2_part_data;

/// Copy particles from base data structures to an SSE2 friendly format.
void
particles_sse2_get(particles_sse2_t *particles)
{
  int n_part = psc.pp.n_part;
  int pad = 0;
  if((n_part % VEC_SIZE) != 0){
    pad = VEC_SIZE - (n_part % VEC_SIZE);
  }
  
  if(n_part > __sse2_part_allocated) {
    free(__sse2_part_data);
    __sse2_part_allocated = n_part * 1.2;
    if(n_part*0.2 < pad){
      __sse2_part_allocated += pad;
    }
    __sse2_part_data = calloc(__sse2_part_allocated, sizeof(*__sse2_part_data));
  }
  particles->particles = __sse2_part_data;
  particles->n_part = n_part;

  int im[3] = {
    psc.domain.ihi[0] - psc.domain.ilo[0],
    psc.domain.ihi[1] - psc.domain.ilo[1],
    psc.domain.ihi[2] - psc.domain.ilo[2],
  };
  
  particle_sse2_real_t dxi = 1. / psc.dx[0];
  particle_sse2_real_t dyi = 1. / psc.dx[1];

  for (int n = 0; n < n_part; n++) {
    particle_base_t *base_part = particles_base_get_one(&psc.pp, n);
    particle_sse2_t *part = &particles->particles[n];

    part->xi  = base_part->xi;
    part->yi  = base_part->yi;
    part->zi  = base_part->zi;
    part->pxi = base_part->pxi;
    part->pyi = base_part->pyi;
    part->pzi = base_part->pzi;
    part->qni = base_part->qni;
    part->mni = base_part->mni;
    part->wni = base_part->wni;
    ///< \FIXME It would be nice to do away with the asserts checking that the ignored
    /// direction can actually be ignored.
    if (im[0] > 1 && im[2] > 1) { // xz
      int j2 = part->yi * dyi + .5;
      assert(j2 == psc.ilo[1]); 
    } else if (im[0] > 1 && im[1] > 1) { // xy
      assert(0);
    } else if (im[1] > 1 && im[2] > 1) { // yz
      int j1 = part->xi * dxi + .5;
      assert(j1 == psc.ilo[0]); 
    }    
  }
  // We need to give the padding a non-zero mass to avoid NaNs
  for(int n = n_part; n < (n_part + pad); n++){
    particle_base_t *base_part = particles_base_get_one(&psc.pp, n_part - 1);
    particle_sse2_t *part = &particles->particles[n];
    part->xi  = base_part->xi; //We need to be sure the padding loads fields inside the local domain
    part->yi  = base_part->yi;
    part->zi  = base_part->zi;
    part->mni = 1.0;
  }
}

/// Copy particles from SSE2 data structures to base structures.
void
particles_sse2_put(particles_sse2_t *particles)
{
   for(int n = 0; n < psc.pp.n_part; n++) {
     particle_base_t *base_part = particles_base_get_one(&psc.pp, n);
     particle_sse2_t *part = &particles->particles[n];
     
     base_part->xi  = part->xi;
     base_part->yi  = part->yi;
     base_part->zi  = part->zi;
     base_part->pxi = part->pxi;
     base_part->pyi = part->pyi;
     base_part->pzi = part->pzi;
     base_part->qni = part->qni;
     base_part->mni = part->mni;
     base_part->wni = part->wni;
   }
}

#endif
