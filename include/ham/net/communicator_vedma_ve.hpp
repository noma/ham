// Copyright (c) 2013-2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_net_communicator_vedma_ve_hpp
#define ham_net_communicator_vedma_ve_hpp

#include "ham/net/communicator_vedma.hpp"

#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>

extern "C" {
#include <vhshm.h>
#include <vedma.h>
}

// ve_inst.h
extern "C" {
static inline void ve_inst_fenceSF(void)
{
	asm ("fencem 1":::"memory");
}

static inline void ve_inst_fenceLF(void)
{
	asm ("fencem 2":::"memory");
}

static inline void ve_inst_fenceLSF(void)
{
	asm ("fencem 3":::"memory");
}

static inline uint64_t ve_inst_lhm(void *vehva)
{
	uint64_t ret;
	asm ("lhm.l %0,0(%1)":"=r"(ret):"r"(vehva));
	return ret;
}

static inline void ve_inst_shm(void *vehva, uint64_t value)
{
    asm ("shm.l %0,0(%1)"::"r"(value),"r"(vehva));
}
}

// VEO C-linkage interface
extern "C" {

// TODO: cleanup
// TODO: exchange node descriptors
//uint64_t ham_comm_veo_ve_get_node_descriptor();
//uint64_t ham_comm_veo_ve_set_descriptor_addr(uint64_t addr);

uint64_t ham_comm_veo_ve_shm_key(uint64_t key);
uint64_t ham_comm_veo_ve_shm_size(uint64_t size);

uint64_t ham_comm_veo_ve_local_buffers_addr(uint64_t addr);
uint64_t ham_comm_veo_ve_local_flags_addr(uint64_t addr);
uint64_t ham_comm_veo_ve_remote_buffers_addr(uint64_t addr);
uint64_t ham_comm_veo_ve_remote_flags_addr(uint64_t addr);

uint64_t ham_comm_veo_ve_address(uint64_t address, uint64_t set);
uint64_t ham_comm_veo_ve_process_count(uint64_t process_count, uint64_t set);
uint64_t ham_comm_veo_ve_host_address(uint64_t host_address, uint64_t set);

}

namespace ham {
namespace net {

class communicator : public communicator_base<communicator> {
	friend class communicator_base<communicator>; // allow request in base class to call functions from here
public:
	communicator(int argc, char* argv[])
	  : communicator_base<communicator>(this),
	    ham_process_count(ham_comm_veo_ve_process_count(0, false)),
	    ham_host_address(ham_comm_veo_ve_host_address(0, false)),
   	    ham_address(ham_comm_veo_ve_address(0, false))
	{
		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: begin." << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: ham_process_count = " << ham_process_count << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: ham_host_address = " << ham_host_address << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: ham_address = " << ham_address << std::endl; )

		// allocate peer data structures
		peers = new veo_peer[ham_process_count]; // TODO: this seems to cause crash in the dynamically linked VEO build

		// get own hostname
		errno_handler(gethostname(peers[ham_address].node_description.name_, peers[ham_address].node_description.name_length_), "gethostname");

		// we are definitly not the host
		assert(!is_host());

		veo_peer& host_peer = peers[ham_host_address]; // NOTE: in current VEO backend we need a struct for ourselfes only, having the data to communicate with the host

// #################### BEGIN TODO ####################
		int err = 0;

		// initialise DMA
		err = ve_dma_init();
		if (err)
			std::cout << "VE: Failed to initialize DMA" << std::endl;

		key_t shm_key = (key_t)ham_comm_veo_ve_shm_key(0);
		size_t shm_size = (key_t)ham_comm_veo_ve_shm_size(0);

		std::cout << "VE: (shm_key = " << shm_key << ", shm_size = " << shm_size << ")" << std::endl;

		// get SHM id from key
		int shm_id = vh_shmget(shm_key, shm_size, SHM_HUGETLB);
		if (shm_id == -1)
			std::cout << "VE: vh_shmget failed, reason: " << strerror(errno) << std::endl;
		
		// attach shared memoy VH address space and resiter to to DMAATB
		// the region is accessible for DMA unter its VEHVA remote_vehva
		host_peer.shm_remote_vehva = 0;
		host_peer.shm_remote_addr = nullptr;			
		host_peer.shm_remote_addr = vh_shmat(shm_id, NULL, 0, (void **)&host_peer.shm_remote_vehva);
		
