// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_net_communicator_tcp_hpp
#define ham_net_communicator_tcp_hpp

#include <cassert>
#include <cstring> // memcpy
#include <stdlib.h> // posix_memalign
#include <thread> // async thread
// #include <memory> // std::shared_ptr

#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include "ham/misc/options.hpp"
#include "ham/misc/constants.hpp"
#include "ham/misc/resource_pool.hpp"
#include "ham/misc/types.hpp"
#include "ham/util/debug.hpp"
#include "ham/util/log.hpp"

using boost::asio::ip::tcp;

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
	const char* name() const { return name_; }
private:
	//std::string name_; // TODO(improvement): unify node description for all back-ends, NOTE: std::string is not trivally transferable
	char name_[64]= "Node descriptions not available for TCP backend";

	friend class net::communicator;
};

class communicator { // : public std::enable_shared_from_this<communicator>
public:
	// externally used interface of request must be shared across all communicator-implementations
	class request {
	public:
		request() : valid_(false), received_(false), sent_(false) {} // instantiate invalid
		
		request(node_t target_node, node_t source_node, size_t send_buffer_index, size_t recv_buffer_index)
		 : target_node(target_node), source_node(source_node), valid_(true), sent_(false), received_(false), send_buffer_index(send_buffer_index), recv_buffer_index(recv_buffer_index), req_count(0)
		{}

		// return true if request was finished
		bool test()
		{
            // tcp backend does not feature asynchronous operations yet
            // HAM_DEBUG( HAM_LOG << "request::test(), TCP backend does not feature asynchronous operations" << std::endl; )

			// int flag = 0;
			// MPI_Testall(req_count, mpi_reqs, &flag, MPI_STATUS_IGNORE); // just test the receive request, since the send belonging to the request triggers the remote send that is received
			return received_;
		}

		void* get() // blocks
		{
			// tcp backend does not feature asynchronous operations yet
            // HAM_DEBUG( HAM_LOG << "request::get(), TCP backend does not feature asynchronous operations" << std::endl; )
            // HAM_DEBUG( HAM_LOG << "request::get(), before MPI_Waitall()" << std::endl; )
			// MPI_Waitall(req_count, mpi_reqs, MPI_STATUS_IGNORE); // must wait for all requests to satisfy the standard

			// block until async receive handler reports completion
			while(!received_);

			return static_cast<void*>(&communicator::instance().peers[target_node].msg_buffers[recv_buffer_index]);
		}

		template<class T>
		void send_result(T* result_msg, size_t size)
		{
			assert(communicator::this_node() == target_node); // this assert fails if send_result is called from the wrong side
			
			// TODO(improvement, low priority): better go through communicator, such that no MPI calls are anywhere else
			// MPI_Send(result_msg, size, MPI_BYTE, source_node, constants::RESULT_TAG, MPI_COMM_WORLD);



			communicator::instance().send_result(source_node, result_msg, size);
            // don't need size * sizeof(T) because req.send_result is called as send_result((void*)&a, sizeof(a)) in offload_msg.hpp
		}

		bool valid() const
		{
			return valid_;
		}

		bool received() const {
			return received_;
		}

		bool sent() const {
			return sent_;
		}

        void wait_sent() const {
            while(!sent_) {};
        }

        node_t target_node;
		node_t source_node;
		bool valid_;
		volatile bool received_; // used for the async receive handler to set to true, checked for completion
		volatile bool sent_; // used for the async send handler to set to true... unused, but the handler likes to do something

		// only needed by the sender
		enum { NUM_REQUESTS = 3 };
		
		size_t send_buffer_index; // buffer to use for sending the message
		size_t recv_buffer_index; // buffer to use for receiving the result
		size_t req_count;
		
	private:
		// not needed since tcp backend does not offer async operations
        // MPI_Request mpi_reqs[NUM_REQUESTS]; // for sending the msg, receiving the result, and an associated data transfer
	}; // class request
	
	typedef request& request_reference_type;
	typedef const request& request_const_reference_type;

