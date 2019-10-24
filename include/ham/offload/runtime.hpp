// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_offload_runtime_hpp
#define ham_offload_runtime_hpp

#include "ham/net/communicator.hpp" // must be included first for Intel MPI

#include <atomic>

#include "ham/misc/types.hpp"
#include "ham/msg/active_msg.hpp"
#include "ham/util/debug.hpp"
#include "ham/util/log.hpp"

// this is the main provided by main.cpp or main_explicit.cpp
extern int ham_user_main(int argc, char* argv[]);

namespace ham {
namespace offload {

class runtime {
public:

	class terminate_functor : public msg::active_msg<terminate_functor, msg::execution_policy_direct>
	{
	public:
		void operator()()
		{
			runtime::instance().abort();
			HAM_DEBUG( HAM_LOG << "Terminate Functor finished" << std::endl; )
		}
	};

	runtime(int* argc_ptr, char** argv_ptr[]);
	~runtime();

	int run_main(int* argc_ptr, char** argv_ptr[]); // not needed if HAM_EXPLICIT is defined

	void terminate_workers();
	int run_receive();

	bool abort() { return abort_flag.exchange(true); }

	static runtime& instance() { return *instance_; }

	net::communicator& communicator() { return comm; } 

	node_t this_node() { return comm.this_node(); }
	int num_nodes() { return comm.num_nodes(); }
	bool is_host() { return comm.is_host(); }

private:
	static runtime* instance_;
	std::atomic_bool abort_flag;
	net::communicator comm;
};

} // namespace offload
} // namespace ham

#endif // ham_offload_runtime_hpp
