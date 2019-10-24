// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_net_communicator_scif_hpp
#define ham_net_communicator_scif_hpp

#include <cassert>
#include <errno.h>
#include <stdlib.h> // posix_memalign
#include <string.h> // errno messages
#include <unistd.h>

#include <boost/program_options.hpp>
#include <scif.h>

#include "ham/misc/constants.hpp"
#include "ham/misc/options.hpp"
#include "ham/misc/resource_pool.hpp"
#include "ham/misc/types.hpp"
#include "ham/util/debug.hpp"
#include "ham/util/log.hpp"

//#include <mmintrin.h>

#ifndef HAM_SCIF_RMA_CPU_THRESHOLD
#define HAM_SCIF_RMA_CPU_THRESHOLD 64
#endif

namespace ham {
namespace net {

template<typename T>
class buffer_ptr
{
public:
	buffer_ptr();
	buffer_ptr(T* ptr, node_t node, off_t offset, size_t registered_size) : ptr_(ptr), node_(node), offset_(offset), registered_size_(registered_size) { }

	T* get() const { return ptr_; }
	off_t offset() const { return offset_; }
	size_t registered_size() const { return registered_size_; }
	node_t node() const { return node_; }

	// TODO(improvement): too dangerous?
	//operator T*() { return ptr; }
	//operator off_t() { return offset; }

	// element access
	T& operator[](size_t i);

	// basic pointer arithmetic to address sub-buffers
	buffer_ptr<T> operator+(size_t off)
	{
		size_t byte = off * sizeof(T);
		return buffer_ptr(ptr_ + off, node_, offset_ + byte, registered_size_ - byte);
	}

private:
	T* ptr_;
	node_t node_;
	off_t offset_; // SCIF offset inside registered window
	size_t registered_size_; // SCIF window registered size, >= actual buffer
};

class node_descriptor
{
public:
	const char* name() const { return name_; }
	
	// TODO(feature): add properties: 
	//    - node type (HOST | MIC | ...)
	//    - socket (to which the mic is connected)
private:
	static constexpr size_t name_length_ {256};
	char name_[name_length_];

	friend class communicator;
};

class communicator {
public:
	enum {
		MAX_SCIF_NODES = 17, // needed for initialisation, there shouldn't be more than 9 scif nodes in a single system (host + phis)
		BASE_PORT = 1024, // ports below 1024 are reserved
		NO_BUFFER_INDEX = constants::MSG_BUFFERS, // invalid buffer index (max valid + 1)
		FLAG_FALSE = constants::MSG_BUFFERS + 1 // special value, outside normal index range
	};

	// externally used interface of request must be shared across all communicator-implementations
	struct request {

		request() : target_buffer_index(NO_BUFFER_INDEX) {} // instantiate invalid
		request(node_t target_node, size_t target_buffer_index, node_t source_node, size_t source_buffer_index)
		 : target_node(target_node), target_buffer_index(target_buffer_index), source_node(source_node), source_buffer_index(source_buffer_index)
		{}

		bool test() const
		{
			return communicator::instance().test_local_flag(target_node, source_buffer_index);
		}

		void* get() const // blocks
		{
			return communicator::instance().recv_msg(target_node, source_buffer_index); // we wait for the remote side, to write into our buffer/flag
		}


		template<class T>
		void send_result(T* result_msg, size_t size)
		{
			assert(communicator::this_node() == target_node); // this assert fails if send_result is called from the wrong side
			communicator::instance().send_msg(source_node, source_buffer_index, NO_BUFFER_INDEX, result_msg, size);
		}

		bool valid() const
		{
			return target_buffer_index != NO_BUFFER_INDEX;
		}

		node_t target_node;
		size_t target_buffer_index; // for sending to target node
		node_t source_node;
		size_t source_buffer_index; // for receiving from target node
	};

	typedef request& request_reference_type;
	typedef const request& request_const_reference_type;

