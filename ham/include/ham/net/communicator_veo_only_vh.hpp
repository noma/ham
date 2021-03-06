// Copyright (c) 2013-2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_net_communicator_veo_only_vh_hpp
#define ham_net_communicator_veo_only_vh_hpp

#include "ham/net/communicator_veo_base.hpp"
#include "ham/misc/options.hpp"

#include <algorithm>
#include <string>
#include <string.h> // strncpy
#include <ve_offload.h>

namespace ham {
namespace net {

class communicator : public communicator_base<communicator> {
	friend class communicator_base<communicator>; // allow request in base class to call functions from here
public:
	communicator(communicator_options& comm_options)
	: communicator_base(this),
	  ham_process_count(comm_options.ham_process_count()),
	  ham_host_address(comm_options.ham_host_address()),
	  ham_address(ham_host_address), // this process' address
	  veo_library_path(comm_options.veo_library_path()),
	  veo_ve_nodes(comm_options.veo_ve_nodes())
	{
		HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator: begin." << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator: using VE nodes: " << veo_ve_nodes << std::endl; )

		// we are definitely the host
		assert(is_host());

		// TODO: convenience: generate default ve_node_list, if arg not provided
		if (veo_ve_nodes.empty())
			HAM_LOG << "communicator(VH)::communicator: error: please provide --ham-veo-ve-nodes with --ham-process-count minus 1 comma-separated values." << std::endl;

		// seprate by comma
		auto ve_node_list_strs = ::ham::detail::split(veo_ve_nodes, ',');
		// parse entries from str to node_t
		std::vector<node_t> ve_node_list(ve_node_list_strs.size(), 0); // list of VE nodes to use
		std::transform(ve_node_list_strs.begin(), ve_node_list_strs.end(), ve_node_list.begin(),
		               [](std::string str) -> node_t { return std::stoi(str); });

		if (ham_process_count != (ve_node_list.size() + 1))
			HAM_LOG << "communicator(VH)::communicator: error: please make sure --ham-veo-ve-nodes contains --ham-process-count minus 1 comma-separated values." << std::endl;

		// setup peer data structures
		peers = new veo_peer[ham_process_count];

		// get own hostname (VH)
		errno_handler(gethostname(peers[ham_address].node_description.name_, node_descriptor::name_length_), "gethostname");

