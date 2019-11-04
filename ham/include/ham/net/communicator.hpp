// Copyright (c) 2013-2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_net_communicator_hpp
#define ham_net_communicator_hpp

#ifdef HAM_COMM_MPI
#include <mpi.h>
#endif

#include "ham/misc/constants.hpp"
#include "ham/misc/types.hpp"

// generic communicator stuff
namespace ham {
namespace net {

	struct
	//alignas(CACHE_LINE_SIZE)
	cache_line_buffer
	{
		char data[constants::CACHE_LINE_SIZE];
	};

	struct
	//alignas(PAGE_SIZE)
	page_buffer
	{
		char data[constants::PAGE_SIZE];
	};

	struct
	//alignas(PAGE_SIZE)
	msg_buffer
	{
		char data[constants::MSG_SIZE];
	};
	
	node_t this_node();
	bool initialised();
} // namespace net
} // namespace ham

// NOTE: include new communication backends here, define HAM_COMM_ONE_SIDED accordingly
#ifdef HAM_COMM_MPI
	#include "ham/net/communicator_mpi.hpp"
#elif defined HAM_COMM_SCIF
	#define HAM_COMM_ONE_SIDED
	#include "ham/net/communicator_scif.hpp"
#elif defined HAM_COMM_VEO
	#define HAM_COMM_ONE_SIDED
	#if (HAM_COMM_VEO == 0) // vector host
		#define HAM_COMM_VH
		#include "ham/net/communicator_veo_only_vh.hpp"
	#elif (HAM_COMM_VEO == 1) // vector engine
		#define HAM_COMM_VE
		#include "ham/net/communicator_veo_only_ve.hpp"
	#else
		static_assert(false, "HAM_COMM_VEO must be set to 0 (vector host build) or 1 (vector engine build).");
	#endif
#elif defined HAM_COMM_VEDMA
	#define HAM_COMM_ONE_SIDED
	#if (HAM_COMM_VEDMA == 0) // vector host
		#define HAM_COMM_VH
		#include "ham/net/communicator_veo_vedma_vh.hpp"
	#elif (HAM_COMM_VEDMA == 1) // vector engine
		#define HAM_COMM_VE
		#include "ham/net/communicator_veo_vedma_ve.hpp"
	#else
		static_assert(false, "HAM_COMM_VEDMA must be set to 0 (vector host build) or 1 (vector engine build).");
	#endif
#else	
	static_assert(false, "Please define either HAM_COMM_MPI, or HAM_COMM_SCIF.");
#endif

#endif // ham_net_communicator_hpp