	communicator(int argc, char* argv[])
	{
		instance_ = this;

		HAM_DEBUG( HAM_LOG << "FLAG_FALSE = " << FLAG_FALSE << ", NO_BUFFER_INDEX = " << NO_BUFFER_INDEX << std::endl; )

		// basic SCIF initialisation
		uint16_t nodes[MAX_SCIF_NODES];
		uint16_t self;
		int err = 0;
		num_nodes_ = scif_get_nodeIDs(nodes, MAX_SCIF_NODES, &self);
		errno_handler(num_nodes_, "scif_get_nodeIDs");

		// command line configuration
		ham_process_count = num_nodes_; // number of participating processes
		ham_address = self; // this processes' address
		ham_host_address = 0; // the address of the host process
		ham_host_node = 0; // SCIF network node of the host process, default is 0 (which is the host machines)

		// command line options
		boost::program_options::options_description desc("HAM Options");
		desc.add_options()
			("ham-help", "Shows this message")
			("ham-process-count", boost::program_options::value(&ham_process_count)->default_value(ham_process_count), "Number of processes the job consists of.")
			("ham-address", boost::program_options::value(&ham_address)->default_value(ham_address), "This processes address, between 0 and host-process-count minus 1.")
			("ham-host-node", boost::program_options::value(&ham_host_node)->default_value(ham_host_node), "The SCIF network node on which the host process is started (0, the host machine by default).")
			("ham-host-address", boost::program_options::value(&ham_host_address)->default_value(ham_host_address), "The address of the host process (0 by default). NOTE: Not implemented yet.")
		;

		boost::program_options::variables_map vm;

		// NOTE: no try-catch here to avoid exceptions, that cause offload-dependencies to boost exception in the MIC code
		const char* options_env = std::getenv("HAM_OPTIONS");
		if (options_env)
		{
			char split_character = ' ';
			if (std::getenv("HAM_OPTIONS_NO_SPACES")) // value does not matter
				split_character = '_';

			// parse from environment
			boost::program_options::store(boost::program_options::command_line_parser(detail::options::split(std::string(options_env), split_character)).options(desc).allow_unregistered().run(), vm);
		}
		else
		{
			// parse from command line
			boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), vm);
		}

		boost::program_options::notify(vm);

		if(vm.count("ham-help"))
		{
			std::cout << desc << std::endl;
			exit(0);
		}

		// SCIF setup

		// allocate peer data structures
		peers = new scif_peer[ham_process_count];

		// get own hostname
		errno_handler(gethostname(peers[ham_address].node_description.name_, peers[ham_address].node_description.name_length_), "gethostname");

