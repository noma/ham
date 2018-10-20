// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_net_communicator_mpi_rma_dynamic_hpp
#define ham_net_communicator_mpi_rma_dynamic_hpp

#include <mpi.h>

#include <cassert>
#include <cstring> // memcpy
#include <stdlib.h> // posix_memalign

#include "ham/misc/constants.hpp"
#include "ham/misc/resource_pool.hpp"
#include "ham/misc/types.hpp"
#include "ham/util/debug.hpp"
#include "ham/util/log.hpp"
// #include "ham/util/time.hpp"
#include "communicator.hpp"

namespace ham {
namespace net {

template<typename T>
class buffer_ptr {
public:
	buffer_ptr();
    buffer_ptr(T* ptr, node_t node) : ptr_(ptr), node_(node), mpi_address_(0) { }
	buffer_ptr(T* ptr, node_t node, MPI_Aint mpi_address) : ptr_(ptr), node_(node), mpi_address_(mpi_address) { }


	T* get() { return ptr_; }
	node_t node() { return node_; }
    MPI_Aint get_mpi_address() { return mpi_address_; }

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
    MPI_Aint mpi_address_;
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
	enum {
        NO_BUFFER_INDEX = constants::MSG_BUFFERS, // invalid buffer index (max valid + 1)
        FLAG_FALSE = constants::MSG_BUFFERS + 1 // special value, outside normal index range
    };

    // externally used interface of request must be shared across all communicator-implementations
	class request {
	public:
		request() : valid_(false) {} // instantiate invalid
		
		request(node_t target_node, node_t source_node, size_t remote_buffer_index, size_t local_buffer_index)
		 : target_node(target_node), source_node(source_node), valid_(true), remote_buffer_index(remote_buffer_index), local_buffer_index(local_buffer_index), req_count(0), uses_rma_(false)
		{}

		// return true if request was finished
        // will not work as intended for rma ops, no equivalent to test() available for remote completion
		bool test()
		{
			// int flag = 0;

            // MPI_Testall(req_count, mpi_reqs, &flag, MPI_STATUS_IGNORE); // just test the receive request, since the send belonging to the request triggers the remote send that is received

            /*
            if(uses_rma_)
            {
                HAM_DEBUG( HAM_LOG << "request::test(), warning: may give false positive on rma remote completion" << std::endl; )
            }

            return flag != 0;
            */
            return communicator::instance().test_local_flag(target_node, local_buffer_index);
		}

		void* get() // blocks
		{

            HAM_DEBUG( HAM_LOG << "request::get(), before MPI_Waitall()" << std::endl; )
			MPI_Waitall(req_count, mpi_reqs, MPI_STATUS_IGNORE); // must wait for all requests to satisfy the standard
            // for async get from receive_data_async() this will block until get is completed
            HAM_DEBUG( HAM_LOG << "request::get(), after MPI_Waitall()" << std::endl; )

            if(uses_rma_)  {
                // this will only be true for async rma data transfers
                // there will be no result returned, so this won't poll on anything and return a dummy instead.
                return nullptr;
            } else {
                return communicator::instance().recv_msg(target_node, local_buffer_index);
            }
		}

		template<class T>
		void send_result(T* result_msg, size_t size)
		{
			assert(communicator::this_node() == target_node); // this assert fails if send_result is called from the wrong side
			
			// TODO(improvement, low priority): better go through communicator, such that no MPI calls are anywhere else
			// MPI_Send(result_msg, size, MPI_BYTE, source_node, constants::RESULT_TAG, MPI_COMM_WORLD);
			communicator::instance().send_msg(source_node, local_buffer_index, NO_BUFFER_INDEX, result_msg, size);
		}

		bool valid() const
		{
			return valid_;
		}