		if (host_peer.shm_remote_addr == nullptr)
			std::cout << "VE: (host_peer.shm_remote_addr == nullptr) " << std::endl;
		if (host_peer.shm_remote_vehva == (uint64_t)-1)
			std::cout << "VE: (host_peer.shm_remote_vehva == -1) " << std::endl;

		// get local memory of same size as SHM on VH
		host_peer.shm_local_addr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_64MB, -1, 0);
		
		if (host_peer.shm_local_addr == nullptr)
			std::cout << "VE: (host_peer.shm_local_addr == nullptr) " << std::endl;

		// register local buffer for DMA
		host_peer.shm_local_vehva = ve_register_mem_to_dmaatb(host_peer.shm_local_addr, shm_size);
		if (host_peer.shm_local_vehva == (uint64_t)-1)
			std::cout << "VE: host_peer.shm_local_vehva == -1 " << std::endl;

		// setup addresses
		// NOTE: assumed SHM layout
		// message buffers remote on VH -> local on VE
		// message buffers local on VH -> remote on VE
		// message flags remote
		// message flags local
		
		// LAYOUT above in local DMA-capable memory: host_peer.shm_local_addr, host_peer.shm_local_vehva
		
		const size_t msg_buffers_size = constants::MSG_BUFFERS * sizeof(msg_buffer); // defaults to 4 KiB per msg 
		const size_t msg_flags_size = constants::MSG_BUFFERS * sizeof(size_t); // 8 byte per flag

		// NOTE: use the message buffer part of our local memory, we do not need to copy the flags
		// addr and vehva for message buffers in local memory
		// corresponds to remote_send_buffers in SHM segment
		host_peer.local_recv_buffers_addr = (char*)host_peer.shm_local_addr;
		host_peer.local_recv_buffers_vehva = host_peer.shm_local_vehva;
		// corresponds to remote_recv_buffers in SHM segment
		host_peer.local_send_buffers_addr = (char*)host_peer.shm_local_addr + msg_buffers_size;
		host_peer.local_send_buffers_vehva = host_peer.shm_local_vehva + msg_buffers_size;

		// LAYOUT above in remote VH SHAM segment: host_peer.shm_remote_addr, host_peer.shm_remote_vehva		
		// NOTE we mirror the VE layout here
		
		host_peer.remote_send_buffers_addr = host_peer.shm_remote_addr;
		host_peer.remote_send_buffers_vehva = host_peer.shm_remote_vehva;

		host_peer.remote_send_flags_addr = (char*)host_peer.shm_remote_addr + (2 * msg_buffers_size);
		host_peer.remote_send_flags_vehva = host_peer.shm_remote_vehva + (2 * msg_buffers_size);

		host_peer.remote_recv_buffers_addr = (char*)host_peer.shm_remote_addr + msg_buffers_size;
		host_peer.remote_recv_buffers_vehva = host_peer.shm_remote_vehva + msg_buffers_size;

		host_peer.remote_recv_flags_addr = (char*)host_peer.shm_remote_addr + (2 * msg_buffers_size + msg_flags_size);
		host_peer.remote_recv_flags_vehva = host_peer.shm_remote_vehva + (2 * msg_buffers_size + msg_flags_size);
		

		
// #################### END TODO ####################
		// NOTE: Memory for communication buffers was allocated by the host
		//       using the provided C-function for VEO offload.
		//       We use the received addresses to setup our buffter_ptr<>
		//       objects.

		// NOTE: These functions also reset the flags, so the host can 
		//       actually send messages into the buffers and set the flags
		//       before the VE-side communicator is even initialised.

//		// allocate actual receive buffers
//		host_peer.local_buffers = buffer_ptr<msg_buffer>       (reinterpret_cast<msg_buffer*>       (ham_comm_veo_ve_local_buffers_addr(0)), ham_address);
//		host_peer.local_flags   = buffer_ptr<cache_line_buffer>(reinterpret_cast<cache_line_buffer*>(ham_comm_veo_ve_local_flags_addr(0)),   ham_address);

//		// allocate "remote_buffers" used to store results
//		host_peer.remote_buffers = buffer_ptr<msg_buffer>       (reinterpret_cast<msg_buffer*>       (ham_comm_veo_ve_remote_buffers_addr(0)), ham_address);
//		host_peer.remote_flags   = buffer_ptr<cache_line_buffer>(reinterpret_cast<cache_line_buffer*>(ham_comm_veo_ve_remote_flags_addr(0)),   ham_address);

//		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: host_peer.local_buffers.get() = "  << (uint64_t)host_peer.local_buffers.get()  << std::endl; )
//		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: host_peer.local_flags.get() = "    << (uint64_t)host_peer.local_flags.get()    << std::endl; )
//		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: host_peer.remote_buffers.get() = " << (uint64_t)host_peer.remote_buffers.get() << std::endl; )
//		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: host_peer.remote_flags.get() = "   << (uint64_t)host_peer.remote_flags.get()   << std::endl; )