		if (is_host())
		{
			HAM_DEBUG( HAM_LOG << "num_nodes_: " << num_nodes_ << std::endl; )
			HAM_DEBUG( for (int i = 0; i < num_nodes_; ++i) HAM_LOG << "node_id " << i << ": " << nodes[i] << std::endl; )

			// open endpoint to list on (own peer)
			peers[ham_address].endpoint = scif_open();
			errno_handler((int)peers[ham_address].endpoint, "scif_open");
			errno_handler(peers[ham_address].endpoint == ((scif_epd_t)-1) ? -1 : 0, "scif_open");
			// bind to port = BASE_PORT + ham_address
			err = scif_bind(peers[ham_address].endpoint, BASE_PORT + ham_address);
			errno_handler(err, "scif_bind");


			scif_listen(peers[ham_address].endpoint, ham_process_count - 1);
			errno_handler(err, "scif_listen");

			// accept connections from ham_process_count peers
			for (int i = 1; i < ham_process_count; ++i) // (ham_process count - 1) iterations
			{
				// accept a SCIF connection from a peer
				scif_portID portID;
				scif_epd_t tmp_endpoint;
				err = scif_accept(peers[ham_address].endpoint, &portID, &tmp_endpoint, SCIF_ACCEPT_SYNC);
				errno_handler(err, "scif_accept");
				
				int ep_index = portID.port - BASE_PORT; // endpoint, or peer, index based on the port
				// the current peer for which we build up the data strucutre
				scif_peer& peer = peers[ep_index];
				peer.endpoint = tmp_endpoint;

				HAM_DEBUG( HAM_LOG << "communicator::communicator(): accepted connection from SCIF-node: " << portID.node << ", on port: " << portID.port << ", HAM address: " << ep_index << std::endl; )

				// allocate local buffers used for the current peer (registered to the corresponding endpoint)
				peer.local_buffers = allocate_buffer<msg_buffer>(constants::MSG_BUFFERS, ep_index);
				peer.local_flags = allocate_buffer<cache_line_buffer>(constants::MSG_BUFFERS, ep_index);
				reset_flags(peer.local_flags);

				// receive remote buffers from current peer
				err = scif_recv(peer.endpoint, (void*)&peer.remote_buffers, sizeof(buffer_ptr<msg_buffer>), SCIF_RECV_BLOCK);
				errno_handler(err, "scif_recv remote_buffers");
				err = scif_recv(peer.endpoint, (void*)&peer.remote_flags, sizeof(buffer_ptr<cache_line_buffer>), SCIF_RECV_BLOCK);
				errno_handler(err, "scif_recv remote_flags");

				// send local buffers to current peer
				err = scif_send(peer.endpoint, (void*)&peer.local_buffers, sizeof(buffer_ptr<msg_buffer>), SCIF_SEND_BLOCK);
				errno_handler(err, "scif_send local_buffers");
				err = scif_send(peer.endpoint, (void*)&peer.local_flags, sizeof(buffer_ptr<cache_line_buffer>), SCIF_SEND_BLOCK);
				errno_handler(err, "scif_send local_flags");

				// map remote buffers
				peer.mapped_remote_buffers = static_cast<msg_buffer*>(scif_mmap(0, peer.remote_buffers.registered_size(), SCIF_PROT_WRITE | SCIF_PROT_READ, 0, peer.endpoint, peer.remote_buffers.offset()));
				errno_handler(SCIF_MMAP_FAILED == peer.mapped_remote_buffers ? -1 : 0, "scif_mmap buffers");
				peer.mapped_remote_flags = static_cast<cache_line_buffer*>(scif_mmap(0, peer.remote_flags.registered_size(), SCIF_PROT_WRITE | SCIF_PROT_READ, 0, peer.endpoint, peer.remote_flags.offset()));
				errno_handler(SCIF_MMAP_FAILED == peer.mapped_remote_flags ? -1 : 0, "scif_mmap flags");

				// fill resource pools
				for (size_t j = constants::MSG_BUFFERS; j > 0; --j) {
					peer.remote_buffer_pool.add(j-1);
					peer.local_buffer_pool.add(j-1);
				}

				// allocate the first request to be used for the next send
				allocate_next_request(ep_index);


				// receive current node_descriptor
				err = scif_recv(peer.endpoint, (void*)&peer.node_description, sizeof(peer.node_description), SCIF_RECV_BLOCK);
				errno_handler(err, "scif_recv remote_flags");
				// send the host's node_descriptor
				err = scif_send(peer.endpoint, (void*)&peers[ham_host_address].node_description, sizeof(peers[ham_host_address].node_description), SCIF_SEND_BLOCK);
				errno_handler(err, "scif_send node_description");
			}
		}
		else // offload target
		{
			scif_peer& host_peer = peers[ham_host_address];

			// bind endpoint to the host
			host_peer.endpoint = scif_open();
			errno_handler((int)host_peer.endpoint, "scif_open");
			errno_handler(host_peer.endpoint == ((scif_epd_t)-1) ? -1 : 0, "scif_open");
			// bind to port = BASE_PORT + ham_address
			err = scif_bind(host_peer.endpoint, BASE_PORT + ham_address);
			errno_handler(err, "scif_bind");

			// only open one end-point to connect with the host
			scif_portID portID = {nodes[ham_host_node], static_cast<uint16_t>(BASE_PORT + ham_host_address)};
			err = scif_connect(host_peer.endpoint, &portID); // connect with host
			errno_handler(err, "scif_accept");

			// allocate actual receive buffers (on host and offload target)
			host_peer.local_buffers = allocate_buffer<msg_buffer>(constants::MSG_BUFFERS, ham_host_address);
			host_peer.local_flags = allocate_buffer<cache_line_buffer>(constants::MSG_BUFFERS, ham_host_address);
			//memset(host_peer.local_flags.get(), FLAG_FALSE, constants::MSG_BUFFERS * sizeof(cache_line_buffer));
			reset_flags(host_peer.local_flags);


			// send addresses to host for mapping
			err = scif_send(host_peer.endpoint, (void*)&host_peer.local_buffers, sizeof(buffer_ptr<msg_buffer>), SCIF_SEND_BLOCK);
			errno_handler(err, "scif_send local_buffers");
			err = scif_send(host_peer.endpoint, (void*)&host_peer.local_flags, sizeof(buffer_ptr<cache_line_buffer>), SCIF_SEND_BLOCK);
			errno_handler(err, "scif_send local_flags");

			// receive remote buffers from host
			err = scif_recv(host_peer.endpoint, (void*)&host_peer.remote_buffers, sizeof(buffer_ptr<msg_buffer>), SCIF_RECV_BLOCK);
			errno_handler(err, "scif_recv remote_buffers");
			err = scif_recv(host_peer.endpoint, (void*)&host_peer.remote_flags, sizeof(buffer_ptr<cache_line_buffer>), SCIF_RECV_BLOCK);
			errno_handler(err, "scif_recv remote_flags");

			// map remote buffers
			host_peer.mapped_remote_buffers = static_cast<msg_buffer*>(scif_mmap(0, host_peer.remote_buffers.registered_size(), SCIF_PROT_WRITE | SCIF_PROT_READ, 0, host_peer.endpoint, host_peer.remote_buffers.offset()));
			errno_handler(SCIF_MMAP_FAILED == host_peer.mapped_remote_buffers ? -1 : 0, "scif_mmap buffers");
			host_peer.mapped_remote_flags = static_cast<cache_line_buffer*>(scif_mmap(0, host_peer.remote_flags.registered_size(), SCIF_PROT_WRITE | SCIF_PROT_READ, 0, host_peer.endpoint, host_peer.remote_flags.offset()));
			errno_handler(SCIF_MMAP_FAILED == host_peer.mapped_remote_flags ? -1 : 0, "scif_mmap flags");

			// send own node_descriptor to host
			err = scif_send(host_peer.endpoint, (void*)&peers[ham_address].node_description, sizeof(peers[ham_address].node_description), SCIF_SEND_BLOCK);
			errno_handler(err, "scif_send node_description");
			// receive the host's node_descriptor
			err = scif_recv(host_peer.endpoint, (void*)&host_peer.node_description, sizeof(host_peer.node_description), SCIF_RECV_BLOCK);
			errno_handler(err, "scif_recv remote_flags");
		}
	}