        bool uses_rma() const
        {
            return uses_rma_;
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
        bool uses_rma_;

		// only needed by the sender
		enum { NUM_REQUESTS = 3 };
		
		size_t remote_buffer_index; // buffer to use for sending the message
		size_t local_buffer_index; // buffer to use for receiving the result
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

        /*
        if (is_host()) {

            for (node_t i = 1; i < nodes_; ++i) { // TODO(improvement): needs to be changed when host-rank becomes configurable
                // allocate buffers
                peers[i].msg_buffers = allocate_peer_buffer<msg_buffer>(constants::MSG_BUFFERS, this_node_);
                // fill resource pools
                for (size_t j = constants::MSG_BUFFERS; j > 0; --j) {
                    peers[i].buffer_pool.add(j - 1);
                }
            }
        }*/

        // initialise data windows
        for (node_t i = 0; i < nodes_; ++i) {
            // dynamic data window
            MPI_Win_create_dynamic(MPI_INFO_NULL, MPI_COMM_WORLD, &(peers[i].rma_data_win));
        }

        // initialise message windows
        for (node_t i = 0; i < nodes_; ++i) { // loop through ranks

            if (i == this_node_) { // create local windows with allocated memory for targets, host creates one inbound set of windows for all targets

                // allocate memory and create windows
                if (this_node_ == host_node_) { // host creates one large window with subsets associated with different targets

                    // (MSG_SIZE+FLAG_SIZE) * MSG_BUFFERS * num_nodes = bytes of memory allocated (sizes are implicit in msg_flag_buffer struct)
                    peers[this_node_].msg_flag_data = allocate_peer_buffer<msg_flag_buffer>(constants::MSG_BUFFERS * nodes_, this_node_);
                    // peers[this_node_].flag_data = allocate_peer_buffer<cache_line_buffer>(constants::MSG_BUFFERS * nodes_, this_node_);
                    // set flags to FLAG_FALSE
                    reset_flags(peers[this_node_].msg_flag_data, constants::MSG_BUFFERS * nodes_); // TODO: Daniel - this may be bad if buffer structs are not contiguos - check

                    // fill resource pools for managing indices on the host
                    for (size_t j = 0; j < nodes_; ++j) {
                        for (size_t k = constants::MSG_BUFFERS; k > 0; --k) {
                            // target buffers
                            peers[j].local_buffer_pool.add(k - 1);
                            peers[j].remote_buffer_pool.add(k - 1);
                        }
                        // allocate first next_request,
                        allocate_next_request(j);
                    }
                    // create window with memory
                    MPI_Win_create((peers[this_node_].msg_flag_data.get()), sizeof(msg_flag_buffer) * constants::MSG_BUFFERS * nodes_, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &(peers[this_node_].msg_flag_win));
                    // MPI_Win_create((peers[this_node_].flag_data.get()), sizeof(cache_line_buffer) * constants::MSG_BUFFERS * nodes_, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &(peers[this_node_].flag_win));

                } else { // targets create one window with the size of their msg "queue"
                    // (MSG_SIZE+FLAG_SIZE) * MSG_BUFFERS = bytes of memory allocated (sizes are implicit in msg_flag_buffer struct)
                    peers[this_node_].msg_flag_data = allocate_peer_buffer<msg_flag_buffer>(constants::MSG_BUFFERS, this_node_);
                    // peers[this_node_].flag_data = allocate_peer_buffer<cache_line_buffer>(constants::MSG_BUFFERS, this_node_);
                    // set flags to FLAG_FALSE
                    reset_flags(peers[this_node_].flag_data, constants::MSG_BUFFERS);

                    // create window with memory
                    MPI_Win_create((peers[this_node_].msg_flag_data.get()), sizeof(msg_buffer) * constants::MSG_BUFFERS, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &(peers[this_node_].msg_flag_win));
                    // MPI_Win_create((peers[this_node_].flag_data.get()), sizeof(cache_line_buffer) * constants::MSG_BUFFERS, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &(peers[this_node_].flag_win));
                }

                // debug msg
                HAM_DEBUG( std::cout << "Rank: " << this_node_ << " in loop run " << i << " created REAL windows..." << std::endl; )


            } else { // create remote windows without memory (join the collective call and retreive the window handle)

                MPI_Win_create(nullptr, 0, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &(peers[i].msg_flag_win));
                // MPI_Win_create(nullptr, 0, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &(peers[i].flag_win));
                // debug msg
                HAM_DEBUG( std::cout << "Rank: " << this_node_ << " in loop run " << i << " creating EMPTY windows..." << std::endl; )
                //MPI_Win_allocate(0, 1, MPI_INFO_NULL, MPI_COMM_WORLD, peers[i].msg_win_data, &(peers[i].rma_msg_win));
                //MPI_Win_allocate(0, 1, MPI_INFO_NULL, MPI_COMM_WORLD, peers[i].flag_win_data, &(peers[i].rma_flag_win));
            }
        }

		// get all locks to targets for data
        // targets lock to other targets for copies
        for (node_t i = 0; i < nodes_; ++i) { // TODO(improvement): needs to be changed when host-rank becomes configurable
            if (i != this_node_) {
                MPI_Win_lock(MPI_LOCK_SHARED, i, 0, peers[i].rma_data_win);  // shared locks because all ranks lock on every target concurrently
            }
        }

        // MPI_Barrier(MPI_COMM_WORLD);


        /* // locking will be done when accessing remote memory
        // locks for active message rma transfers
        if (this_node_ != host_node_) { // targets
            MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, peers[0].msg_win);
            MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, peers[0].flag_win);
        } else { // host
            for (node_t i = 0; i < nodes_; ++i) {
                MPI_Win_lock(MPI_LOCK_SHARED, i, 0, peers[i].msg_win);
                MPI_Win_lock(MPI_LOCK_SHARED, i, 0, peers[i].flag_win);
            }
        }
        */

        HAM_DEBUG( HAM_LOG << "communicator::communicator(): rma window creation completed" << std::endl; )
        HAM_DEBUG( HAM_LOG << "communicator::communicator(): communicator initialization completed" << std::endl; )
	}

