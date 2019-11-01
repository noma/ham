// Copyright (c) 2013-2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_net_communicator_veo_only_ve_hpp
#define ham_net_communicator_veo_only_ve_hpp

#include "ham/net/communicator_veo_base.hpp"
#include "ham/net/communicator_veo_common_ve.hpp"

// VEO C-linkage interface
extern "C" {

}

namespace ham {
namespace net {

class communicator : public communicator_base<communicator> {
	friend class communicator_base<communicator>; // allow request in base class to call functions from here
public:
	communicator(communicator_options& comm_options)
	  : communicator_base<communicator>(this),
	    ham_process_count(ham_comm_veo_ve_process_count(0, false)),
	    ham_host_address(ham_comm_veo_ve_host_address(0, false)),
	    ham_address(ham_comm_veo_ve_address(0, false)),
	    device_number(ham_comm_veo_ve_device_number(0, false))
	{
		HAM_UNUSED_VAR(comm_options);
		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: begin." << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: ham_process_count = " << ham_process_count << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: ham_host_address = " << ham_host_address << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: ham_address = " << ham_address << std::endl; )

		// we are definitely not the host
		assert(!is_host());

		// allocate peer data structures
		peers = new veo_peer[ham_process_count]; // TODO: this seems to cause crash in the dynamically linked VEO build

		// set VH name as hostname
		errno_handler(gethostname(peers[ham_host_address].node_description.name_, node_descriptor::name_length_), "gethostname");

		// set own node name locally as "hostname/VE<NR>"
		std::stringstream ss;
		ss << peers[ham_host_address].node_description.name_ // VH-name = hostname (set above)
		   << '/' << "VE" << device_number;
		strncpy(peers[ham_address].node_description.name_, ss.str().c_str(), node_descriptor::name_length_);

		veo_peer& host_peer = peers[ham_host_address]; // NOTE: in current VEO backend we need a struct for ourselfes only, having the data to communicate with the host

		// NOTE: Memory for communication buffers was allocated by the host
		//       using the provided C-function for VEO offload.
		//       We use the received addresses to setup our buffter_ptr<>
		//       objects.

		// NOTE: These functions also reset the flags, so the host can 
		//       actually send messages into the buffers and set the flags
		//       before the VE-side communicator is even initialised.

		// allocate actual receive buffers
		host_peer.local_buffers = buffer_ptr<msg_buffer>       (reinterpret_cast<msg_buffer*>       (ham_comm_veo_ve_local_buffers_addr(0)), ham_address);
		host_peer.local_flags   = buffer_ptr<cache_line_buffer>(reinterpret_cast<cache_line_buffer*>(ham_comm_veo_ve_local_flags_addr(0)),   ham_address);

		// allocate "remote_buffers" used to store results
		host_peer.remote_buffers = buffer_ptr<msg_buffer>       (reinterpret_cast<msg_buffer*>       (ham_comm_veo_ve_remote_buffers_addr(0)), ham_address);
		host_peer.remote_flags   = buffer_ptr<cache_line_buffer>(reinterpret_cast<cache_line_buffer*>(ham_comm_veo_ve_remote_flags_addr(0)),   ham_address);

		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: host_peer.local_buffers.get() = "  << (uint64_t)host_peer.local_buffers.get()  << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: host_peer.local_flags.get() = "    << (uint64_t)host_peer.local_flags.get()    << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: host_peer.remote_buffers.get() = " << (uint64_t)host_peer.remote_buffers.get() << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: host_peer.remote_flags.get() = "   << (uint64_t)host_peer.remote_flags.get()   << std::endl; )

		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: end." << std::endl; )
	}

