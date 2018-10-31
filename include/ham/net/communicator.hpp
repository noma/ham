// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
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

	struct
	msg_flag_buffer
	{
		// buffer for fully rma backend. windows will consist of several of those
		char msg[constants::MSG_SIZE];
		flag_t flag;
	};
	
	node_t this_node();
}
}

// NOTE: include new communication backends here, define HAM_COMM_ONE_SIDED accordingly
#ifdef HAM_COMM_MPI
#include "ham/net/communicator_mpi.hpp"
#elif defined HAM_COMM_SCIF
#define HAM_COMM_ONE_SIDED
#include "ham/net/communicator_scif.hpp"
#elif defined HAM_COMM_MPI_RMA_DYNAMIC
#include "ham/net/communicator_mpi_rma_dynamic.hpp"
#elif defined HAM_COMM_TCP
#include "ham/net/communicator_tcp.hpp"
#else
static_assert(false, "Please define either HAM_COMM_MPI, HAM_COMM_MPI_RMA_DYNAMIC or HAM_COMM_SCIF.");
#endif

#endif // ham_net_communicator_hpp