	communicator(int argc, char* argv[]) : node_desc_dummy()
	{
		HAM_DEBUG( HAM_LOG << "communicator::communicator(): initialising configuration" << std::endl; )

		instance_ = this;

		// command line configuration
		nodes_ = 0;		// number of nodes
		this_node_ = 0;		// "rank" of this node
		this_port_ = 0;		// tcp port used for this node
		host_node_ = 0;		// host node
		host_address_ = "empty";		// host IP address or resolvable name
		host_port_ = "empty";		// host port


		// command line options
		boost::program_options::options_description desc("HAM Options");
		desc.add_options()
				("ham-help", "Shows this message")
				("ham-process-count", boost::program_options::value(&nodes_)->required(), "Required: Number of processes the job consists of.")
				("ham-address", boost::program_options::value(&this_node_)->required(), "Required: This processes UNIQUE address, between 0 and ham-process-count-1. 0 will make the process the host (required EXACTLY once). -1 will assign any free non-host rank.")
				("ham-tcp-port", boost::program_options::value(&this_port_)->default_value(this_port_), "TCP port used if this process is a client. Default will auto select an available port. Host will use ham-tcp-hostport and ignore this.")
				("ham-tcp-hostname", boost::program_options::value(&host_address_)->required(), "Required: IP address or resolvable hostname of the host process. Required. May be used on host to select interface.")
				("ham-tcp-hostport", boost::program_options::value(&host_port_)->required(), "Required: TCP port used by the host.")
				;

		boost::program_options::variables_map vm;

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

		HAM_DEBUG( HAM_LOG << "communicator::communicator(): command line config:" << std::endl
							 << "ham-process-count: " << nodes_ << std::endl
							 << "ham-address: " << this_node_ << std::endl
				   			 << "ham-tcp-port: " << this_port_ << std::endl
							 << "ham-tcp-hostname: " << host_address_ << std::endl
							 << "ham-tcp-hostport: " << host_port_ << std:: endl;
  		)


		HAM_DEBUG( HAM_LOG << "communicator::communicator(): initialising configuration done" << std::endl; )

		HAM_DEBUG( HAM_LOG << "communicator::communicator(): connecting targets to host" << std::endl; )


		// init peers structure
		peers = new tcp_peer[nodes_];

		// targets init tcp connection to host
		if(!is_host()) {
            tcp::socket* sock = new tcp::socket(io_context); // socket is always stored with index = target node, so no "if_host" switching is necessary for functions executed on host and target
            peers[host_node_].tcp_socket = sock;
            tcp::resolver resolver(io_context);
            //tcp::resolver::query query(tcp::v4(), host_address_, host_port_);
            //tcp::resolver::iterator iter = resolver.resolve(query);
			boost::asio::connect(*peers[host_node_].tcp_socket, resolver.resolve(host_address_, host_port_));

			// send requested rank to host
			HAM_DEBUG( HAM_LOG << "communicator::communicator(): requesting ham-address " << this_node_ << " from host" << std::endl; )
			boost::asio::write(*peers[host_node_].tcp_socket, boost::asio::buffer((void*)&this_node_, sizeof(this_node_)));
			// recv rank from host
			boost::asio::read(*peers[host_node_].tcp_socket, boost::asio::buffer((void*)&this_node_, sizeof(this_node_)));
			HAM_DEBUG( HAM_LOG << "communicator::communicator(): received ham-address " << this_node_ << " from host" << std::endl; )
		}

		// host accepts tcp connection from targets
		if(is_host()) {
			tcp::resolver resolver(io_context);
			tcp::resolver::query query(tcp::v4(), host_address_, host_port_);
			tcp::resolver::iterator iter = resolver.resolve(query);
			tcp::endpoint endpoint = iter->endpoint();
			tcp::acceptor acc(io_context, endpoint);

			node_t req_ranks[nodes_]; // store requested ranks in order of connection
			tcp::socket* temp_socks[nodes_]; // store sockets temporarily in connection order
            for (int l = 1; l < nodes_; ++l) {
                temp_socks[l] = new tcp::socket(io_context);
            }

			bool taken_ranks[nodes_];
            for (int x = 0; x < nodes_; ++x) {
                taken_ranks[x]= false;
            }
			taken_ranks[0] = true; // host rank has to be correctly provided and is therefore already taken (by the executing process)

			for(int i=1; i < nodes_; i++) {
                 // temp_socks[i]->close();
				 acc.accept(*temp_socks[i]); // accept connection

				// recv rank
				boost::asio::read(*temp_socks[i], boost::asio::buffer((void *) &req_ranks[i], sizeof(node_t)));
			}

			// rearrange sockets and inform targets of resulting rank
			for (int j = 1; j < nodes_; ++j) {
				if((req_ranks[j] > (nodes_-1))) { // check if rank invalid // TODO: fix -1 wildcard, currently not possible because req_ranks is unsigned node_t=size_t
					std::cout << "communicator::communicator(): illegal ham-address requested: " << req_ranks[j] << std::endl;
					exit(-1);
				}else if(req_ranks[j] == -1) { // skip wildcard ranks, handled later to avoid conflicting ranks with following connects
					HAM_DEBUG( HAM_LOG << "communicator::communicator(): connection " << j << " requested wildcard ham-address" << std::endl; )
					continue;
				}
				if(taken_ranks[req_ranks[j]]) { // check if rank already taken
					std::cout << "communicator::communicator(): ham-address requested more than once:" << req_ranks[j] << std::endl;
					exit(-1);
				} else {
					node_t rrank = req_ranks[j];
					HAM_DEBUG( HAM_LOG << "communicator::communicator(): connection " << j << " requested ham-address: " << rrank << std::endl; )
					peers[rrank].tcp_socket = temp_socks[j]; // = move https://www.boost.org/doc/libs/1_65_0/doc/html/boost_asio/reference/basic_stream_socket/operator_eq_.html
					taken_ranks[rrank] = true; // mark the requested rank as taken
					HAM_DEBUG( HAM_LOG << "communicator::communicator(): associated ham-address: " << rrank << " with connection " << j << std::endl; )
					// send assigned rank to target
					boost::asio::write(*peers[rrank].tcp_socket, boost::asio::buffer((void*)&rrank, sizeof(rrank)));
				}
			}

			// handle wildcard ranks
			for (int k = 1; k < nodes_; ++k) { // k is index to connections in connection order
				if(req_ranks[k] == -1) { // find wildcard connections

					for (int i = 1; i < nodes_; ++i) { // i is index to ranks in final rank order
						if(!taken_ranks[i]) { // find a free rank
							HAM_DEBUG( HAM_LOG << "communicator::communicator(): associating wildcard connection: " << k << " with ham-address " << i << std::endl; )
							peers[i].tcp_socket = temp_socks[k];
							taken_ranks[i] = true;
							boost::asio::write(*peers[i].tcp_socket, boost::asio::buffer((void*)&i, sizeof(i)));
							break; // stop if free rank is assigned, go back to k-loop for next wildcard connection
						}
					}
				}
			}
		}

		HAM_DEBUG( HAM_LOG << "communicator::communicator(): connecting hosts done" << std::endl; )

		// host init message buffers
		if (is_host()) {
			for (node_t i = 1; i < nodes_; ++i) { // TODO(improvement): needs to be changed when host-rank becomes configurable
				// allocate buffers
				peers[i].msg_buffers = allocate_buffer<msg_buffer>(constants::MSG_BUFFERS, this_node_);
				// fill resource pools
				for(size_t j = constants::MSG_BUFFERS; j > 0; --j) {
					peers[i].buffer_pool.add(j-1);
				}
			}

            HAM_DEBUG( HAM_LOG << "communicator::communicator(): initializing buffers done" << std::endl; )

			// host runs io_context in separate thread (asynchronous progress thread) for async operations
            thread_ = std::thread([this](){
                HAM_DEBUG( HAM_LOG << "ASYNC THREAD: Heyooo, I live." << std::endl; )
                // TODO(bug fix): need to figure out how to reset work from main thread so the background thread can return from run() before the host killst the io_context
                boost::asio::io_service::work work(io_context);
                io_context.run();
                HAM_DEBUG( HAM_LOG << "ASYNC THREAD: Oh noes, I'm dead!" << std::endl; )
                }
            );
            // thread_.detach(); no longer needed with member thread

            HAM_DEBUG( HAM_LOG << "communicator::communicator(): async thread started" << std::endl; )
		}



	}

