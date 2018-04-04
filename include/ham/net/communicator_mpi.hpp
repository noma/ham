// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_net_communicator_mpi_hpp
#define ham_net_communicator_mpi_hpp

#include <mpi.h>

#include <cassert>
#include <cstring> // memcpy
#include <stdlib.h> // posix_memalign

#include "ham/misc/constants.hpp"
#include "ham/misc/resource_pool.hpp"
#include "ham/misc/types.hpp"
#include "ham/util/debug.hpp"
#include "ham/util/log.hpp"

namespace ham {
namespace net {

template<typename T>
class buffer_ptr {
public:
	buffer_ptr();
	buffer_ptr(T* ptr, node_t node) : ptr_(ptr), node_(node) { }

	T* get() { return ptr_; }
	node_t node() { return node_; }
	
	// element access
	T& operator [] (size_t i);

	// basic pointer arithmetic to address sub-buffers
	buffer_ptr<T> operator+(size_t off)
	{
		return buffer_ptr(ptr_ + off, node_);
	}

private:
	T* ptr_;
	node_t node_;
};

class node_descriptor
{
public:
	//node_descriptor() : name(MPI_MAX_PROCESSOR_NAME, 0) {}

	//const std::string& name() const { return name_; }
	const char* name() const { return name_; }
private:
	//std::string name_; // TODO(improvement): unify node description for all back-ends, NOTE: std::string is not trivally transferable
	char name_[MPI_MAX_PROCESSOR_NAME + 1];

	friend class net::communicator;
};

class communicator {
public:
	// externally used interface of request must be shared across all communicator-implementations
	class request {
	public:
		request() : valid_(false) {} // instantiate invalid
		
		request(node_t target_node, node_t source_node, size_t send_buffer_index, size_t recv_buffer_index)
		 : target_node(target_node), source_node(source_node), valid_(true), send_buffer_index(send_buffer_index), recv_buffer_index(recv_buffer_index), req_count(0)
		{}

		// return true if request was finished
		bool test()
		{
			int flag = 0;
			MPI_Testall(req_count, mpi_reqs, &flag, MPI_STATUS_IGNORE); // just test the receive request, since the send belonging to the request triggers the remote send that is received
			return flag != 0;
		}

		void* get() // blocks
		{
			HAM_DEBUG( HAM_LOG << "request::get(), before MPI_Waitall()" << std::endl; )
			MPI_Waitall(req_count, mpi_reqs, MPI_STATUS_IGNORE); // must wait for all requests to satisfy the standard
			HAM_DEBUG( HAM_LOG << "request::get(), after MPI_Waitall()" << std::endl; )
			return static_cast<void*>(&communicator::instance().peers[target_node].msg_buffers[recv_buffer_index]);
		}

		template<class T>
		void send_result(T* result_msg, size_t size)
		{
			assert(communicator::this_node() == target_node); // this assert fails if send_result is called from the wrong side
			
			// TODO(improvement, low priority): better go through communicator, such that no MPI calls are anywhere else
			MPI_Send(result_msg, size, MPI_BYTE, source_node, constants::RESULT_TAG, MPI_COMM_WORLD);
			//communicator::instance().send_msg(source_node, source_buffer_index, NO_BUFFER_INDEX, result_msg, size);
		}

		bool valid() const
		{
			return valid_;
		}
		
		MPI_Request& next_mpi_request()
		{
			HAM_DEBUG( HAM_LOG << "next_mpi_request(): this=" << this << ", req_count=" << req_count << ", NUM_REQUESTS=" << NUM_REQUESTS << std::endl; )
			assert(req_count < NUM_REQUESTS);
			return mpi_reqs[req_count++]; // NOTE: post-increment
		}

		node_t target_node;
		node_t source_node;
		bool valid_;

		// only needed by the sender
		enum { NUM_REQUESTS = 3 };
		
		size_t send_buffer_index; // buffer to use for sending the message
		size_t recv_buffer_index; // buffer to use for receiving the result
		size_t req_count;
		
	private:
		MPI_Request mpi_reqs[NUM_REQUESTS]; // for sending the msg, receiving the result, and an associated data transfer
	}; // class request
	
	typedef request& request_reference_type;
	typedef const request& request_const_reference_type;

