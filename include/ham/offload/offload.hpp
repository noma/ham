// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_offload_offload_hpp
#define ham_offload_offload_hpp

#include "ham/net/communicator.hpp" // must be first for Intel MPI

#include <atomic>
#include <cassert>
#include <functional>

#include "ham/functor/buffer.hpp"
#include "ham/misc/types.hpp"
#include "ham/offload/offload_msg.hpp"
#include "ham/offload/runtime.hpp"
#include "ham/util/at_end_of_scope_do.hpp"
#include "ham/util/debug.hpp"
#include "ham/util/log.hpp"

namespace ham {
namespace offload {

// NOTE: put everything that is relevant to the external offload API into the ham::offload namespace
using ::ham::net::buffer_ptr;
using ::ham::net::node_descriptor;
using ::ham::node_t;

node_t this_node();
size_t num_nodes();
bool is_host();
bool is_host(node_t node);
const node_descriptor& get_node_description(node_t node);

template<typename T>
class future
{
public:
	future() : valid_(false) {}
	
	// create a dummy future, request is still invalid
	future(bool valid) : valid_(valid)
	{
		
	}

	future(net::communicator::request req) // ownership of req ist transferred
	 : req(req), valid_(true)
	{}

	// move-only object
	future(const future& other) = delete;
	
	future(future&& other)
	 : req(std::move(other.req)), valid_(other.valid_) // move state of other
	{
		// invalidate other without deleting anything
		other.valid_ = false;
	}

	future& operator=(const future& other) = delete;
	
	future& operator=(future&& other)
	{
		// if this future is valid, we have to complete protocol before overwriting this with other
		if(valid()) get();

		// move state of other
		req = std::move(other.req);
		valid_ = other.valid_;
		// invalidate other without deleting anything
		other.valid_ = false;
		return *this;
	}

	~future()
	{
		if(valid()) get(); // finish the protocol
	}

	// NOTE: not C++11 future conform
	bool test()
	{
		return req.test();
	}

	T get()
	{
		if (valid()) {
			auto x = util::at_end_of_scope_do(std::bind(&future<T>::invalidate, this)); // call invalidate() after returning the result by value
			HAM_DEBUG( HAM_LOG << "future::get(): returning result." << std::endl; )
			if (req.valid())
				return static_cast<detail::result_container<T>*>(req.get())->get();
			else // dummy behaviour
				return T(); // return default instance
			// the returned expression might evaluate to void for T = void,
			// this only works in a single return statement, and allows to
			// compile this with T=void and thus eleminates the need for a 	
			// specialisation
		} else {
			HAM_DEBUG( HAM_LOG << "future::get(): error: get() called on invalid future (double get?)." << std::endl; )
			assert(false);
			return T(); // avoid compiler warning
		}
	}

	bool valid() const
	{
		return valid_;// && req.valid();
	}

	net::communicator::request_reference_type get_request()
	{
		return req;
	}

private:
	void invalidate()
	{
		valid_ = false;
		if (req.valid())
			net::communicator::instance().free_request(req);
	}