	~communicator()
	{
		// SCIF guarantees to free everything when the process is finished
		HAM_DEBUG( HAM_LOG << "~communicator" << std::endl; )

		if (is_host())
		{
			for (int i = 0; i < ham_process_count; ++i)
			{
				if(i != ham_host_address) // ommit the host
				{
					scif_peer& peer = peers[i];

					// free local memory
					free_buffer(peer.local_buffers);
					free_buffer(peer.local_flags);
					// unmap remote memory
					int err;
					err = scif_munmap(peer.mapped_remote_buffers, peer.remote_buffers.registered_size());
					errno_handler(err, "scif_munmap buffers");
					err = scif_munmap(peer.mapped_remote_flags, peer.remote_flags.registered_size());
					errno_handler(err, "scif_munmap flags");
				}
			}
		}
		else
		{
			scif_peer& host_peer = peers[ham_host_address];
			// free local memory
			free_buffer(host_peer.local_buffers);
			free_buffer(host_peer.local_flags);
			// unmap remote memory
			int err;
			err = scif_munmap(host_peer.mapped_remote_buffers, host_peer.remote_buffers.registered_size());
			errno_handler(err, "scif_munmap buffers");
			err = scif_munmap(host_peer.mapped_remote_flags, host_peer.remote_flags.registered_size());
			errno_handler(err, "scif_munmap flags");
		}

		// delete
		delete [] peers;
	}

private:
	// pre-allocates the next request and modifies remote_node's internal peer data
	const request& allocate_next_request(node_t remote_node)
	{
		HAM_DEBUG( HAM_LOG << "communicator::allocate_next_request(): remote_node = " << remote_node << std::endl; )

		const size_t remote_buffer_index = peers[remote_node].remote_buffer_pool.allocate();
		const size_t local_buffer_index = peers[remote_node].local_buffer_pool.allocate();

		{
			HAM_DEBUG(
			request& req = peers[remote_node].next_request;
			HAM_LOG << "communicator::allocate_next_request(): old next_request = " <<
				"request(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl );
		}

		peers[remote_node].next_request = { remote_node, remote_buffer_index, ham_address, local_buffer_index };

		{
			HAM_DEBUG(
			request& req = peers[remote_node].next_request;
			HAM_LOG << "communicator::allocate_next_request(): new next_request = " <<
				"req(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl );
		}

		return peers[remote_node].next_request;
	}

public:
	request allocate_request(node_t remote_node)
	{
		{
			HAM_DEBUG(
			HAM_LOG << "communicator::allocate_request(): remote_node = " << remote_node << std::endl;
			request& req = peers[remote_node].next_request;
			HAM_LOG << "communicator::allocate_request(): returning next_request = " <<
				"request(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl );
		}
		// there is always one pre_allocated request, that corresponds to the next buffer index written to the receiver in the last send
		return peers[remote_node].next_request;
	}

	void free_request(request_reference_type req)
	{
		assert(req.source_node == ham_address);

		scif_peer& peer = peers[req.target_node];

		// set flags
		volatile size_t* local_flag = reinterpret_cast<size_t*>(&peer.local_flags.get()[req.source_buffer_index]);
		volatile size_t* remote_flag = reinterpret_cast<size_t*>(&peer.mapped_remote_flags[req.target_buffer_index]);
		*local_flag = FLAG_FALSE;
		*remote_flag = FLAG_FALSE;
		_mm_sfence(); // make preceding stores globally visible

		// pool indices
		peer.remote_buffer_pool.free(req.target_buffer_index);
		peer.local_buffer_pool.free(req.source_buffer_index);

		// invalidate request
		req.target_buffer_index = NO_BUFFER_INDEX;
	}

	void send_msg(request_reference_type req, void* msg, size_t size)
	{
		const request& next_req = allocate_next_request(req.target_node); // pre-allocate-request for the next send, because we set this index on the remote size

		HAM_DEBUG( HAM_LOG << "communicator::send_msg(): " <<
			"request(" << req.target_node << ", " << req.target_buffer_index << ", " << req.source_node << ", " << req.source_buffer_index << ")" << std::endl );

		send_msg(req.target_node, req.target_buffer_index, next_req.target_buffer_index, msg, size);
		//send_msg(req.target_node, req.target_buffer_index, peers[req.target_node].remote_buffer_pool.next(), msg, size);
	}

private:
	void send_msg(node_t node, size_t buffer_index, size_t next_buffer_index, void* msg, size_t size)
	{
		HAM_DEBUG( HAM_LOG << "communicator::send_msg(): node =  " << node << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator::send_msg(): remote buffer index = " << buffer_index << std::endl; )

		char* mapped_remote_buffer = reinterpret_cast<char*>(&peers[node].mapped_remote_buffers[buffer_index]);
		volatile size_t* mapped_remote_flag = reinterpret_cast<size_t*>(&peers[node].mapped_remote_flags[buffer_index]);
		HAM_DEBUG( HAM_LOG << "communicator::send_msg(): mapped remote buffer is: " << (void*)mapped_remote_buffer << std::endl; )

		HAM_DEBUG( HAM_LOG << "communicator::send_msg(): sending message of size: " << size << std::endl; )
		// copy to remote memory
		memcpy((char*)mapped_remote_buffer, (void*)&size, sizeof(size_t)); // size = header

		memcpy((char*)mapped_remote_buffer + sizeof(size_t), msg , size);
		_mm_sfence(); // NOTE: intel intrinsic: store fence, make changes visible on the remote site to which we wrote

		*mapped_remote_flag = next_buffer_index; // signal remote side that the message has been written, and transfer the next buffer/flag index in the process
		_mm_sfence(); // NOTE: intel intrinsic: store fence, make changes visible on the remote site
	}

private:
	void* recv_msg(node_t node, size_t buffer_index = NO_BUFFER_INDEX, void* msg = nullptr, size_t size = constants::MSG_SIZE)
	{
		// use next_flag as index, if none is given
		buffer_index = buffer_index == NO_BUFFER_INDEX ?  peers[node].next_flag : buffer_index;
		HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): remote node is: " << node << std::endl; )
		HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): using buffer index: " << buffer_index << std::endl; )

		char* local_buffer = reinterpret_cast<char*>(&peers[node].local_buffers.get()[buffer_index]);
		volatile size_t* local_flag = reinterpret_cast<size_t*>(&peers[node].local_flags.get()[buffer_index]);
		HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): local buffer is: " << (void*)local_buffer << std::endl; )

		HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): FLAG before polling: " << (int)*local_flag << std::endl; )
		while (*local_flag == FLAG_FALSE); // poll on flag
		HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): FLAG after polling: " << (int)*local_flag << std::endl; )