	communicator(int argc, char* argv[])
	{
		HAM_DEBUG( std::cout << "communicator::communicator(): initialising MPI" << std::endl; )

		instance_ = this;
		int p;
		MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &p);
		if (p != MPI_THREAD_MULTIPLE)
		{
			std::cerr << "Could not initialise MPI with MPI_THREAD_MULTIPLE, MPI_Init_thread() returned " << p << std::endl;
		}
		HAM_DEBUG( std::cout << "communicator::communicator(): initialising MPI ..." << std::endl; )

		int t;
		MPI_Comm_rank(MPI_COMM_WORLD, &t);
		this_node_ = t;
		MPI_Comm_size(MPI_COMM_WORLD, &t);
		nodes_ = t;
		host_node_ = 0; // TODO(improvement): make configureable, like for SCIF

		HAM_DEBUG( std::cout << "communicator::communicator(): initialising MPI done" << std::endl; )

		peers = new mpi_peer[nodes_];
		
		// start of node descriptor code:
		node_descriptions.resize(nodes_);
		
		// build own node descriptor
		node_descriptor node_description;
		int count;
		MPI_Get_processor_name(node_description.name_, &count);
		node_description.name_[count] = 0x0; // null terminate

//		char hostname[MPI_MAX_PROCESSOR_NAME + 1];
//		MPI_Get_processor_name(hostname, &count);
//		hostname[count] = 0x0; // null terminate
//		node_description.name_.assign(hostname, count);

		// append rank for testing:
		//node_description.name_[count] = 48 + this_node_;
		//node_description.name_[count+1] = 0x0;

		// communicate descriptors between nodes
		HAM_DEBUG( HAM_LOG << "communicator::communicator(): gathering node descriptions" << std::endl; )
		//MPI_Alltoall(&node_description, sizeof(node_descriptor), MPI_BYTE, node_descriptions.data(), sizeof(node_descriptor), MPI_BYTE, MPI_COMM_WORLD);
		MPI_Allgather(&node_description, sizeof(node_descriptor), MPI_BYTE, node_descriptions.data(), sizeof(node_descriptor), MPI_BYTE, MPI_COMM_WORLD);
		HAM_DEBUG( HAM_LOG << "communicator::communicator(): gathering node descriptions done" << std::endl; )
		
		if (is_host()) {
			for (node_t i = 1; i < nodes_; ++i) { // TODO(improvement): needs to be changed when host-rank becomes configurable
				// allocate buffers
				peers[i].msg_buffers = allocate_buffer<msg_buffer>(constants::MSG_BUFFERS, this_node_);
				// fill resource pools
				for(size_t j = constants::MSG_BUFFERS; j > 0; --j) {
					peers[i].buffer_pool.add(j-1);
				}
			}
		}
	}

	~communicator()
	{
		MPI_Finalize(); // TODO(improvement): check on error and create output if there was one
		HAM_DEBUG( HAM_LOG << "~communicator" << std::endl; )
	}


	request allocate_request(node_t remote_node)
	{
		HAM_DEBUG( HAM_LOG << "communicator::allocate_next_request(): remote_node = " << remote_node << std::endl; )

		const size_t send_buffer_index = peers[remote_node].buffer_pool.allocate();
		const size_t recv_buffer_index = peers[remote_node].buffer_pool.allocate();

		return { remote_node, this_node_, send_buffer_index, recv_buffer_index };
	}

	void free_request(request& req)
	{
		assert(req.valid());
		assert(req.source_node == this_node_);
	
		mpi_peer& peer = peers[req.target_node];

		peer.buffer_pool.free(req.send_buffer_index);
		peer.buffer_pool.free(req.recv_buffer_index);
		req.valid_ = false;
	}

public:
	void send_msg(request_reference_type req, void* msg, size_t size)
	{
		// copy message from caller into transfer buffer
		void* msg_buffer = static_cast<void*>(&peers[req.target_node].msg_buffers[req.send_buffer_index]);
		memcpy(msg_buffer, msg, size);
		MPI_Isend(msg_buffer, size, MPI_BYTE, req.target_node, constants::DEFAULT_TAG, MPI_COMM_WORLD, &req.next_mpi_request());
	}
	