	~communicator()
	{
		// TODO(bug fix): what we actually want:
        // stop the work guard, so the thread will return from io_context.run() when all outstanding ops completed
        // join the thread so the host waits until above is done
        // stop the context
        // currently: have to kill the context first because otherwise the thread wont complete to be joined
        // but this causes thread to abandon any outstanding ops
        io_context.stop();
        if(is_host()) {
            thread_.join();
        }
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
	
		tcp_peer& peer = peers[req.target_node];

		peer.buffer_pool.free(req.send_buffer_index);
		peer.buffer_pool.free(req.recv_buffer_index);
		req.valid_ = false;
	}

public:

	// called by host only
	void send_msg(request_reference_type req, void* msg, size_t size)
	{
		// copy message from caller into transfer buffer
		void* msg_buffer = static_cast<void*>(&peers[req.target_node].msg_buffers[req.send_buffer_index]);
		memcpy(msg_buffer, msg, size);

		// tcp write
		// auto self(shared_from_this());
        HAM_DEBUG( HAM_LOG << "communicator::send_msg(): sending msg to: " << req.target_node << std::endl; )
        //always write full message size TODO(improvement): improve with delimiter and read_until @ target
		boost::asio::async_write(*peers[req.target_node].tcp_socket, boost::asio::buffer(msg_buffer, constants::MSG_SIZE),
								[this, &req](boost::system::error_code ec, size_t length) {
                                    if (!ec)
                                    {
                                        req.sent_ = true;
                                        HAM_DEBUG( HAM_LOG << "THREAD: Async completion handler executed, send_msg() to " << req.target_node << " completed. Wrote " << length << " Bytes." << std::endl; )
                                    } else {
                                        HAM_DEBUG( HAM_LOG << "THREAD: Async completion handler executed, failed to send_msg() to " << req.target_node << " Error: " << ec.message() << std::endl; )
                                    }
								}
		);
		// MPI_Isend(msg_buffer, size, MPI_BYTE, req.target_node, constants::DEFAULT_TAG, MPI_COMM_WORLD, &req.next_mpi_request());
	}
	