		if (*local_flag != NO_BUFFER_INDEX) // the flag contains the next buffer index to poll on
			peers[node].next_flag = *local_flag;

		// copy size from recv buffer
		memcpy((void*)&size, (char*)local_buffer, sizeof(size_t));

		HAM_DEBUG( HAM_LOG << "communicator::recv_msg(): received msg size: " << size << std::endl; )
		//HAM_DEBUG( HAM_LOG << "receiving message of size: " << size << std::endl; )

		_mm_lfence(); // NOTE: intel intrinsic: all prior loads are globally visible
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
		// nothing todo here, since this communicator implementation uses one-sided communication
		// the data is already where it is expected (in the buffer referenced in req)
		return;
	}


	template<typename T>
	void send_data(T* local_source, buffer_ptr<T>& remote_dest, size_t size)
	{
		//scif_send(peers[node].endpoint, local_source, size, SCIF_SEND_BLOCK);
		HAM_DEBUG( HAM_LOG << "communicator::send_data(): writing " << size << " byte from " << local_source << " to " << remote_dest.offset() << std::endl; )
		int err =
		scif_vwriteto(peers[remote_dest.node()].endpoint, (void*)local_source, size * sizeof(T), remote_dest.offset(), SCIF_RMA_SYNC |
#ifdef HAM_SCIF_RMA_CPU
				(((size * sizeof(T)) < HAM_SCIF_RMA_CPU_THRESHOLD) ? SCIF_RMA_USECPU : 0)
#else
				0
#endif
				);

		// | SCIF_RMA_USECACHE); // TODO(low priority): => segfaulted?! => too many pinned pages?!
		HAM_DEBUG( errno_handler(err, "scif_vwriteto"); )
	}

	template<typename T>
	void recv_data(buffer_ptr<T>& remote_source, T* local_dest, size_t size)
	{
		//scif_recv(peers[node].endpoint, local_dest, size, SCIF_RECV_BLOCK);
		HAM_DEBUG( HAM_LOG << "communicator::recv_data(): reading " << size << " byte from " << remote_source.offset() << " to " << local_dest << std::endl; )
		int err =
		scif_vreadfrom(peers[remote_source.node()].endpoint, (void*)local_dest, size * sizeof(T), remote_source.offset(), SCIF_RMA_SYNC |
#ifdef HAM_SCIF_RMA_CPU
				(((size * sizeof(T)) < HAM_SCIF_RMA_CPU_THRESHOLD) ? SCIF_RMA_USECPU : 0)
#else
				0
#endif
				);
		HAM_DEBUG( errno_handler(err, "scif_vreadfrom"); )
	}

	template<typename T>
	buffer_ptr<T> allocate_buffer(const size_t n, node_t source_node)
	{
		// NOTE: no ctor calls (buffer vs. array)
		//ptr = new(ptr) T[n];
		T* ptr;
		size_t n_rma  = n * sizeof(T); // space to allocate on byte
		n_rma = n_rma % constants::PAGE_SIZE == 0 ? n_rma : ((n_rma / constants::PAGE_SIZE) + 1) * constants::PAGE_SIZE; // round to whole pages
		//int err =
		posix_memalign((void**)&ptr, constants::PAGE_SIZE, n_rma); // allocate page boundary aligned memory

		off_t offset = scif_register(peers[source_node].endpoint, ptr, n_rma, 0, SCIF_PROT_READ | SCIF_PROT_WRITE, 0);
		HAM_DEBUG( errno_handler(offset, "scif_register"); )
		return buffer_ptr<T>(ptr, ham_address, offset, n_rma);
	}

	template<typename T>
	void free_buffer(buffer_ptr<T> ptr)
	{
		assert(ptr.node() == ham_address);
		// NOTE: no ctor calls (buffer vs. array)
		//delete [] ptr;
		scif_unregister(peers[ptr.node()].endpoint, ptr.offset(), ptr.registered_size());
		free((void*)ptr.get());
	}

	static const node_descriptor& get_node_description(node_t node)
	{
		return instance().peers[node].node_description;
	}

	static communicator& instance() { return *instance_; }
	static node_t this_node() { return instance().ham_address; }
	static size_t num_nodes() { return instance().ham_process_count; }
	bool is_host() { return ham_address == ham_host_address ; }
	bool is_host(node_t node) { return node == ham_host_address; }

