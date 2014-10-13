// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload/main_explicit.hpp"

#include "ham/offload/offload.hpp" // must be first
#include "ham/misc/options.hpp"
#include "ham/msg/msg_handler_registry.hpp"
#include "ham/util/cpu_affinity.hpp"
#include "ham/util/debug.hpp"
#include "ham/util/log.hpp"

namespace ham {
namespace offload {

runtime* offload_runtime = nullptr;

// This must be called explicitly before you start using the HAM-Offload API, usually at the beginning of main().
// NOTE: This call blocks in all processes except the logical host process and 
bool ham_init(int argc, char* argv[])
{
	// assumption: there is one process per shared-memory-domain (or even numa-domain?)

	// init active message layer translation data
	ham::msg::msg_handler_registry::init();
	HAM_DEBUG( ham::msg::msg_handler_registry::print_handler_map(std::cout); )
	HAM_DEBUG( ham::msg::msg_handler_registry::print_handler_vector(std::cout); )

	// init runtime/comm
	HAM_DEBUG( std::cout << "HAM: initisalising runtime" << std::endl; )
	offload_runtime = new ham::offload::runtime(argc, argv);
	HAM_DEBUG( std::cout << "HAM: initisalising runtime done" << std::endl; )

	if (!offload_runtime->is_host())
	{
		offload_runtime->run_receive();
	}

	HAM_DEBUG( HAM_LOG << "HAM: end of ham_init" << std::endl; )
	return offload_runtime->is_host();
}

// This must be called explicitly when you are done using the HAM-Offload API, usually at the end of main().
int ham_finalise()
{
	if (offload_runtime)
	{
		if (offload_runtime->is_host())
		{
			offload_runtime->terminate_workers();
		}

		delete offload_runtime;
		HAM_DEBUG( HAM_LOG << "HAM: finalise done" << std::endl; )
	}
	else
	{
		HAM_DEBUG( HAM_LOG << "HAM: ham_finalise() called prior to ham_init()" << std::endl; )
	}
	return 0;
}

} // namespace offload
} // namespace ham

// just a dummy for satisfying the runtime implementation
int ham_user_main(int argc, char* argv[])
{
	// this should never be called
	assert(false);
	return -1;
}