	// to be used by the offload target's main loop: synchronously receive one message at a time
	// NOTE: the local static receive buffer!
	void* recv_msg_host(void* msg = nullptr, size_t size = constants::MSG_SIZE)
	{
		static msg_buffer buffer; // NOTE !
		// MPI_Recv(&buffer, size, MPI_BYTE, host_node_, constants::DEFAULT_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        HAM_DEBUG( HAM_LOG << "communicator::recv_msg_host(): node " << this_node_ << " awaiting AM from host"  << std::endl; )
		boost::asio::read(*peers[host_node_].tcp_socket, boost::asio::buffer(&buffer, size)); // will always read full MSG_SIZE
        return static_cast<void*>(&buffer);
	}

    // target only -> sync
    // send result through communicator
    // only to be used by request.send_result()
    template<class T>
    void send_result(node_t target_node, T* message, size_t size) {
        HAM_DEBUG( HAM_LOG << "communicator::send_result(): node " << this_node_ << " sending result to node: " << target_node  << std::endl; )
        void* ptr; // ugly stuff to wrap result into MSG_SIZE buffer TODO(improvement): change to transfering only actual result size by using delimiter and read_until in recv_result()
        posix_memalign((void**)&ptr, constants::CACHE_LINE_SIZE, constants::MSG_SIZE);
        memcpy(ptr, message, size);
        boost::asio::write(*peers[target_node].tcp_socket, boost::asio::buffer(ptr, constants::MSG_SIZE));
    }

