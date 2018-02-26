
#pragma once

#include "dim.hxx"
#include "inc_defs.h"

struct CacheFieldsNone;
struct CacheFields;

template<typename MP, typename MF, typename D,
	 typename IP, typename O,
	 typename CALCJ = opt_calcj_esirkepov,
	 typename OPT_EXT = opt_ext_none,
	 typename CF = CacheFieldsNone>
struct push_p_config
{
  using mparticles_t = MP;
  using mfields_t = MF;
  using dim = D;
  using ip = IP;
  using order = O;
  using calcj = CALCJ;
  using ext = OPT_EXT;
  using CacheFields = CF;
};

#include "psc_particles_double.h"
#include "psc_fields_c.h"

using Config2ndXYZ = push_p_config<PscMparticlesDouble, PscMfieldsC, dim_xyz, opt_ip_2nd, opt_order_2nd>;
using Config2ndXY = push_p_config<PscMparticlesDouble, PscMfieldsC, dim_xy, opt_ip_2nd, opt_order_2nd>;
using Config2ndXZ = push_p_config<PscMparticlesDouble, PscMfieldsC, dim_xz, opt_ip_2nd, opt_order_2nd>;
using Config2ndYZ = push_p_config<PscMparticlesDouble, PscMfieldsC, dim_yz, opt_ip_2nd, opt_order_2nd>;
using Config2ndY = push_p_config<PscMparticlesDouble, PscMfieldsC, dim_y, opt_ip_2nd, opt_order_2nd>;
using Config2ndZ = push_p_config<PscMparticlesDouble, PscMfieldsC, dim_z, opt_ip_2nd, opt_order_2nd>;
using Config2nd1 = push_p_config<PscMparticlesDouble, PscMfieldsC, dim_1, opt_ip_2nd, opt_order_2nd>;

using Config2ndDoubleYZ = push_p_config<PscMparticlesDouble, PscMfieldsC, dim_yz, opt_ip_2nd, opt_order_2nd,
				 opt_calcj_esirkepov, opt_ext_none, CacheFields>;

using Config1stXZ = push_p_config<PscMparticlesDouble, PscMfieldsC, dim_xz, opt_ip_1st, opt_order_1st>;
using Config1stYZ = push_p_config<PscMparticlesDouble, PscMfieldsC, dim_yz, opt_ip_1st, opt_order_1st>;
