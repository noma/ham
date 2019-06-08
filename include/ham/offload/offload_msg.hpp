// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_offload_offload_msg_hpp
#define ham_offload_offload_msg_hpp

// for the copy msg we want to store the remote memory address as MPI_Aint
#if defined(HAM_COMM_MPI_RMA_DYNAMIC) || defined(HAM_COMM_MPI_RMA_DYNAMIC_DATA_ONLY)
#include <mpi.h>
#endif

#include "ham/msg/active_msg.hpp"
#include "ham/msg/execution_policy.hpp"
#include "ham/misc/constants.hpp"
#include "ham/misc/types.hpp"
#include "ham/net/communicator.hpp"
#include "ham/util/debug.hpp"
#include "ham/util/log.hpp"

namespace ham {
namespace offload {
namespace detail {

using ::ham::detail::result_container;
using ::ham::msg::default_execution_policy;
using ::ham::msg::active_msg;
using ::ham::net::communicator;
using ::ham::net::buffer_ptr;

template<typename Functor, typename Result>
struct helper {
	static result_container<Result> execute(Functor& func) {
		return result_container<Result>( func.operator()() ); // execute functor
	}
};

template<typename Functor>
struct helper<Functor, void> {
	static result_container<void> execute(Functor& func) {
		func.operator()(); // execute functor
		return result_container<void>();
	}
};

// executes the functor, and send back its result
// used for all offloads, remote allocation
template<class Functor, template<class> class ExecutionPolicy = default_execution_policy>
class offload_result_msg
	: public active_msg<offload_result_msg<Functor, ExecutionPolicy>, ExecutionPolicy>
	, public Functor
{
public:
	typedef typename Functor::result_type Result;

	offload_result_msg(Functor& f, const communicator::request& req)
	 : Functor(std::forward<Functor>(f)), req(req) { }

	offload_result_msg(Functor&& f, const communicator::request& req)
	 : Functor(std::forward<Functor>(f)), req(req) { }

	void operator()() //const
	{
		result_container<Result> result = helper<Functor, Result>::execute(*static_cast<Functor*>(this)); // this helper stuff is needed to handle void without too much code redundancy
//		HAM_DEBUG( HAM_LOG << "offload_result_msg::operator()(): sending result via req(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl; )
		req.send_result((void*)&result, sizeof result);
	}
private:
	communicator::request req; // TODO(improvement, high priority): use a subset of req here!
};

// just execute the functor
// fire & forget, not used by current HAM-Offload API
template<class Functor, template<class> class ExecutionPolicy = default_execution_policy>
class offload_msg
	: public active_msg<offload_msg<Functor, ExecutionPolicy>, ExecutionPolicy>
	, public Functor
{
public:
	offload_msg(Functor&& f)
	 : Functor(std::forward<Functor>(f)) { }

	void operator()() //const
	{
		Functor::operator()(); // execute functor
	}
};

// data transfer message type, triggers RECEIVING data at the target
// not used by MPI_RMA_COMMUNICATOR since one-sided put is used
template<typename T, template<class> class ExecutionPolicy = default_execution_policy>
class offload_write_msg
	: public active_msg<offload_write_msg<T, ExecutionPolicy>, ExecutionPolicy>
{
public:
	offload_write_msg(communicator::request req, node_t remote_node, T* local_dest, size_t n)
	 : req(req), remote_node(remote_node), local_dest(local_dest), n(n) { }

	void operator()() //const
	{
		communicator::instance().recv_data(buffer_ptr<T>(nullptr, remote_node), local_dest, n); // NOTE: Why nullptr? This is for two-sided communicators, so we do not know the remote address, but match a send operation that has the address.

		// send a result to tell the sender, that the transfer is done
		if (req.valid()) {
			req.send_result((void*)&n, sizeof n);
		}
	}
private:
	communicator::request req; // TODO(improvement, high priority): use a subset of req here!

	node_t remote_node;
	T* local_dest;
	size_t n;
	
};

// data transfer message type, triggers SENDING data at the target
// not used by MPI_RMA_COMMUNICATOR since one-sided put is used
template<typename T, template<class> class ExecutionPolicy = default_execution_policy>
class offload_read_msg
	: public active_msg<offload_read_msg<T, ExecutionPolicy>, ExecutionPolicy>
{
public:
	offload_read_msg(communicator::request req, node_t remote_node, T* local_source, size_t n)
	 : req(req), remote_node(remote_node), local_source(local_source), n(n) { }

	void operator()() //const
	{
		communicator::instance().send_data(local_source, buffer_ptr<T>(nullptr, remote_node), n); // NOTE: Why nullptr? This is for two-sided communicators, so we do not know the remote address, but match a receive operation that has the address.
		
		// send a result message to tell the sender, that the transfer is done
		// TODO(improvement, potential speedup): this may be removed along with receiving the result in offload get(). For host-target transfer completion of receive is sufficient, for copy the destination informs the host of completion
		if (req.valid()) {
			req.send_result((void*)&n, sizeof n);
		}
	}
private:
	communicator::request req; // TODO(improvement, high priority): use a subset of req here!

	node_t remote_node;
	T* local_source;
	size_t n;
};

#if defined(HAM_COMM_MPI_RMA_DYNAMIC) || defined(HAM_COMM_MPI_RMA_DYNAMIC_DATA_ONLY)
	// data transfer message, triggers RMA data transfer to copy target
	// used only with MPI_RMA communicator
	// necessary because of the target buffer's address (remote_addr)
	template<typename T, template<class> class ExecutionPolicy = default_execution_policy>
	class offload_rma_copy_msg
			: public active_msg<offload_rma_copy_msg<T, ExecutionPolicy>, ExecutionPolicy>
	{
	public:
		offload_rma_copy_msg(communicator::request req, node_t remote_node, MPI_Aint remote_addr, T* local_source, size_t n)
				: req(req), remote_node(remote_node), remote_addr(remote_addr), local_source(local_source), n(n) { }

		void operator()() //const
		{
			// MPI_RMA_COMMUNICATOR-only variant of send_data(), because of buffer address (remote_addr)
			communicator::instance().send_data(local_source, buffer_ptr<T>(nullptr, remote_node, remote_addr), n);

			// send a result message to tell the sender, that the transfer is done
			if (req.valid()) {
				req.send_result((void*)&n, sizeof n);
			}
		}
	private:
		communicator::request req; // TODO(improvement, high priority): use a subset of req here!

		node_t remote_node;
		MPI_Aint remote_addr; // this is why we imported mpi.h
		T* local_source;
		size_t n;
	};
#endif

} // namespace detail
} // namespace offload
} // namespace ham

#endif // ham_offload_offload_msg_hpp