	~communicator()
	{
		HAM_DEBUG( HAM_LOG << "communicator(VE)::~communicator()" << std::endl; )

		// delete
		delete [] peers;
	}

private:
	// pre-allocates the next request and modifies remote_node's internal peer data
	const request& allocate_next_request(node_t remote_node)
	{
		HAM_DEBUG( HAM_LOG << "communicator(VE)::allocate_next_request(): remote_node = " << remote_node << std::endl; )

		const size_t remote_buffer_index = peers[remote_node].remote_buffer_pool.allocate();
		const size_t local_buffer_index = peers[remote_node].local_buffer_pool.allocate();

		{
			HAM_DEBUG(
			request& req = peers[remote_node].next_request;
			HAM_LOG << "communicator(VE)::allocate_next_request(): old next_request = " <<
				"request(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl );
		}

		peers[remote_node].next_request = { remote_node, remote_buffer_index, ham_address, local_buffer_index };

		{
			HAM_DEBUG(
			request& req = peers[remote_node].next_request;
			HAM_LOG << "communicator(VE)::allocate_next_request(): new next_request = " <<
				"request(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl );
		}

		return peers[remote_node].next_request;
	}

public:
	request allocate_request(node_t remote_node)
	{
		{
			HAM_DEBUG(
			HAM_LOG << "communicator(VE)::allocate_request(): remote_node = " << remote_node << std::endl;
			request& req = peers[remote_node].next_request;
			HAM_LOG << "communicator(VE)::allocate_request(): returning next_request = " <<
				"request(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl );
		}
		// there is always one pre_allocated request, that corresponds to the next buffer index written to the receiver in the last send
		return peers[remote_node].next_request;
	}

	void free_request(request_reference_type req)
	{
		assert(req.source_node == ham_address);

		veo_peer& peer = peers[req.target_node];

		// set flags
		volatile size_t* local_flag = reinterpret_cast<size_t*>(&peer.local_flags.get()[req.source_buffer_index]);
		volatile size_t* remote_flag = reinterpret_cast<size_t*>(&peer.remote_flags[req.target_buffer_index]);
		*local_flag = FLAG_FALSE;
		*remote_flag = FLAG_FALSE;
//		_mm_sfence(); // make preceding stores globally visible

		// pool indices
		peer.remote_buffer_pool.free(req.target_buffer_index);
		peer.local_buffer_pool.free(req.source_buffer_index);

		// invalidate request
		req.target_buffer_index = NO_BUFFER_INDEX;
	}

	void send_msg(request_reference_type req, void* msg, size_t size)
	{
		const request& next_req = allocate_next_request(req.target_node); // pre-allocate-request for the next send, because we set this index on the remote size

		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_msg(): " <<
			"request(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl );

		send_msg(req.target_node, req.target_buffer_index, next_req.target_buffer_index, msg, size);
//		send_msg(req.target_node, req.target_buffer_index, peers[req.target_node].remote_buffer_pool.next(), msg, size);
	}

private:
	void send_msg(node_t node, size_t buffer_index, size_t next_buffer_index, void* msg, size_t size)
	{
		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_msg(): node =  " << node << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_msg(): remote buffer index = " << buffer_index << std::endl; )

		char* remote_buffer = reinterpret_cast<char*>(&peers[node].remote_buffers[buffer_index]);
		volatile size_t* remote_flag = reinterpret_cast<size_t*>(&peers[node].remote_flags[buffer_index]);
		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_msg(): remote buffer is: " << (void*)remote_buffer << std::endl; )

		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_msg(): sending message of size: " << size << std::endl; )
		// copy to remote memory
		memcpy((char*)remote_buffer, (void*)&size, sizeof(size_t)); // size = header

		memcpy((char*)remote_buffer + sizeof(size_t), msg , size);
//		_mm_sfence(); // NOTE: intel intrinsic: store fence, make changes visible on the remote site to which we wrote

		*remote_flag = next_buffer_index; // signal remote side that the message has been written, and transfer the next buffer/flag index in the process
//		_mm_sfence(); // NOTE: intel intrinsic: store fence, make changes visible on the remote site
	}

private:
	void* recv_msg(node_t node, size_t buffer_index = NO_BUFFER_INDEX, void* msg = nullptr, size_t size = constants::MSG_SIZE)
	{
		HAM_UNUSED_VAR(msg);
		// use next_flag as index, if none is given
		buffer_index = buffer_index == NO_BUFFER_INDEX ?  peers[node].next_flag : buffer_index;
		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_msg(): remote node is: " << node << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_msg(): using buffer index: " << buffer_index << std::endl; )

		char* local_buffer = reinterpret_cast<char*>(&peers[node].local_buffers.get()[buffer_index]);
		volatile size_t* local_flag = reinterpret_cast<size_t*>(&peers[node].local_flags.get()[buffer_index]);
		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_msg(): local buffer is: " << (uint64_t)local_buffer << std::endl; )

		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_msg(): FLAG at " << (uint64_t)local_flag << " before polling: " << (int)*local_flag << std::endl; )
		while (*local_flag == FLAG_FALSE); // poll on flag
		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_msg(): FLAG after polling: " << (int)*local_flag << std::endl; )

		if (*local_flag != NO_BUFFER_INDEX) // the flag contains the next buffer index to poll on
			peers[node].next_flag = *local_flag;

		// copy size from recv buffer
		memcpy((void*)&size, (char*)local_buffer, sizeof(size_t));

		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_msg(): received msg size: " << size << std::endl; )
		//HAM_DEBUG( HAM_LOG << "receiving message of size: " << size << std::endl; )

//		_mm_lfence(); // NOTE: intel intrinsic: all prior loads are globally visible
		return (char*)local_buffer + sizeof(size_t); // we directly return our buffer here, which is safe, since it can only be re-used after being freed by the future which returns the result by value to the user
	}

	bool test_local_flag(node_t node, size_t buffer_index)
	{
		volatile size_t* local_flag = reinterpret_cast<size_t*>(&peers[node].local_flags.get()[buffer_index]);
		return *local_flag != FLAG_FALSE; // NO_BUFFER_INDEX; // set from the other side by send_result below
	}

public:
	// receive offload messages from the host
	void* recv_msg_host(void* msg = nullptr, size_t size = constants::MSG_SIZE)
	{
		return recv_msg(ham_host_address, NO_BUFFER_INDEX, msg, size);
	}
	
	// trigger receiving the result of a message on the sending side
	void recv_result(request_reference_type req)
	{
		HAM_UNUSED_VAR(req);
		// nothing to do here, since this communicator implementation uses one-sided communication
		// the data is already where it is expected (in the buffer referenced in req)
		return;
	}

	template<typename T>
	void send_data(T* local_source, buffer_ptr<T>& remote_dest, size_t size)
	{
		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_data(): writing " << size << " byte from " << local_source << " to " << remote_dest.get() << std::endl; )
		// NOTE: not supported on target
		HAM_UNUSED_VAR(local_source);
		HAM_UNUSED_VAR(remote_dest);
		HAM_UNUSED_VAR(size);
		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_data(): Operation not supported on VE." << std::endl; )
		exit(1);
	}

	template<typename T>
	void recv_data(buffer_ptr<T>& remote_source, T* local_dest, size_t size)
	{

		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_data(): reading " << size << " byte from " << remote_source.get() << " to " << local_dest << std::endl; )
		// NOTE: not supported on target
		HAM_UNUSED_VAR(remote_source);
		HAM_UNUSED_VAR(local_dest);
		HAM_UNUSED_VAR(size);
		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_data(): Operation not supported on VE." << std::endl; )
		exit(1);
	}

	static const node_descriptor& get_node_description(node_t node)
	{
		return instance().peers[node].node_description;
	}

	static node_t this_node() { return instance().ham_address; }
	static size_t num_nodes() { return instance().ham_process_count; }
	bool is_host() const { return ham_address == ham_host_address ; }
	bool is_host(node_t node) const { return node == ham_host_address; }

private:
	// NOTE:: set before ctor via VEO C-interface
	const uint64_t ham_process_count; // number of participating processes
	const node_t ham_host_address; // the address of the host process
	const node_t ham_address; // this processes' address
	const uint64_t device_number; // our VE device number as used by OS

	struct veo_peer {
		veo_peer() : next_flag(0) //, remote_buffer_pool(constants::MSG_BUFFERS), remote_flag_pool(constants::MSG_BUFFERS)
		{}

		request next_request; // the next request, belonging to next flag
		size_t next_flag; // flag

		// pointer to the peer's receive buffer
		buffer_ptr<msg_buffer> local_buffers; // local memory, remote peer writes to these buffers
		buffer_ptr<cache_line_buffer> local_flags; // local memory, remote peer signals writing is complete via these flags, I poll on this flag

		buffer_ptr<msg_buffer> remote_buffers; // remote memory, I write messages to this peer into these buffers
		buffer_ptr<cache_line_buffer> remote_flags; // remote memory, I write these flags to signal a message was sent

		// needed by sender to manage which buffers are in use and which are free
		// just manages indices, that can be used by
		// the sender to go into mapped_remote_buffers/flags
		// the receiver to go into local_buffers/flags
		detail::resource_pool<size_t> remote_buffer_pool;
		detail::resource_pool<size_t> local_buffer_pool;

		node_descriptor node_description;
	};

	// pointers to arrays of veo_peers, index is peer address
	veo_peer* peers = nullptr;

public:

	// return address to own node descriptor
	node_descriptor* veo_ve_get_host_node_descriptor() 
	{
		return &(peers[ham_address].node_description); 
	}
	
	void veo_ve_set_node_descriptor(void* addr)
	{
		// mem_cpy
		peers[ham_address].node_description.name_[0] = 't';
		memcpy(addr, &(peers[ham_address].node_description), sizeof(node_descriptor));
	}

};

} // namespace net
} // namespace ham

// definitions that need a complete type for communicator
#include "ham/net/communicator_veo_only_post.hpp"

#endif // ham_net_communicator_veo_only_ve_hpp