		// (ham_process count - 1) iterations, i.e. targets
		int ve_list_index = 0;
		for (node_t i = 0; i < static_cast<node_t>(ham_process_count); ++i) {
			if(i != ham_host_address) { // omit the host address
				HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator: setting up HAM node " << i << " with VE " << ve_node_list[ve_list_index] << std::endl; )
				veo_peer& peer = peers[i];
				int err = 0;

				// create VE process
#ifdef HAM_COMM_VEO_STATIC
				peer.veo_proc = veo_proc_create_static(ve_node_list[ve_list_index], veo_library_path.c_str());
				if (peer.veo_proc == 0) {
					HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator: error: veo_proc_create_static(" << ve_node_list[ve_list_index] << ") returned 0" << std::endl; );
					exit(1); // TODO: check how we terminate elsewhere to be consistent
				}
#else
				peer.veo_proc = veo_proc_create(ve_node_list[ve_list_index]);
				if (peer.veo_proc == 0) {
					HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator: error: veo_proc_create() returned 0" << std::endl; );
					exit(1); // TODO: check how we terminate elsewhere to be consistent
				}
#endif
				// TODO: nicify and unify error handling

				// load VE library image 
#ifdef HAM_COMM_VEO_STATIC
				peer.veo_lib_handle = 0;
#else
				peer.veo_lib_handle = veo_load_library(peer.veo_proc, veo_library_path.c_str());
				HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator: veo_load_library() returned handle: " << (void*)peer.veo_proc << std::endl; );
#endif
				// VEO context
				peer.veo_main_context = veo_context_open(peer.veo_proc);
				HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator: veo_context_open() returned ctx: " << peer.veo_main_context << std::endl; );
				
				// allocate local communication buffers
				peer.recv_buffers = allocate_buffer<msg_buffer>(constants::MSG_BUFFERS, ham_address); // local host memory

				// allocate communication buffers on the target
				err = veo_alloc_mem(peer.veo_proc, &peer.local_buffers_addr, sizeof(msg_buffer) * constants::MSG_BUFFERS);
				errno_handler(err, "veo_alloc_mem(local_buffers_addr)");
				err = veo_alloc_mem(peer.veo_proc, &peer.local_flags_addr, sizeof(cache_line_buffer) * constants::MSG_BUFFERS);
				errno_handler(err, "veo_alloc_mem(local_flags_addr)");

				err = veo_alloc_mem(peer.veo_proc, &peer.remote_buffers_addr, sizeof(msg_buffer) * constants::MSG_BUFFERS);
				errno_handler(err, "veo_alloc_mem(remote_buffers_addr)");
				err = veo_alloc_mem(peer.veo_proc, &peer.remote_flags_addr, sizeof(cache_line_buffer) * constants::MSG_BUFFERS);
				errno_handler(err, "veo_alloc_mem(remote_flags_addr)");

				// lambda to set allocated addresses at target
				auto set_target_address = [&](const char* func_name, uint64_t addr) {
					struct veo_args *argp = veo_args_alloc();
					err = veo_args_set_u64(argp, 0, addr); // set argument
					errno_handler(err, "veo_args_set_u64()");
					uint64_t sym = veo_get_sym(peer.veo_proc, peer.veo_lib_handle, func_name);
					assert(sym != 0); // i.e. symbol was found
					uint64_t id = veo_call_async(peer.veo_main_context, sym, argp);
					assert(id != VEO_REQUEST_ID_INVALID);
					uint64_t res_addr = 0;
					err = veo_call_wait_result(peer.veo_main_context, id, &res_addr); // sync on call
					errno_handler(err, "veo_call_wait_result()");
					assert(res_addr == 0); // NOTE: if function is used as setter the expected return is 0
					veo_args_free(argp);
				};

				//uint64_t ham_comm_veo_ve_local_buffers_addr(uint64_t addr);
				set_target_address("ham_comm_veo_ve_local_buffers_addr", peer.local_buffers_addr);

				//uint64_t ham_comm_veo_ve_local_flags_addr(uint64_t addr);
				set_target_address("ham_comm_veo_ve_local_flags_addr", peer.local_flags_addr);
			
				//uint64_t ham_comm_veo_ve_remote_buffers_addr(uint64_t addr);
				set_target_address("ham_comm_veo_ve_remote_buffers_addr", peer.remote_buffers_addr);
			
				//uint64_t ham_comm_veo_ve_remote_flags_addr(uint64_t addr);
				set_target_address("ham_comm_veo_ve_remote_flags_addr", peer.remote_flags_addr);

				// fill resource pools
				for (size_t j = constants::MSG_BUFFERS; j > 0; --j) {
					peer.remote_buffer_pool.add(j-1);
					peer.local_buffer_pool.add(j-1);
				}

				HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator(VH): node " << i << " host_peer.local_buffers_addr = "  << peer.local_buffers_addr << std::endl; )
				HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator(VH): node " << i << " host_peer.local_flags_addr = "    << peer.local_flags_addr << std::endl; )
				HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator(VH): node " << i << " host_peer.remote_buffers_addr = " << peer.remote_buffers_addr << std::endl; )
				HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator(VH): node " << i << " host_peer.remote_flags_addr = "   << peer.remote_flags_addr << std::endl; )

				// allocate the first request to be used for the next send
				allocate_next_request(i);

				// lambda to set the command line arguments at target (instead of passing argc and argv and pasing on the target again...
				auto set_target_argument = [&](const char* func_name, uint64_t value) {
					struct veo_args *argp = veo_args_alloc();
					err = veo_args_set_u64(argp, 0, value); // set argument
					errno_handler(err, "veo_args_set_u64(0)");
					err = veo_args_set_u64(argp, 1, static_cast<uint64_t>(true)); // set argument
					errno_handler(err, "veo_args_set_u64(1)");
					uint64_t sym = veo_get_sym(peer.veo_proc, peer.veo_lib_handle, func_name);
					assert(sym != 0); // i.e. symbol was found
					uint64_t id = veo_call_async(peer.veo_main_context, sym, argp);
					assert(id != VEO_REQUEST_ID_INVALID);
					uint64_t res_addr = 0;
					err = veo_call_wait_result(peer.veo_main_context, id, &res_addr); // sync on call
					errno_handler(err, "veo_call_wait_result()");
					assert(res_addr == 0); // NOTE: if function is used as setter the expected return is 0
					veo_args_free(argp);
				};

				// uint64_t ham_comm_veo_ve_address(uint64_t address, uint64_t set);
				set_target_argument("ham_comm_veo_ve_address", i);
				// uint64_t ham_comm_veo_ve_process_count(uint64_t process_count, uint64_t set);
				set_target_argument("ham_comm_veo_ve_process_count", ham_process_count);
				// uint64_t ham_comm_veo_ve_host_address(uint64_t host_address, uint64_t set);
				set_target_argument("ham_comm_veo_ve_host_address", ham_host_address);
				// uint64_t ham_comm_veo_ve_device_number(uint64_t device_number, uint64_t set);
				set_target_argument("ham_comm_veo_ve_device_number", ve_node_list[ve_list_index]);

				// set node name locally as "hostname/VE<NR>"
				std::stringstream ss;
				ss << peers[ham_host_address].node_description.name_ // VH-name = hostname (set above)
				   << '/' << "VE" << ve_node_list[ve_list_index];
				strncpy(peer.node_description.name_, ss.str().c_str(), node_descriptor::name_length_);

// TODO: exchange node descriptions
/*
std::cerr << "getting node desc" << std::endl;
		// get node descriptor from target

		{
			int err;
			// allocate memory at the target
			uint64_t mem_addr = 0;
			err = veo_alloc_mem(peer.veo_proc, &mem_addr, sizeof(node_descriptor));
			errno_handler(err, "veo_alloc_mem");
			std::cerr << "allocated target memory for node_descriptor: " << mem_addr << std::endl;

			// get address of node descriptor
			struct veo_args *argp = veo_args_alloc();
			uint64_t sym = veo_get_sym(peer.veo_proc, handle, "ham_comm_veo_ve_get_node_descriptor");
			//uint64_t id = veo_call_async_by_name(peer.veo_main_context, handle, "ham_comm_veo_ve_get_node_descriptor", argp);
			uint64_t id = veo_call_async(peer.veo_main_context, sym, argp);
			uint64_t res_nd_addr = 0;
			veo_call_wait_result(peer.veo_main_context, id, &res_nd_addr);
			veo_args_free(argp);

			// set address of node descriptor
//			struct veo_args *argp = veo_args_alloc();
//			err = veo_args_set_u64(argp, 0, mem_addr);
//			errno_handler(err, "veo_args_set_u64");
//			uint64_t sym = veo_get_sym(peer.veo_proc, handle, "ham_comm_veo_ve_set_descriptor_addr");
//			assert(sym != 0); // i.e. symbol was found
//			//uint64_t id = veo_call_async_by_name(peer.veo_main_context, handle, "ham_comm_veo_ve_get_node_descriptor", argp);
//			uint64_t id = veo_call_async(peer.veo_main_context, sym, argp);
//			assert(id != VEO_REQUEST_ID_INVALID);
//			uint64_t res_nd_addr = 0;
//			err = veo_call_wait_result(peer.veo_main_context, id, &res_nd_addr);
//			errno_handler(err, "veo_call_wait_result");
//			veo_args_free(argp);
			// read remote node descriptor
			node_descriptor target_nd;
			err = veo_read_mem(peer.veo_proc, &target_nd, res_nd_addr, sizeof(node_descriptor));
			//err = veo_read_mem(peer.veo_proc, &target_nd, mem_addr, sizeof(node_descriptor));
			errno_handler(err, "veo_read_mem");
			

// TODO: error checks and output
//		HAM_DEBUG( HAM_LOG << "info: veo_get_sym() returned sym: " << (void*)veo_main_sym_ << std::endl; );
		HAM_DEBUG( HAM_LOG << "info: got target node_descriptor " << sizeof(node_descriptor) << ": '" << target_nd.name() << "'" << std::endl; );
		}
std::cerr << "got node desc" << std::endl;
*/		

				// start the target runtime
				uint64_t veo_main_sym = veo_get_sym(peer.veo_proc, peer.veo_lib_handle, "_Z8lib_mainiPPc"); // TODO: fix mangled name issue
				HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator: info: veo_get_sym() returned sym: " << (void*)veo_main_sym << std::endl; );

				// TODO: set up arguments (if needed) and do an async call
				peer.veo_main_args = veo_args_alloc();
				veo_args_set_stack(peer.veo_main_args, VEO_INTENT_IN, 0, (char *)comm_options.argc_ptr(), sizeof(*comm_options.argc_ptr()));
				nullptr_t target_argv = nullptr;
				// TODO: we set argv to nullptr until its needed, for deepcopying argv see: https://stackoverflow.com/questions/36804759/how-to-copy-argv-to-a-new-variable
				veo_args_set_stack(peer.veo_main_args, VEO_INTENT_IN, 1, (char *)&target_argv, sizeof(nullptr_t)); 
				
				// call target main asynchronously
				peer.veo_main_id = veo_call_async(peer.veo_main_context, veo_main_sym, peer.veo_main_args);
				HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator: info: veo_call_async() returned ID: " << peer.veo_main_id << std::endl; );

				// NOTE: target main is running now and can receive messages

				++ve_list_index;
			} // if
		} // for i

