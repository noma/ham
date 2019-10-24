// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload/runtime.hpp"

#include "ham/misc/options.hpp"
#include "ham/offload/offload.hpp"
#include "ham/util/cpu_affinity.hpp"

namespace ham {
namespace offload {

runtime* runtime::instance_ = nullptr;

runtime::runtime(int* argc_ptr, char** argv_ptr[]) : abort_flag(false), comm(argc_ptr, argv_ptr) // NOTE: communicator ctor, might change argc, argv values, e.g. MPI_Init
{
	HAM_DEBUG( HAM_LOG << "runtime::runtime()" << std::endl; )
	ham::detail::options ham_options(argc_ptr, argv_ptr);
	if (ham_options.cpu_affinity() >= 0)
		ham::util::set_cpu_affinity(ham_options.cpu_affinity());

	instance_ = this;
}

runtime::~runtime()
{
	HAM_DEBUG( HAM_LOG << "runtime::~runtime" << std::endl; )
}

// not needed if HAM_EXPLICIT is defined
int runtime::run_main(int* argc_ptr, char** argv_ptr[])
{
	// execute user main
	HAM_DEBUG( HAM_LOG << "runtime::run_main: executing user_main" << std::endl; )
	int result;
	result = ham_user_main(*argc_ptr, *argv_ptr); // result is a reference to a local variable
	HAM_DEBUG( HAM_LOG << "runtime::run_main: user_main finished" << std::endl; )
	terminate_workers();
	return result;
}

void runtime::terminate_workers()
{
	for (node_t node = 0; node < static_cast<node_t>(num_nodes()); ++node)
	{
		if(node != this_node())
		{
			HAM_DEBUG( HAM_LOG << "runtime::run_main: sending terminate to node: " << node << std::endl; )
			ping(node, terminate_functor());
		}
	}
}

int runtime::run_receive()
{
	// receive and execute active messages
	while (!abort_flag)
	{
		HAM_DEBUG( HAM_LOG << "runtime::run_receive(), waiting for message" << std::endl; )
		void* msg_buffer = comm.recv_msg_host();
		auto functor = *reinterpret_cast<msg::active_msg_base*>(msg_buffer); // buffer to message
		HAM_DEBUG( HAM_LOG << "runtime::run_receive(), executing message: " << (void*)msg_buffer << std::endl; )
		functor(msg_buffer); // call active_msg_base::operator()(msg_buffer) which calls execution_policy::handler()
		HAM_DEBUG( HAM_LOG << "runtime::run_receive(), message execution done." << std::endl; )
	}
	HAM_DEBUG( HAM_LOG << "runtime::run_receive(), returning." << std::endl; )
	return 0;
}

} // namespace offload
} // namespace ham

