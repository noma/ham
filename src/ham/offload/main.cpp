// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload/main.hpp"

#include "ham/offload/offload.hpp" // must be first
#include "ham/misc/options.hpp"
#include "ham/msg/msg_handler_registry.hpp"
#include "ham/util/cpu_affinity.hpp"
#include "ham/util/debug.hpp"
#include "ham/util/log.hpp"

namespace ham {
namespace offload {

int ham_main(int argc, char* argv[])
{
	// the actual programme starts here
	// assumption: there is one process per shared-memory-domain (or even numa-domain?)

	// init active message layer translation data
	ham::msg::msg_handler_registry::init();
	HAM_DEBUG( ham::msg::msg_handler_registry::print_handler_map(std::cout); )
	HAM_DEBUG( ham::msg::msg_handler_registry::print_handler_vector(std::cout); )

	// init runtime/comm
	HAM_DEBUG( std::cout << "HAM: initisalising runtime" << std::endl; )
	ham::offload::runtime offload_runtime(argc, argv);
	HAM_DEBUG( std::cout << "HAM: initisalising runtime done" << std::endl; )

	int result = 0; // for rank 0, this value is overwritten by UserMainFunctor::operator()()

	if (offload_runtime.is_host())
	{
		result = offload_runtime.run_main(argc, argv);
	}
	else
	{
		result = offload_runtime.run_receive();
	}

	HAM_DEBUG( HAM_LOG << "HAM: end of ham_main" << std::endl; )

	// implicit clean up by dtors

	return result;
}

} // namespace offload
} // namespace ham