	~communicator()
	{
		MPI_Finalize(); // TODO(improvement): check on error and create output if there was one
		HAM_DEBUG( HAM_LOG << "~communicator" << std::endl; )
	}

    // this is only used by the host to manage remote msg buffers and local reply buffers and assign them to requests
    const request& allocate_next_request(node_t remote_node)
    {
        // this allocates a host-managed index for the remote nodes msg "queue"
        // so the host knows which buffers are available on the target
        const size_t remote_buffer_index = peers[remote_node].remote_buffer_pool.allocate();
        // this allocates an index in the hosts "reply queue"
        // request is included in offload message, so the target knows into which buffers replys must be written
        // when used, the index will need to be added to an offset determined by a targets rank to address the part of the buffer belonging to this target
        // NOTE: the actual host buffer is stored at the hosts peers[0], but the buffer_pools are stored at the corresponding peers[target]
        // buffer_pools manage idices within the targets section of the hosts buffer
        const size_t local_buffer_index = peers[remote_node].local_buffer_pool.allocate();

        peers[remote_node].next_request = {remote_node, this_node_, remote_buffer_index, local_buffer_index};

        return peers[remote_node].next_request;
    }

    // only used by host
	request allocate_request(node_t remote_node)
	{
        HAM_DEBUG( HAM_LOG << "communicator::allocate_next_request(): remote_node = " << remote_node << std::endl; )

		return peers[remote_node].next_request;
	}

    // used for rma data transfers, so they wont take up unneeded buffer indices
    // only put() and get() use this, copy() offloads an active msg to the data source and therefore uses allocate_request()
    request allocate_data_request(node_t remote_node) {
        HAM_DEBUG( HAM_LOG << "communicator::allocate_next_request(): remote_node = " << remote_node << std::endl; )
        return { remote_node, this_node_, NO_BUFFER_INDEX, NO_BUFFER_INDEX };
    }