private:
	void errno_handler(int ret, const char * hint)
	{
		if (ret < 0)
		{
			char buffer[ 256 ];
			char * errorMessage = strerror_r( errno, buffer, 256 );
			HAM_LOG << "Errno(" << errno << ") Message for \"" << hint << "\": " << errorMessage << std::endl;
			//HAM_LOG << strerror_r(errno) << std::endl;
			exit(-1);
		}
	}

	void reset_flags(buffer_ptr<cache_line_buffer> flags)
	{
		cache_line_buffer fill_value;
		cache_line_buffer* fill_value_ptr = &fill_value;
		// null fill_value
		std::fill(reinterpret_cast<unsigned char*>(fill_value_ptr), reinterpret_cast<unsigned char*>(fill_value_ptr) + sizeof(cache_line_buffer), 0);
		// set to flag false
		*reinterpret_cast<size_t*>(fill_value_ptr) = FLAG_FALSE;
		// set all flags to fill_value
		std::fill(flags.get(), flags.get() + constants::MSG_BUFFERS, fill_value);
	}

	static communicator* instance_;

	uint16_t num_nodes_; // number of scif nodes
	uint16_t ham_process_count; // number of participating processes
	uint16_t ham_address; // this processes' address
	uint16_t ham_host_node; // SCIF network node of the host process, default is 0 (which is the host machines)
	uint16_t ham_host_address; // the address of the host process

	// pointers to arrays of buffers, index is peer address

	struct scif_peer {
		scif_peer() : next_flag(0) //, remote_buffer_pool(constants::MSG_BUFFERS), remote_flag_pool(constants::MSG_BUFFERS)
		{}

		request next_request; // the next request, belonging to next flag

		scif_epd_t endpoint; // endpoint to the connection with this peer

		// pointer to the peer's receive buffer
		buffer_ptr<msg_buffer> local_buffers; // local memory, remote peer writes to these buffers
		buffer_ptr<cache_line_buffer> local_flags; // local memory, remote peer signals writing is complete via these flags, I poll on this flag
		size_t next_flag = 0; // flag

		buffer_ptr<msg_buffer> remote_buffers; // remote memory, I write messages to this peer into these buffers
		buffer_ptr<cache_line_buffer> remote_flags; // remote memory, I write these flags to signal a message was sent

		msg_buffer* mapped_remote_buffers; // remote_buffers mapped into local memory
		cache_line_buffer* mapped_remote_flags; // remote_flags mapped into local memory

		// needed by sender to manage which buffers are in use and which are free
		// just manages indices, that can be used by
		// the sender to go into mapped_remote_buffers/flags
		// the receiver to go into local_buffers/flags
		detail::resource_pool<size_t> remote_buffer_pool;
		detail::resource_pool<size_t> local_buffer_pool;

		node_descriptor node_description;
	};

	scif_peer* peers;
};

template<typename T>
buffer_ptr<T>::buffer_ptr() : buffer_ptr(nullptr, net::communicator::this_node(), 0, 0) { }

template<typename T>
T& buffer_ptr<T>::operator[](size_t i)
{
	assert(node_ == net::communicator::this_node());
	return ptr_[i];
}

} // namespace net
} // namespace ham

#endif // ham_net_communicator_scif_hpp