	net::communicator::request req;
	bool valid_ = false;
};

/*
// TODO(improvement, maybe): generalise message transfer used by put, get, copy, sync, async
// internal
template<typename Result, typename Msg>
static future<Result> send_msg_async(net::communicator& comm, Msg& msg)
{
	// allocate a request and construct a future
	future<Result> result(comm.allocate_request(remote_dest.node()));
	comm.send_msg(result.get_request(), static_cast<void*>(&msg), sizeof msg);
	comm.recv_result(result.get_request()); // trigger receiving the result message (async)
	return result; // synchronise
}

//internal
template<typename Result, typename Msg>
static Result send_msg_sync(net::communicator& comm, Msg& msg)
{
	return send_msg_async(comm, msg).get();
}
*/

// asynchronous offload
template<typename Functor>
future<typename std::remove_reference<Functor>::type::result_type> async(node_t node, Functor&& func)
//auto async(node_t node, Functor&& func) -> typename Functor::result_type
{
	typedef typename std::remove_reference<Functor>::type FunctorT;
	typedef typename FunctorT::result_type Result;

	net::communicator& comm = runtime::instance().communicator();

	// allocate a request and construct a future
	future<Result> result(comm.allocate_request(node));

//	{
//		HAM_DEBUG(
//				const net::communicator::request& req = result.get_request();
//				HAM_LOG << "runtime::async(): req(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl; )
//	}

	// generate an offload message
	// TODO(improvement, high priority): create message inplace the communication buffer and avoid a copy
	auto msg = detail::offload_result_msg<FunctorT>(std::forward<Functor>(func), result.get_request());
	HAM_DEBUG( HAM_LOG << "runtime::async(): sending msg..." << std::endl; )
	comm.send_msg(result.get_request(), (void*)&msg, sizeof msg);
	comm.recv_result(result.get_request()); // trigger receiving the result

	return result;
}

template<typename Functor>
typename std::remove_reference<Functor>::type::result_type sync(node_t node, Functor&& func)
//auto sync(node_t node, Functor&& func) -> typename Functor::result_type
{
	// NOTE: async + get on the resulting future
	return async(node, func).get(); // TODO(investigate): std::forward(func) => compile time error ?!
}

// synchronously send fire & forget
template<typename Functor>
void ping(node_t node, Functor&& func)
{
	typedef typename std::remove_reference<Functor>::type FunctorT;
	net::communicator& comm = runtime::instance().communicator();
	
	auto msg = detail::offload_msg<FunctorT, msg::execution_policy_direct>(std::forward<Functor>(func));
	HAM_DEBUG( HAM_LOG << "runtime::ping(): sending msg..." << std::endl; )
	net::communicator::request req = comm.allocate_request(node); // TODO(improvement): resource deallocation of this request (currently only used for terminating)
	comm.send_msg(req, (void*)&msg, sizeof msg);
	HAM_DEBUG( HAM_LOG << "runtime::ping(): sending msg done." << std::endl; )
}


template<typename T>
buffer_ptr<T> allocate(const node_t node, size_t n)
{
	// use existing functor + sync
	return sync(node, new_buffer<T>(n, this_node()));
}

template<typename T>
void free(buffer_ptr<T> remote_data)
{
	sync(remote_data.node(), delete_buffer<T>(remote_data));
	// use existing functor + sync
}

template<typename T>
future<void> put(T* local_source, buffer_ptr<T>& remote_dest, size_t n)
{
	net::communicator& comm = runtime::instance().communicator();
#ifdef HAM_COMM_ONE_SIDED
	// TODO(improvement): create a data transfer thread for one-sided
	comm.send_data(local_source, remote_dest, n); // sync
	return future<void>(true); // return dummy future
#else
	// allocate a request and construct a future
	future<void> result(comm.allocate_request(remote_dest.node()));
	// generate an offload message
	auto msg = detail::offload_write_msg<T>(result.get_request(), this_node(), remote_dest.get(), n);
	HAM_DEBUG( HAM_LOG << "runtime::write(): sending write msg..." << std::endl; )
	comm.send_msg(result.get_request(), (void*)&msg, sizeof msg); // async
	comm.send_data_async(result.get_request(), local_source, remote_dest, n); // async
	comm.recv_result(result.get_request()); // trigger receiving the msgs result // async
	
	return result;
#endif
}

template<typename T>
void put_sync(T* local_source, buffer_ptr<T>& remote_dest, size_t n)
{
#ifdef HAM_COMM_ONE_SIDED
	net::communicator& comm = runtime::instance().communicator();
	comm.send_data(local_source, remote_dest, n); // sync
#else
	put(local_source, remote_dest, n).get();
#endif
}

template<typename T>
future<void> get(buffer_ptr<T> remote_source, T* local_dest, size_t n)
{
	net::communicator& comm = runtime::instance().communicator();
#ifdef HAM_COMM_ONE_SIDED
	// TODO(improvement): create a data transfer thread for one-sided
	comm.recv_data(remote_source, local_dest, n); // sync
	return future<void>(true); // return dummy future
#else
	// allocate a request and construct a future
	future<void> result(comm.allocate_request(remote_source.node()));
	// generate an offload message
	auto msg = detail::offload_read_msg<T>(result.get_request(), this_node(), remote_source.get(), n);
	HAM_DEBUG( HAM_LOG << "runtime::read(): sending read msg..." << std::endl; )
	comm.send_msg(result.get_request(), (void*)&msg, sizeof msg);
	comm.recv_data_async(result.get_request(), remote_source, local_dest, n);
	comm.recv_result(result.get_request()); // trigger receiving the result

	return result;
#endif
}

template<typename T>
void get_sync(buffer_ptr<T> remote_source, T* local_dest, size_t n)
{
#ifdef HAM_COMM_ONE_SIDED
	net::communicator& comm = runtime::instance().communicator();
	comm.recv_data(remote_source, local_dest, n);
#else
	get(remote_source, local_dest, n).get();
#endif
}

// TODO(improvement):
// generalise copy for the local cases and simplifiy put/get
// * -> * = copy
// remote -> remote = copy
// local -> remote = put
// remote -> local = get
// local -> local = (just make a memcpy)

// TODO(feature, high priority): copy_async 
//	- problems: combine two futures into a new one: recursive list template
//     sync on read_result and write_result,
//   
//template<typename T>
//future<void> copy(buffer_ptr<T> source, buffer_ptr<T> dest, size_t n)
//{

//}

#ifndef HAM_COMM_ONE_SIDED // TODO(feature, high priority): implement
template<typename T>
void copy_sync(buffer_ptr<T> source, buffer_ptr<T> dest, size_t n)
{
	net::communicator& comm = runtime::instance().communicator();
#ifdef HAM_COMM_ONE_SIDED
// TODO(feature, high priority): implement
// fix 1st arg:
//	comm.send_data(src_node, local_source, remote_dest, n);
//	static_assert(false, "copy is not implemented yet for the SCIF back-end");
#else
	// send corresponding write and read messages to the sender and the receiver

	// issues a send operation on the source node, that sends the memory at source to the destination node
	future<void> read_result(comm.allocate_request(source.node()));
	auto read_msg = detail::offload_read_msg<T>(read_result.get_request(), dest.node(), source.get(), n); 
	comm.send_msg(read_result.get_request(), (void*)&read_msg, sizeof read_msg);
	comm.recv_result(read_result.get_request()); // trigger receiving the result

	// issues a receive operation on the destination node, that receives from source.node()
	future<void> write_result(comm.allocate_request(dest.node()));
	auto write_msg = detail::offload_write_msg<T>(write_result.get_request(), source.node(), dest.get(), n);
	comm.send_msg(write_result.get_request(), (void*)&write_msg, sizeof write_msg); // async
	comm.recv_result(write_result.get_request()); // trigger receiving the msg result // async
	
	// synchronise
	read_result.get();
	write_result.get();
#endif
}
#endif

// TODO(feature): new API elements
// construct/destruct remote objects => object_ptr
// array_ptr in addition to buffer_ptr (ctor, dtor calls, size check, ...)

} // namespace offload
} // namespace ham

#endif // ham_offload_offload_hpp