    // only used by host
	void free_request(request& req)
	{
		assert(req.valid());
		assert(req.source_node == this_node_);

        // dont do any of the following for data transfer requests
        if(req.remote_buffer_index == NO_BUFFER_INDEX ) {
            return;
        }

        mpi_peer& peer = peers[req.target_node];


        // set flag for buffer indices associated with request to false
        // local flag is inside the hosts large array of msg_flag_buffers @ peers[host]
        // index offset computed using target node
        // TODO: Daniel - figure out access to flag memory
        size_t offset = constants::MSG_BUFFERS * req.target_node; // offset msg_flag_buffers to the corresponding nodes region
        volatile size_t* local_flag = reinterpret_cast<size_t*>(&peers[host_node_].msg_flag_data.get()[offset + req.local_buffer_index]); // this will point to the beginning of a msg_flag_buffer
        *local_flag = FLAG_FALSE;
        // remote flag on target
        /* This is done by the target after having reveived the new index to poll on
        size_t remote_flag = FLAG_FALSE;
        MPI_Put(&remote_flag, sizeof(remote_flag), MPI_BYTE, req.target_node, 0, sizeof(remote_flag), MPI_BYTE, peer.flag_win);
        // flush? don't think so
        */

        peer.remote_buffer_pool.free(req.remote_buffer_index);

        peer.local_buffer_pool.free(req.local_buffer_index);

        req.valid_ = false;
    }

public:
    // make private?!
    // called by func below
    void send_msg(node_t node, size_t buffer_index, size_t next_buffer_index, void* msg, size_t size) {
        // write msg to target msg buffer
        HAM_DEBUG( HAM_LOG << "communicator::send_msg(): node =  " << node << std::endl; )
        HAM_DEBUG( HAM_LOG << "communicator::send_msg(): remote buffer index = " << buffer_index << std::endl; )

        if (node != host_node_) { // to targets
            // ham::util::time::statistics msg_put(1,0);
            // ham::util::time::statistics flush(1,0);
            // ham::util::time::statistics flag_put(1,0);

            // ham::util::time::timer t1;
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, node, 0, peers[node].msg_win);
            MPI_Put(msg, size, MPI_BYTE, node, sizeof(msg_buffer) * buffer_index, size, MPI_BYTE, peers[node].msg_win);
            // msg_put.add(t1);
            HAM_DEBUG( HAM_LOG << "communicator::send_msg(): wrote msg" << std::endl; )
            HAM_DEBUG( HAM_LOG << "communicator::send_msg(): writing msg took: " << ""/*msg_put.min().count()*/ << std::endl; )


            // TODO DANIEL: because MPI does not guarantee order on RMA ops, there might be a FLUSH necessary here
            // unlock includes flush, no need for it here
            MPI_Win_unlock(node, peers[node].msg_win);
            // ham::util::time::timer t2;
            // MPI_Win_flush(node, peers[node].msg_win);
            // flush.add(t2);
            // HAM_DEBUG( HAM_LOG << "communicator::send_msg(): flushed msg" << std::endl; )
            // HAM_DEBUG( HAM_LOG << "communicator::send_msg(): flushing msg took: " << ""/*flush.min().count()*/ << std::endl; )

            // write flag to target flags buffer
            // not sure on the size here?
            // ham::util::time::timer t3;
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, node, 0, peers[node].flag_win);
            MPI_Put(&next_buffer_index, sizeof(next_buffer_index), MPI_BYTE, node, sizeof(cache_line_buffer) * buffer_index, sizeof(next_buffer_index), MPI_BYTE, peers[node].flag_win);
            // flag_put.add(t3);
            HAM_DEBUG( HAM_LOG << "communicator::send_msg(): wrote flag" << std::endl; )
            HAM_DEBUG( HAM_LOG << "communicator::send_msg(): writing flag took: " << ""/*flag_put.min().count()*/ <<std::endl; )
            MPI_Win_unlock(node, peers[node].flag_win);

        } else { // to host, used by send_result
            // ham::util::time::statistics msg_put(1,0);
            // ham::util::time::statistics flush(1,0);
            // ham::util::time::statistics flag_put(1,0);

            // compute offset in the hosts window
            size_t offset = constants::MSG_BUFFERS * this_node_;
            HAM_DEBUG( HAM_LOG << "communicator::send_msg(): using msg host-offset (bytes): " << offset*sizeof(msg_buffer) << std::endl; )
            // ham::util::time::timer t1;
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, node, 0, peers[node].msg_win);
            MPI_Put(msg, size, MPI_BYTE, node, sizeof(msg_buffer) * (offset + buffer_index), size, MPI_BYTE, peers[node].msg_win);
            // msg_put.add(t1);
            HAM_DEBUG( HAM_LOG << "communicator::send_msg(): wrote msg" << std::endl; )
            HAM_DEBUG( HAM_LOG << "communicator::send_msg(): writing msg took: " << ""/*msg_put.min().count()*/ << std::endl; )
            MPI_Win_unlock(node, peers[node].msg_win);