		HAM_DEBUG( HAM_LOG << "communicator(VE)::communicator: end." << std::endl; )
	}

	~communicator()
	{
		HAM_DEBUG( HAM_LOG << "communicator(VE)::~communicator()" << std::endl; )
		int err = 0;
		err = ve_unregister_mem_from_dmaatb(peers[ham_host_address].shm_local_vehva);
		if (err)
			std::cout << "VE: Failed to unregister local buffer from DMAATB" << std::endl;

		err = vh_shmdt(peers[ham_host_address].shm_remote_addr);
		if (err)
			std::cout << "VE: Failed to detach from VH sysV shm" << std::endl;

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
				"req(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl );
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
		assert(false); // TODO: is this actually called on VE side?
		assert(req.source_node == ham_address);

		veo_peer& peer = peers[req.target_node];

		// set flags
		size_t* local_flag = reinterpret_cast<size_t*>((char*)peer.remote_send_flags_addr + req.source_buffer_index * sizeof(size_t));
		size_t* remote_flag = reinterpret_cast<size_t*>((char*)peer.remote_recv_flags_addr + req.target_buffer_index * sizeof(size_t));
// TODO: set using ve stuff
		*local_flag = FLAG_FALSE;
		*remote_flag = FLAG_FALSE;

//		volatile size_t* local_flag = reinterpret_cast<size_t*>(&peer.local_flags.get()[req.source_buffer_index]);
//		volatile size_t* remote_flag = reinterpret_cast<size_t*>(&peer.remote_flags[req.target_buffer_index]);
//		*local_flag = FLAG_FALSE;
//		*remote_flag = FLAG_FALSE;
//		_mm_sfence(); // make preceding stores globally visible

		// pool indices
		peer.remote_buffer_pool.free(req.target_buffer_index);
		peer.local_buffer_pool.free(req.source_buffer_index);

		// invalidate request
		req.target_buffer_index = NO_BUFFER_INDEX;
	}

	// TODO: has to write msg into remote_memory
	void send_msg(request_reference_type req, void* msg, size_t size)
	{
		const request& next_req = allocate_next_request(req.target_node); // pre-allocate-request for the next send, because we set this index on the remote size

		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_msg(): " <<
			"request(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl );

		send_msg(req.target_node, req.target_buffer_index, next_req.target_buffer_index, msg, size);
		//send_msg(req.target_node, req.target_buffer_index, peers[req.target_node].remote_buffer_pool.next(), msg, size);
	}

private:
	void send_msg(node_t node, size_t buffer_index, size_t next_buffer_index, void* msg, size_t size)
	{
//size = 1024; // TODO: remove: for quick and dirty testing of larger message sizes
		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_msg(): node =  " << node << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_msg(): remote buffer index = " << buffer_index << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_msg(): msg size is: " << size << std::endl; )

		// TODO: compute
		uint64_t remote_recv_flag_vehva = peers[node].remote_recv_flags_vehva + buffer_index * sizeof(size_t);
		uint64_t remote_recv_buffer_vehva = peers[node].remote_recv_buffers_vehva + buffer_index * sizeof(msg_buffer);

		uint64_t local_send_buffer_vehva = peers[node].local_send_buffers_vehva + buffer_index * sizeof(msg_buffer);
		void* local_send_buffer_addr = (char*)peers[node].local_send_buffers_addr + buffer_index * sizeof(msg_buffer);
		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_msg(): remote recv flag SHM offset is: " << (remote_recv_flag_vehva - peers[node].shm_remote_vehva) << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_msg(): remote recv buffer SHM offset is: " << (remote_recv_buffer_vehva - peers[node].shm_remote_vehva) << std::endl; )
		
		// BEGIN: VERSION A

		// DMA message to VH
		// construct message in local buffer: size + msg
		memcpy((char*)local_send_buffer_addr, (void*)&size, sizeof(size_t)); // size = header
		memcpy((char*)local_send_buffer_addr + sizeof(size_t), msg, size);
		const size_t msg_buffer_size = sizeof(size_t) + size;