		HAM_DEBUG( HAM_LOG << "communicator(VH)::communicator: end." << std::endl; )
	}

	~communicator()
	{
		HAM_DEBUG( HAM_LOG << "communicator(VH)::~communicator()" << std::endl; )

		for (node_t i = 0; i < static_cast<node_t>(ham_process_count); ++i) {
			if(i != ham_host_address) // ommit the host
			{
				veo_peer& peer = peers[i];
				int err = 0;
							
				// sync on offloaded main()
				uint64_t retval;
				err = veo_call_wait_result(peer.veo_main_context, peer.veo_main_id, &retval);
				if (err < 0)
					HAM_LOG << "communicator(VH)::~communicator(): error: veo_call_wait_result() for synchronising on target main() failed." << std::endl;
				HAM_DEBUG( HAM_LOG << "communicator(VH)::~communicator(): info: veo_call_wait_result() returned : " << err << ", target main returned " << retval << std::endl; );
			
				// free VEO resources
				veo_args_free(peer.veo_main_args);

				err = veo_free_mem(peer.veo_proc, peer.local_buffers_addr);
				assert(err == 0);
				err = veo_free_mem(peer.veo_proc, peer.local_flags_addr);
				assert(err == 0);
				err = veo_free_mem(peer.veo_proc, peer.remote_buffers_addr);
				assert(err == 0);
				err = veo_free_mem(peer.veo_proc, peer.remote_flags_addr);
				assert(err == 0);


				err = veo_context_close(peer.veo_main_context);
				assert(err == 0);
				err = veo_proc_destroy(peer.veo_proc);
				assert(err == 0);
				HAM_UNUSED_VAR(err); // avoid warnings in non-debug builds
			} // if
		} // for i

		delete [] peers;
	}

protected:
	// pre-allocates the next request and modifies remote_node's internal peer data
	const request& allocate_next_request(node_t remote_node)
	{
		HAM_DEBUG( HAM_LOG << "communicator(VH)::allocate_next_request(): remote_node = " << remote_node << std::endl; )

		const size_t remote_buffer_index = peers[remote_node].remote_buffer_pool.allocate();
		const size_t local_buffer_index = peers[remote_node].local_buffer_pool.allocate();

		{
			HAM_DEBUG(
			request& req = peers[remote_node].next_request;
			HAM_LOG << "communicator(VH)::allocate_next_request(): old next_request = " <<
				"request(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl );
		}

		peers[remote_node].next_request = { remote_node, remote_buffer_index, ham_address, local_buffer_index };

		{
			HAM_DEBUG(
			request& req = peers[remote_node].next_request;
			HAM_LOG << "communicator(VH)::allocate_next_request(): new next_request = " <<
				"request(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl );
		}

		return peers[remote_node].next_request;
	}

public:
	request allocate_request(node_t remote_node)
	{
		{
			HAM_DEBUG(
			HAM_LOG << "communicator(VH)::allocate_request(): remote_node = " << remote_node << std::endl;
			request& req = peers[remote_node].next_request;
			HAM_LOG << "communicator(VH)::allocate_request(): returning next_request = " <<
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
		size_t flag_value = FLAG_FALSE;
		uint64_t local_flag_addr = peer.local_flags_addr + sizeof(cache_line_buffer) * req.target_buffer_index; // used by send
		uint64_t remote_flag_addr = peer.remote_flags_addr + sizeof(cache_line_buffer) * req.source_buffer_index; // used by recv

		errno_handler(
			veo_write_mem(peer.veo_proc, local_flag_addr, (const void*)&flag_value, sizeof(size_t)), 
			"veo_write_mem(local_flag) inside free_request()"
		);
		errno_handler(
			veo_write_mem(peer.veo_proc, remote_flag_addr, (const void*)&flag_value, sizeof(size_t)), 
			"veo_write_mem(remote_flag) inside free_request()"
		);
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

		HAM_DEBUG( HAM_LOG << "communicator(VH)::send_msg(): " <<
			"request(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl );

		send_msg(req.target_node, req.target_buffer_index, next_req.target_buffer_index, msg, size);
		//send_msg(req.target_node, req.target_buffer_index, peers[req.target_node].remote_buffer_pool.next(), msg, size);
	}

protected:
	// NOTE: we write into the target memory here via VEO, coming from the SCIF names
	//       local_buffers on the target are for receiving on the target
	//       remote_buffers on the target are for sending from the target
	//       => send_msg writes into local_buffers on the host side
	void send_msg(node_t node, size_t buffer_index, size_t next_buffer_index, void* msg, size_t size)
	{
		HAM_DEBUG( HAM_LOG << "communicator(VH)::send_msg(): node =  " << node << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VH)::send_msg(): remote buffer index = " << buffer_index << std::endl; )

		uint64_t target_buffer_addr = peers[node].local_buffers_addr + sizeof(msg_buffer) * buffer_index;
		uint64_t target_flag_addr = peers[node].local_flags_addr + sizeof(cache_line_buffer) * buffer_index;
		
		HAM_DEBUG( HAM_LOG << "communicator(VH)::send_msg(): sending message size: " << size << std::endl; )
		// copy to remote memory
//		memcpy((char*)remote_buffer, (void*)&size, sizeof(size_t)); // size = header
		// TODO: generalise and put into helper function buffer_index to offset
		errno_handler(
			veo_write_mem(peers[node].veo_proc, target_buffer_addr, (const void*)&size, sizeof(size_t)), 
			"veo_write_mem(size) inside send_msg()"
		);

		HAM_DEBUG( HAM_LOG << "communicator(VH)::send_msg(): sending message" << std::endl; )
		errno_handler(
			veo_write_mem(peers[node].veo_proc, target_buffer_addr + sizeof(size_t), (const void*)msg, size), 
			"veo_write_mem(msg) inside send_msg()"
		);
//		_mm_sfence(); // NOTE: intel intrinsic: store fence, make changes visible on the remote site to which we wrote

		HAM_DEBUG( HAM_LOG << "communicator(VH)::send_msg(): setting flag at " << target_flag_addr << " to next_buffer_index = " << next_buffer_index << std::endl; )
		errno_handler(
			veo_write_mem(peers[node].veo_proc, target_flag_addr, (const void*)&next_buffer_index, sizeof(size_t)), 
			"veo_write_mem(flag) inside send_msg()"
		);
//		_mm_sfence(); // NOTE: intel intrinsic: store fence, make changes visible on the remote site
	}

	// NOTE: we read from the target memory here via VEO 
	// we read flag, size, and msg from the targets "remote_buffer"
	void* recv_msg(node_t node, size_t buffer_index = NO_BUFFER_INDEX, void* msg = nullptr, size_t size = constants::MSG_SIZE)
	{
		HAM_UNUSED_VAR(msg);
		// use next_flag as index, if none is given
		buffer_index = buffer_index == NO_BUFFER_INDEX ?  peers[node].next_flag : buffer_index;
		HAM_DEBUG( HAM_LOG << "communicator(VH)::recv_msg(): remote node is: " << node << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator(VH)::recv_msg(): using buffer index: " << buffer_index << std::endl; )

		uint64_t target_buffer_addr = peers[node].remote_buffers_addr + sizeof(msg_buffer) * buffer_index;
		uint64_t target_flag_addr = peers[node].remote_flags_addr + sizeof(cache_line_buffer) * buffer_index;

		size_t local_flag = FLAG_FALSE;
		HAM_DEBUG( HAM_LOG << "communicator(VH)::recv_msg(): FLAG before polling: " << local_flag << std::endl; )
		while (local_flag == FLAG_FALSE) { // poll on flag
			// read flag	
			errno_handler(
				veo_read_mem(peers[node].veo_proc, (void*)&local_flag, target_flag_addr, sizeof(size_t)), 
				"veo_read_mem(flag) inside recv_msg()"
			);
		} // while
		HAM_DEBUG( HAM_LOG << "communicator(VH)::recv_msg(): FLAG after polling: " << local_flag << std::endl; )

		if (local_flag != NO_BUFFER_INDEX) // the flag contains the next buffer index to poll on
			peers[node].next_flag = local_flag;

		// copy size from recv buffer
		//memcpy((void*)&size, (char*)local_buffer, sizeof(size_t));
		errno_handler(
			veo_read_mem(peers[node].veo_proc, (void*)&size, target_buffer_addr, sizeof(size_t)), 
			"veo_read_mem(size) inside recv_msg()"
		);
	
		HAM_DEBUG( HAM_LOG << "communicator(VH)::recv_msg(): received msg size: " << size << std::endl; )
		//HAM_DEBUG( HAM_LOG << "receiving message of size: " << size << std::endl; )

//		_mm_lfence(); // NOTE: intel intrinsic: all prior loads are globally visible

		// buffer to read our msg to (without the size)
		char* recv_buffer = reinterpret_cast<char*>(&peers[node].recv_buffers.get()[buffer_index]);
		// TODO: put the message somewhere, a buffer is needed
		errno_handler(
			veo_read_mem(peers[node].veo_proc, (void*)recv_buffer, target_buffer_addr + sizeof(size_t), size), 
			"veo_read_mem(msg) inside recv_msg()"
		);

//		return (char*)local_buffer + sizeof(size_t); // we directly return our buffer here, which is safe, since it can only be re-used after being freed by the future which returns the result by value to the user
		return recv_buffer; // we directly return our buffer here, which is safe, since it can only be re-used after being freed by the future which returns the result by value to the user, at free_request() here
	}

	// works with same flags and buffers as recv_msg
	bool test_local_flag(node_t node, size_t buffer_index)
	{
		uint64_t target_flag_addr = peers[node].remote_flags_addr + sizeof(cache_line_buffer) * buffer_index;

		size_t flag = FLAG_FALSE;
		// read flag	
		errno_handler(
			veo_read_mem(peers[node].veo_proc, (void*)&flag, target_flag_addr, sizeof(size_t)), 
			"veo_read_mem(flag) inside test_local_flag()"
		);

		return flag != FLAG_FALSE;
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
		HAM_DEBUG( HAM_LOG << "communicator(VH)::send_data(): writing " << size * sizeof(T) << " byte from " << local_source << " to " << remote_dest.get() << std::endl; )
		errno_handler(
			veo_write_mem(peers[remote_dest.node()].veo_proc, reinterpret_cast<uint64_t>(remote_dest.get()), (const void*)local_source, size * sizeof(T)), 
			"veo_write_mem(data) inside send_data()"
		);
	}

	template<typename T>
	void recv_data(buffer_ptr<T>& remote_source, T* local_dest, size_t size)
	{
		HAM_DEBUG( HAM_LOG << "communicator(VH)::recv_data(): reading " << size * sizeof(T) << " byte from " << remote_source.get() << " to " << local_dest << std::endl; )
		errno_handler(
			veo_read_mem(peers[remote_source.node()].veo_proc, (void*)local_dest, reinterpret_cast<uint64_t>(remote_source.get()), size * sizeof(T)), 
			"veo_read_mem(msg) inside recv_msg()"
		);
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
	// command line options
	uint64_t ham_process_count = 2; // number of participating processes
	node_t ham_host_address = 0; // the address of the host process
	node_t ham_address = 0; // this process' address
	std::string veo_library_path = "";
	std::string veo_ve_nodes = "";

	// pointers to arrays of buffers, index is peer address
	struct veo_peer {
		veo_peer() : next_flag(0) //, remote_buffer_pool(constants::MSG_BUFFERS), remote_flag_pool(constants::MSG_BUFFERS)
		{}

		request next_request; // the next request, belonging to next flag
		size_t next_flag; // flag

		// VEO data
		struct veo_proc_handle* veo_proc = nullptr;
		uint64_t veo_lib_handle;
		struct veo_thr_ctxt* veo_main_context;
		// the async VEO offload of our main()
		uint64_t veo_main_id;
		struct veo_args* veo_main_args;

		// addresses of remote communication buffer
		uint64_t local_buffers_addr = 0; // for sending to target
		uint64_t local_flags_addr = 0;

		uint64_t remote_buffers_addr = 0; // for receiving from target
		uint64_t remote_flags_addr = 0;

		// buffer to copy received messages from target to
		buffer_ptr<msg_buffer> recv_buffers; // local memory, recv_msg copies data to these buffers using veo_read_mem()

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
};

} // namespace net
} // namespace ham

// definitions that need a complete type for communicator
#include "ham/net/communicator_veo_only_post.hpp"

#endif // ham_net_communicator_veo_only_vh_hpp
