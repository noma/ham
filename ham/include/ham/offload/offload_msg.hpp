// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_offload_offload_msg_hpp
#define ham_offload_offload_msg_hpp

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
		return {}; // NOTE: return default initialised return type
	}
};

// executes the functor, and send back its result
template<class Functor, template<class> class ExecutionPolicy = default_execution_policy>
class offload_result_msg
	: public active_msg<offload_result_msg<Functor, ExecutionPolicy>, ExecutionPolicy>
	, public Functor
{
public:
	using Result = typename Functor::result_type;

	offload_result_msg(Functor& f, const communicator::request& req)
	 : Functor(std::forward<Functor>(f)), req(req) { }

	offload_result_msg(Functor&& f, const communicator::request& req)
	 : Functor(std::forward<Functor>(f)), req(req) { }

	void operator()() //const
	{
		result_container<Result> result = helper<Functor, Result>::execute(*static_cast<Functor*>(this)); // this helper stuff is needed to handle void without too much code redundancy
		HAM_DEBUG( HAM_LOG << "offload_result_msg::operator()(): this = " << (void*)this << " sending result via req(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl; )
		req.send_result((void*)&result, sizeof result);
	}
private:
	communicator::request req; // TODO(improvement, high priority): use a subset of req here!
};

// just execute the functor
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

template<typename T, template<class> class ExecutionPolicy = default_execution_policy>
class offload_read_msg
	: public active_msg<offload_read_msg<T, ExecutionPolicy>, ExecutionPolicy>
{
public:
	offload_read_msg(communicator::request req, node_t remote_node, T* local_source, size_t n)
	 : req(req), remote_node(remote_node), local_source(local_source), n(n) { }

	void operator()() //const
	{
		communicator::instance().send_data(local_source, buffer_ptr<T>(nullptr, remote_node), n);  // NOTE: Why nullptr? This is for two-sided communicators, so we do not know the remote address, but match a receive operation that has the address.
		
		// send a result message to tell the sender, that the transfer is done
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

} // namespace detail
} // namespace offload
} // namespace ham

#endif // ham_offload_offload_msg_hpp