            // ham::util::time::timer t2;
            // MPI_Win_flush(node, peers[node].msg_win);
            // flush.add(t2);
            HAM_DEBUG( HAM_LOG << "communicator::send_msg(): flushed msg" << std::endl; )
            HAM_DEBUG( HAM_LOG << "communicator::send_msg(): flushing msg took: " << ""/*flush.min().count()*/ << std::endl; )

            // ham::util::time::timer t3;
            HAM_DEBUG( HAM_LOG << "communicator::send_msg(): using flag host-offset (bytes): " << offset*sizeof(cache_line_buffer) << std::endl; )
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, node, 0, peers[node].flag_win);
            MPI_Put(&next_buffer_index, sizeof(next_buffer_index), MPI_BYTE, node, sizeof(cache_line_buffer) * (offset + buffer_index), sizeof(next_buffer_index), MPI_BYTE, peers[node].flag_win);
            // flag_put.add(t3);
            MPI_Win_unlock(node, peers[node].flag_win);
            HAM_DEBUG( HAM_LOG << "communicator::send_msg(): wrote flag" << std::endl; )
            HAM_DEBUG( HAM_LOG << "communicator::send_msg(): writing flag took: " << ""/*flag_put.min().count()*/ <<std::endl; )

        }
    }

	void send_msg(request_reference_type req, void* msg, size_t size)
	{
		/*
        // copy message from caller into transfer buffer
		void* msg_buffer = static_cast<void*>(&peers[req.target_node].msg_buffers[req.send_buffer_index]);
		memcpy(msg_buffer, msg, size);
		MPI_Isend(msg_buffer, size, MPI_BYTE, req.target_node, constants::DEFAULT_TAG, MPI_COMM_WORLD, &req.next_mpi_request());
	    */

        const request& next_req = allocate_next_request(req.target_node); // allocate_next_req needed??
        send_msg(req.target_node, req.remote_buffer_index, next_req.remote_buffer_index, msg, size);
    }

    // make private?!
    // called by function below
    void* recv_msg(node_t node, size_t buffer_index = NO_BUFFER_INDEX, void* msg = nullptr, size_t size = constants::MSG_SIZE)
    {
        // ham::util::time::statistics pre_poll(1,0);
        // ham::util::time::statistics poll(1,0);
        // ham::util::time::timer t1;
        buffer_index = buffer_index == NO_BUFFER_INDEX ? peers[node].next_flag : buffer_index;
        HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): remote node is: " << node << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): using buffer index: " << buffer_index << std::endl; )


        size_t *local_flag;
        size_t received_flag = FLAG_FALSE;

		// needed on host to access the memory belonging to the node from which to receive
		size_t offset = (this_node_ == host_node_) ? constants::MSG_BUFFERS * node : 0;

        if (this_node_ == host_node_) {
            local_flag = reinterpret_cast<size_t*>(&peers[host_node_].flag_data.get()[offset + buffer_index]);
        } else {
            local_flag = reinterpret_cast<size_t*>(&peers[this_node_].flag_data.get()[buffer_index]);
        }


        HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): FLAG before polling: " << (int)*local_flag << std::endl; )
        // pre_poll.add(t1);
        HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): pre-polling took: " << ""/*pre_poll.min().count()*/ << std::endl; )
        // ham::util::time::timer t2;
		HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): polling at offset (bytes): " << offset * sizeof(cache_line_buffer) << ""/*pre_poll.min().count()*/ << std::endl; )



        while (received_flag == FLAG_FALSE) {
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, this_node_, 0, peers[this_node_].flag_win);
            MPI_Get(&received_flag, sizeof(cache_line_buffer), MPI_BYTE, this_node_, (offset + buffer_index) * sizeof(cache_line_buffer) , sizeof(cache_line_buffer), MPI_BYTE, peers[this_node_].flag_win);
            MPI_Win_unlock(this_node_, peers[this_node_].flag_win);
        } // poll on flag for completion
        // poll.add(t2);
        HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): FLAG after polling: " << (int)*local_flag << std::endl; )
        HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): polling took: " << ""/*poll.min().count()*/ << std::endl; )

        // reset the flag (thanks mpi for requiring me to get a lock for that again...
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, this_node_, 0, peers[this_node_].flag_win);
        *local_flag = FLAG_FALSE;
        MPI_Win_unlock(this_node_, peers[this_node_].flag_win);

        // make sure message window is updated locally too
        MPI_Win_lock(MPI_LOCK_SHARED, this_node_, 0, peers[this_node_].msg_win);
        MPI_Win_unlock(this_node_, peers[this_node_].msg_win);

        if (received_flag != NO_BUFFER_INDEX) // the flag contains the next buffer index to poll on
            peers[node].next_flag = received_flag;

        HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): done " << ""/*poll.min().count()*/ << std::endl; )

        if (this_node_ == host_node_) {
            size_t offset = constants::MSG_BUFFERS * node;
            return &peers[host_node_].msg_data.get()[offset + buffer_index];
        } else {
            return &peers[this_node_].msg_data.get()[buffer_index];
        }
    }

	// to be used by the offload target's main loop: synchronously receive one message at a time
	// NOTE: the local static receive buffer!
	void* recv_msg_host(void* msg = nullptr, size_t size = constants::MSG_SIZE)
	{
		/* static msg_buffer buffer; // NOTE !
		MPI_Recv(&buffer, size, MPI_BYTE, host_node_, constants::DEFAULT_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        return static_cast<void*>(&buffer); */
        return recv_msg(host_node_, NO_BUFFER_INDEX, msg, size);
	}

	// trigger receiving the result of a message on the sending side
	void recv_result(request_reference_type req)
	{
		// nothing todo here, since this communicator implementation uses one-sided communication
		// the data will be written to where it is expected

        // MPI_Irecv(static_cast<void*>(&peers[req.target_node].msg_buffers[req.recv_buffer_index]), constants::MSG_SIZE, MPI_BYTE, req.target_node, constants::RESULT_TAG, MPI_COMM_WORLD, &req.next_mpi_request());
		return;
	}

    // only used by the host
    bool test_local_flag(node_t node, size_t buffer_index)
    {
        size_t offset = (constants::MSG_SIZE + constants::FLAG_SIZE) * node;
        volatile size_t * local_flag = reinterpret_cast<size_t*>(&peers[node].flag_data.get()[buffer_index]);
        return *local_flag != FLAG_FALSE;
    }

    void reset_flags(buffer_ptr<cache_line_buffer> flags, size_t size)
	{
		cache_line_buffer fill_value;
		cache_line_buffer* fill_value_ptr = &fill_value;
		// null fill_value
		std::fill(reinterpret_cast<unsigned char*>(fill_value_ptr), reinterpret_cast<unsigned char*>(fill_value_ptr) + sizeof(cache_line_buffer), 0);
		// set to flag false
		*reinterpret_cast<size_t*>(fill_value_ptr) = FLAG_FALSE;
		// set all flags to fill_value
		std::fill(flags.get(), flags.get() + size, fill_value);
	}

	// in MPI RMA backend only used by copy
	// host uses async version
	// targets don't send data to host as host uses rma get
	template<typename T>
	void send_data(T* local_source, buffer_ptr<T> remote_dest, size_t size)
	{
		// execute transfer
        MPI_Put(local_source, size * sizeof(T), MPI_BYTE, remote_dest.node(), remote_dest.get_mpi_address(), size * sizeof(T), MPI_BYTE, peers[remote_dest.node()].rma_data_win);
        MPI_Win_flush(remote_dest.node(), peers[remote_dest.node()].rma_data_win);
	}

	// to be used by the host only
	template<typename T>
	void send_data_async(request_reference_type req, T* local_source, buffer_ptr<T> remote_dest, size_t size)
	{
        req.uses_rma_ = true;

        MPI_Rput(local_source, size * sizeof(T), MPI_BYTE, remote_dest.node(), remote_dest.get_mpi_address(), size * sizeof(T), MPI_BYTE, peers[remote_dest.node()].rma_data_win, &req.next_mpi_request());
	}

	// not used in MPI RMA backend
	// host uses async version
	// targets don't use get
	// should be safe to remove
	template<typename T>
	void recv_data(buffer_ptr<T> remote_source, T* local_dest, size_t size)
	{
		MPI_Get(local_dest, size * sizeof(T), MPI_BYTE, remote_source.node(), remote_source.get_mpi_address(), size * sizeof(T), MPI_BYTE, peers[remote_source.node()].rma_data_win);
		MPI_Win_flush(remote_source.node(), peers[remote_source.node()].rma_data_win);
	}
	
	// to be used by the host
	template<typename T>
	void recv_data_async(request_reference_type req, buffer_ptr<T> remote_source, T* local_dest, size_t size)
	{
        req.uses_rma_ = true;

		MPI_Rget(local_dest, size * sizeof(T), MPI_BYTE, remote_source.node(), remote_source.get_mpi_address(), size * sizeof(T), MPI_BYTE, peers[remote_source.node()].rma_data_win, &req.next_mpi_request());
	}

	template<typename T>
	buffer_ptr<T> allocate_buffer(const size_t n, node_t source_node)
	{
		T* ptr;
		//int err =
		posix_memalign((void**)&ptr, constants::CACHE_LINE_SIZE, n * sizeof(T));

        // attach to own window
        HAM_DEBUG( HAM_LOG << "communicator::allocate_buffer(), allocating buffer @: " << (long)ptr << std::endl; )
        MPI_Win_attach(peers[this_node_].rma_data_win, (void*)ptr, n * sizeof(T));

		MPI_Aint mpi_address;
		MPI_Get_address((void*)ptr, &mpi_address);
		// NOTE: no ctor is called
		return buffer_ptr<T>(ptr, this_node_, mpi_address);
	}

	// for host to allocate peer message buffers, needed because original function now manages dynamic window for data buffers
	template<typename T>
	buffer_ptr<T> allocate_peer_buffer(const size_t n, node_t source_node)
	{
        T* ptr;
		posix_memalign((void**)&ptr, constants::CACHE_LINE_SIZE, n * sizeof(T));
		// NOTE: no ctor is called
		return buffer_ptr<T>(ptr, this_node_);
	}

    // used for data buffers only
	template<typename T>
	void free_buffer(buffer_ptr<T> ptr)
	{
		assert(ptr.node() == this_node_);
		// NOTE: no dtor is called
        // remove from own rma window
        HAM_DEBUG( HAM_LOG << "communicator::free_buffer(), freeing buffer @: " << (long)ptr.get() << " belonging to node: " << ptr.node() << std::endl; )
        MPI_Win_detach(peers[this_node_].rma_data_win, ptr.get());
        /* for (node_t i = 1; i < nodes_; ++i) { // nonsense, all accesses to a rank will only take place on that targets window, no need to attach to other
            MPI_Win_detach(peers[i].rma_data_win, ptr.get());
        } */
		free(static_cast<void*>(ptr.get()));
	}

    // for host to free peer message buffers, needed because original function now manages rma window which must not happen for host-only local buffers
	template<typename T>
	void free_peer_buffer(buffer_ptr<T> ptr)
	{
        // TODO DANIEL: this is where mem is freed that should be mapped to static mpi windows
        // i dont think this is ever called on the actual memory mapped to static mpi windows, freeing it would equal "disconnecting" corresponding target
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
		buffer_ptr<msg_buffer> msg_buffers; // buffers used for MPI_ISend and IRecv by the sender // buffers used for MPI_RPut and RGet

		// needed by sender to manage which buffers are in use and which are free
		// just manages indices, that can be used by
		detail::resource_pool<size_t> local_buffer_pool;
        detail::resource_pool<size_t> remote_buffer_pool;

        request next_request;
        size_t next_flag = 0;
        // NOTE: behind these buffers are MSG_BUFFERS many buffers of size MSG_SIZE+FLAG_SIZE, indices are managed by buffer_pool

        // static window for inbound rma messages and their flags
        buffer_ptr<msg_flag_buffer> msg_flag_data;
        MPI_Win msg_flag_win;

		// mpi rma dynamic window for data
		MPI_Win rma_data_win;
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