	// to be used by the offload target's main loop: synchronously receive one message at a time
	// NOTE: the local static receive buffer!
	void* recv_msg_host(void* msg = nullptr, size_t size = constants::MSG_SIZE)
	{
		HAM_UNUSED_VAR(msg);
		static msg_buffer buffer; // NOTE !
		MPI_Recv(&buffer, size, MPI_BYTE, host_node_, constants::DEFAULT_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		return static_cast<void*>(&buffer);
	}

	// trigger receiving the result of a message on the sending side
	void recv_result(request_reference_type req)
	{
		// nothing todo here, since this communicator implementation uses one-sided communication
		// the data is already where it is expected (in the buffer referenced in req)
		MPI_Irecv(static_cast<void*>(&peers[req.target_node].msg_buffers[req.recv_buffer_index]), constants::MSG_SIZE, MPI_BYTE, req.target_node, constants::RESULT_TAG, MPI_COMM_WORLD, &req.next_mpi_request());
		return;
	}

	template<typename T>
	void send_data(T* local_source, buffer_ptr<T> remote_dest, size_t size)
	{
		MPI_Send((void*)local_source, size * sizeof(T), MPI_BYTE, remote_dest.node(), constants::DATA_TAG, MPI_COMM_WORLD);
	}

	// to be used by the host
	template<typename T>
	void send_data_async(request_reference_type req, T* local_source, buffer_ptr<T> remote_dest, size_t size)
	{
		MPI_Isend((void*)local_source, size * sizeof(T), MPI_BYTE, remote_dest.node(), constants::DATA_TAG, MPI_COMM_WORLD, &req.next_mpi_request());
	}


	template<typename T>
	void recv_data(buffer_ptr<T> remote_source, T* local_dest, size_t size)
	{
		MPI_Recv((void*)local_dest, size * sizeof(T), MPI_BYTE, remote_source.node(), constants::DATA_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	
	// to be used by the host
	template<typename T>
	void recv_data_async(request_reference_type req, buffer_ptr<T> remote_source, T* local_dest, size_t size)
	{
		MPI_Irecv(static_cast<void*>(local_dest), size * sizeof(T), MPI_BYTE, remote_source.node(), constants::DATA_TAG, MPI_COMM_WORLD, &req.next_mpi_request());
	}

	template<typename T>
	buffer_ptr<T> allocate_buffer(const size_t n, node_t source_node)
	{
		HAM_UNUSED_VAR(source_node); // NOTE: might be needed in other communicator implementations
		T* ptr;
		//int err =
		posix_memalign((void**)&ptr, constants::CACHE_LINE_SIZE, n * sizeof(T));
		// NOTE: no ctor is called
		return buffer_ptr<T>(ptr, this_node_);
	}

	template<typename T>
	void free_buffer(buffer_ptr<T> ptr)
	{
		assert(ptr.node() == this_node_);
		// NOTE: no dtor is called
		free(static_cast<void*>(ptr.get()));
	}

	static communicator& instance() { return *instance_; }
	static node_t this_node() { return instance().this_node_; }
	static size_t num_nodes() { return instance().nodes_; }
	bool is_host() { return this_node_ == 0; } // TODO(improvement): ham_address == ham_host_address ; }
	bool is_host(node_t node) { return node == 0; } // TODO(improvement): node == ham_host_address; }

	static const node_descriptor& get_node_description(node_t node)
	{
		return instance().node_descriptions[node];
	}

private:
	static communicator* instance_;
	node_t this_node_;
	size_t nodes_;
	node_t host_node_;
	std::vector<node_descriptor> node_descriptions; // not as member in peer below, because Allgather is used to exchange node descriptions
		
	struct mpi_peer {
		buffer_ptr<msg_buffer> msg_buffers; // buffers used for MPI_ISend and IRecv by the sender

		// needed by sender to manage which buffers are in use and which are free
		// just manages indices, that can be used by
		detail::resource_pool<size_t> buffer_pool;
	};
	
	mpi_peer* peers;
};

template<typename T>
buffer_ptr<T>::buffer_ptr() : buffer_ptr(nullptr, communicator::this_node()) { }

template<typename T>
T& buffer_ptr<T>::operator[](size_t i)
{
	assert(node_ == communicator::this_node());
	return ptr_[i];
}

} // namespace net
} // namespace ham

#endif // ham_net_communicator_mpi_hpp