		int err = 0;
		err = ve_dma_post_wait(remote_recv_buffer_vehva, local_send_buffer_vehva, msg_buffer_size);
		if (err)
			std::cout << "VE: ve_dma_post_wait has failed!, err = " << err << std::endl;
//*/
		// END: VERSION A

		// BEGIN: VERSION B
/*
		// write msg size
		ve_inst_shm((void *)remote_recv_buffer_vehva, size);
		// write msg in chunks
		size_t chunk_size = sizeof(uint64_t);
		size_t chunks = size % chunk_size == 0 ? (size / chunk_size) : (size / chunk_size + 1);
		for (size_t i = 0; i < chunks; ++i)
			ve_inst_shm((void *)(remote_recv_buffer_vehva + sizeof(size_t) + i * chunk_size), *reinterpret_cast<uint64_t*>(msg) + i);
//*/
		// END: VERSION B

		// set flag
		ve_inst_shm((void *)remote_recv_flag_vehva, next_buffer_index);
		ve_inst_fenceSF();

/*
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
*/
	}

private:
	void* recv_msg(node_t node, size_t buffer_index = NO_BUFFER_INDEX, void* msg = nullptr, size_t size = constants::MSG_SIZE)
	{
		// use next_flag as index, if none is given
		buffer_index = buffer_index == NO_BUFFER_INDEX ?  peers[node].next_flag : buffer_index;
		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_msg(): remote node is: " << node << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_msg(): using buffer index: " << buffer_index << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_msg(): msg size is: " << size << std::endl; )

		// compute addresses
		uint64_t remote_send_flag_vehva = peers[node].remote_send_flags_vehva + buffer_index * sizeof(size_t);
		uint64_t remote_send_buffer_vehva = peers[node].remote_send_buffers_vehva + buffer_index * sizeof(msg_buffer);
		uint64_t local_recv_buffer_vehva = peers[node].local_recv_buffers_vehva + buffer_index * sizeof(msg_buffer);
		void* local_recv_buffer_addr = (char*)peers[node].local_recv_buffers_addr + buffer_index * sizeof(msg_buffer);

		// poll on flag
		size_t local_flag = FLAG_FALSE;
		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_msg(): FLAG at SHM offset: " << (remote_send_flag_vehva - peers[node].shm_remote_vehva) << " before polling: " << local_flag << std::endl; )
		ve_inst_fenceLF();
		do {
			local_flag = ve_inst_lhm((void *)remote_send_flag_vehva);
		} while (local_flag == FLAG_FALSE);

		// DMA polling: TODO: debug, and benchmark
//		void* local_flag_addr = (char*)peers[node].shm_local_addr  + 2 * constants::MSG_BUFFERS * sizeof(msg_buffer);
//		uint64_t local_flag_vehva = peers[node].shm_local_vehva + 2 * constants::MSG_BUFFERS * sizeof(msg_buffer);
//		*((size_t*)local_flag_addr) = FLAG_FALSE; // reset flag copy
//		do {
//			int err = 0;
//			err = ve_dma_post_wait(local_flag_vehva, remote_send_flag_vehva, sizeof(uint64_t));

//		} while (*((size_t*)local_flag_addr) == FLAG_FALSE);

		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_msg(): FLAG after polling: " << local_flag << std::endl; )

		if (local_flag != NO_BUFFER_INDEX) // the flag contains the next buffer index to poll on
			peers[node].next_flag = local_flag;
			
		// TODO: 2 options:
		// a) dma whole message (4 KiB)
		// b) get size, then DMA message
		// we do B

		// get size
		ve_inst_fenceLF();
		size = ve_inst_lhm((void *)remote_send_buffer_vehva);
//size = 1024; // TODO: remove: for quick and dirty testing of larger message sizes
		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_msg(): size = " << size << std::endl; )
		
		// BEGIN: VERSION A

		// DMA message
		int err = 0;
		err = ve_dma_post_wait(local_recv_buffer_vehva, remote_send_buffer_vehva + sizeof(size), size); // NOTE: copy without
		if (err)
			std::cout << "VE: ve_dma_post_wait has failed!, err = " << err << std::endl;
		//*/
		// END: VERSION A

		// BEGIN: VERSION B
		/*
		// read msg in chunks
		size_t chunk_size = sizeof(uint64_t);
		size_t chunks = size % chunk_size == 0 ? (size / chunk_size) : (size / chunk_size + 1);
		for (size_t i = 0; i < chunks; ++i)
			*(reinterpret_cast<uint64_t*>(local_recv_buffer_addr) + i) = ve_inst_lhm((void *)(remote_send_buffer_vehva + sizeof(size_t) + i * chunk_size));
		//*/
		// END: VERSION B

		//return (char*)local_recv_buffer_addr + sizeof(size_t);
		return (char*)local_recv_buffer_addr; // NOTE: we copied without size header

// ------------------------
/*
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
*/
	}

	bool test_local_flag(node_t node, size_t buffer_index)
	{
		assert(false); // TODO: is this actually called on VE side?
		volatile size_t* local_flag = reinterpret_cast<size_t*>((char*)peers[node].remote_send_flags_addr + buffer_index *sizeof(size_t));
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
		// nothing todo here, since this communicator implementation uses one-sided communication
		// the data is already where it is expected (in the buffer referenced in req)
		return;
	}


	template<typename T>
	void send_data(T* local_source, buffer_ptr<T>& remote_dest, size_t size)
	{
		// NOTE: not supported on target
		HAM_DEBUG( HAM_LOG << "communicator(VE)::send_data(): writing " << size << " byte from " << local_source << " to " << remote_dest.get() << std::endl; )
	}

	template<typename T>
	void recv_data(buffer_ptr<T>& remote_source, T* local_dest, size_t size)
	{
		// NOTE: not supported on target
		HAM_DEBUG( HAM_LOG << "communicator(VE)::recv_data(): reading " << size << " byte from " << remote_source.get() << " to " << local_dest << std::endl; )
	}

	static const node_descriptor& get_node_description(node_t node)
	{
		return instance().peers[node].node_description;
	}

	static node_t this_node() { return instance().ham_address; }
	static size_t num_nodes() { return instance().ham_process_count; }
	bool is_host() { return ham_address == ham_host_address ; }
	bool is_host(node_t node) { return node == ham_host_address; }