    // host only -> async
	// trigger receiving the result of an active message on the host
	void recv_result(request_reference_type req)
	{
		// tcp receive
        // auto self(shared_from_this());
        HAM_DEBUG( HAM_LOG << "communicator::recv_result(): receiving msg from: " << req.target_node << std::endl; )

        boost::asio::async_read(*peers[req.target_node].tcp_socket, boost::asio::buffer(static_cast<void*>(&peers[req.target_node].msg_buffers[req.recv_buffer_index]), constants::MSG_SIZE),
				[this, &req](boost::system::error_code ec, size_t length) {
                    if (!ec)
                    {
                        req.received_ = true;
                        HAM_DEBUG( HAM_LOG << "THREAD: Async completion handler executed, recv_result() completed " << req.target_node << std::endl; )
                    } else {
                        HAM_DEBUG( HAM_LOG << "THREAD: Async completion handler executed, failed to recv_result() from " << req.target_node << " Error: " << ec.message() << std::endl; )
                    }
				}
		);
		// MPI_Irecv(static_cast<void*>(&peers[req.target_node].msg_buffers[req.recv_buffer_index]), constants::MSG_SIZE, MPI_BYTE, req.target_node, constants::RESULT_TAG, MPI_COMM_WORLD, &req.next_mpi_request());
		return;
	}

    // target only, host never uses sync variant
	template<typename T>
	void send_data(T* local_source, buffer_ptr<T> remote_dest, size_t size)
	{
		// tcp send
        boost::asio::write(*peers[remote_dest.node()].tcp_socket, boost::asio::buffer((void*)local_source, size * sizeof(T)));

		// MPI_Send((void*)local_source, size * sizeof(T), MPI_BYTE, remote_dest.node(), constants::DATA_TAG, MPI_COMM_WORLD);
	}

	// host only
	template<typename T>
	void send_data_async(request_reference_type req, T* local_source, buffer_ptr<T> remote_dest, size_t size)
	{
		// auto self(shared_from_this());
		boost::asio::async_write(*peers[remote_dest.node()].tcp_socket, boost::asio::buffer((void*)local_source, size*sizeof(T)),
								 [&req](boost::system::error_code ec, size_t length) {
									 req.sent_ = true;
								 }
		);
		// MPI_Isend((void*)local_source, size * sizeof(T), MPI_BYTE, remote_dest.node(), constants::DATA_TAG, MPI_COMM_WORLD, &req.next_mpi_request());
	}

    // target only
	template<typename T>
	void recv_data(buffer_ptr<T> remote_source, T* local_dest, size_t size)
	{
		// tcp recv
        boost::asio::read(*peers[remote_source.node()].tcp_socket, boost::asio::buffer((void*)local_dest, size * sizeof(T)));
		// MPI_Recv((void*)local_dest, size * sizeof(T), MPI_BYTE, remote_source.node(), constants::DATA_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	
	// to be used by the host
	template<typename T>
	void recv_data_async(request_reference_type req, buffer_ptr<T> remote_source, T* local_dest, size_t size)
	{
        // auto self(shared_from_this());
		boost::asio::async_read(*peers[remote_source.node()].tcp_socket, boost::asio::buffer(static_cast<void*>(local_dest), size*sizeof(T)),
								[&req](boost::system::error_code ec, size_t length) {
									req.received_ = true;
								}
		);
		// MPI_Irecv(static_cast<void*>(local_dest), size * sizeof(T), MPI_BYTE, remote_source.node(), constants::DATA_TAG, MPI_COMM_WORLD, &req.next_mpi_request());
	}

	template<typename T>
	buffer_ptr<T> allocate_buffer(const size_t n, node_t source_node)
	{
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
        return instance().node_desc_dummy;
	}

private:
	static communicator* instance_;
	size_t nodes_;
	node_t this_node_;
	int this_port_;
	node_t host_node_;
	std::string host_address_;
	std::string host_port_;
    node_descriptor node_desc_dummy;
	boost::asio::io_service io_context;
    std::thread thread_;

    struct tcp_peer {
		buffer_ptr<msg_buffer> msg_buffers; // buffers used for MPI_ISend and IRecv by the sender

		// needed by sender to manage which buffers are in use and which are free
		// just manages indices, that can be used by
		detail::resource_pool<size_t> buffer_pool;

		// tcp socket
		tcp::socket* tcp_socket;
	};
	
	tcp_peer* peers;
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

#endif // ham_net_communicator_tcp_hpp