private:
	// NOTE:: set before ctor via VEO C-interface
	const uint64_t ham_process_count; // number of participating processes
	const node_t ham_host_address; // the address of the host process
	const node_t ham_address; // this processes' address

	struct veo_peer {
		veo_peer() : next_flag(0) //, remote_buffer_pool(constants::MSG_BUFFERS), remote_flag_pool(constants::MSG_BUFFERS)
		{}

		request next_request; // the next request, belonging to next flag

		// SHM 
		uint64_t shm_local_vehva;
		void* shm_local_addr;
		
		uint64_t shm_remote_vehva;
		void* shm_remote_addr;
		
		// addresess 
		void* local_recv_buffers_addr = nullptr;
		uint64_t local_recv_buffers_vehva = 0;

		void* local_send_buffers_addr = nullptr;
		uint64_t local_send_buffers_vehva = 0;
		
		void* remote_send_buffers_addr = nullptr;
		uint64_t remote_send_buffers_vehva = 0;

		void* remote_send_flags_addr = nullptr;
		uint64_t remote_send_flags_vehva = 0;

		void* remote_recv_buffers_addr = nullptr;
		uint64_t remote_recv_buffers_vehva = 0;

		void* remote_recv_flags_addr = nullptr;
		uint64_t remote_recv_flags_vehva = 0;
	
		
		// buffer to copy received messages from VH to
		buffer_ptr<msg_buffer> recv_buffers; // local memory, recv_msg copies data to these buffers using veo_read_mem()

		size_t next_flag; // flag

		node_descriptor node_description;

// TODO: remove after cleanup
		// pointer to the peer's receive buffer
//		buffer_ptr<msg_buffer> local_buffers; // local memory, remote peer writes to these buffers
//		buffer_ptr<cache_line_buffer> local_flags; // local memory, remote peer signals writing is complete via these flags, I poll on this flag

//		buffer_ptr<msg_buffer> remote_buffers; // remote memory, I write messages to this peer into these buffers
//		buffer_ptr<cache_line_buffer> remote_flags; // remote memory, I write these flags to signal a message was sent

		// needed by sender to manage which buffers are in use and which are free
		// just manages indices, that can be used by
		// the sender to go into mapped_remote_buffers/flags
		// the receiver to go into local_buffers/flags
		detail::resource_pool<size_t> remote_buffer_pool;
		detail::resource_pool<size_t> local_buffer_pool;

	};

	// pointers to arrays of buffers, index is peer address
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
#include "ham/net/communicator_vedma_post.hpp"

#endif // ham_net_communicator_vedma_ve_hpp
