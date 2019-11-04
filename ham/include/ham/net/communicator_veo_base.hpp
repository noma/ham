// Copyright (c) 2013-2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_net_communicator_veo_base_hpp
#define ham_net_communicator_veo_base_hpp

#include <cassert>
#include <errno.h>
#include <stdlib.h> // posix_memalign
#include <string.h> // errno messages
#include <unistd.h>

#include "ham/misc/constants.hpp"
#include "ham/misc/options.hpp"
#include "ham/misc/resource_pool.hpp"
#include "ham/misc/types.hpp"
#include "ham/util/debug.hpp"
#include "ham/util/log.hpp"

namespace ham {
namespace net {

template<typename T>
class buffer_ptr
{
public:
	buffer_ptr();
	buffer_ptr(T* ptr, node_t node) : ptr_(ptr), node_(node) { }

	T* get() const { return ptr_; }
//	off_t offset() const { return offset_; }
//	size_t size() const { return size_; }
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
//		return buffer_ptr(ptr_ + off, node_, offset_ + byte, size_ - byte);
		return buffer_ptr(ptr_ + off, node_);
	}

private:
	T* ptr_;
	node_t node_;
//	off_t offset_; // SCIF offset inside registered window
//	size_t size_; // registered size
};

class node_descriptor
{
public:
	const char* name() const { return name_; }
	
private:
	static constexpr size_t name_length_ {256};
	char name_[name_length_];

	friend class communicator;
};

class communicator_options : public ham::detail::options
{
public:
	communicator_options(int* argc_ptr, char** argv_ptr[]) : options(argc_ptr, argv_ptr)
	{
// NOTE: no command line handling on the VE side
//       see also options in options.hpp
// TODO: solve mystery: just adding options on the VE side actually influences the results offload call benchmark significantly (~1 µs) (VE code makes the difference)
#ifndef HAM_COMM_VE
		// add backend-specific options
		app_.add_option("--ham-process-count", ham_process_count_, "Number of processes the job consists of (number of targets + 1).");
		app_.add_option("--ham-host-address", ham_host_address_, "The address of the host process (0 by default).");
		app_.add_option("--ham-veo-ve-lib", veo_library_path_, "Path to the VEO VE library of the application.");
		app_.add_option("--ham-veo-ve-nodes", veo_ve_nodes_, "VE Nodes to be used as offload targets, comma separated list, no spaces.");
#endif

		// NOTE: no further inheritance or adding
		parse();
	}

	// command line argument getters
	const uint64_t& ham_process_count() const { return ham_process_count_; }
	const node_t& ham_host_address() const { return ham_host_address_; }
	const std::string& veo_library_path() const { return veo_library_path_; }
	const std::string& veo_ve_nodes() const { return veo_ve_nodes_; }

private:
	uint64_t ham_process_count_ = 2; // number of participating processes
	node_t ham_host_address_ = 0; // the address of the host process
	std::string veo_library_path_ = "";
	std::string veo_ve_nodes_ = "";
};

template<class Derived>
class communicator_base {
public:
	using communicator = Derived;

	enum {
		MAX_VEO_NODES = 9, // host + 8 VEs
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

	communicator_base(communicator* instance)
	{
		assert(instance_ == nullptr); // singleton
		instance_ = instance;
		HAM_DEBUG( HAM_LOG << "FLAG_FALSE = " << FLAG_FALSE << ", NO_BUFFER_INDEX = " << NO_BUFFER_INDEX << std::endl; )
	}
	
	static communicator& instance() { return *instance_; }
	static bool initialised() { return instance_ != nullptr; };

	static communicator* instance_;

	template<typename T>
	buffer_ptr<T> allocate_buffer(const size_t n, node_t source_node)
	{
		HAM_UNUSED_VAR(source_node);
		// TODO: figure out if page-size restriction is necessary/beneficial on the VE
		// NOTE: no ctor calls (buffer vs. array)
		//ptr = new(ptr) T[n];
		T* ptr;
		size_t n_rma  = n * sizeof(T); // space to allocate on byte
		n_rma = n_rma % constants::PAGE_SIZE == 0 ? n_rma : ((n_rma / constants::PAGE_SIZE) + 1) * constants::PAGE_SIZE; // round to whole pages
		//int err =
		posix_memalign((void**)&ptr, constants::PAGE_SIZE, n_rma); // allocate page boundary aligned memory

		HAM_DEBUG( HAM_LOG << "communicator::allocate_buffer(): ptr = " << ptr << "(" << (uint64_t)ptr << "), node = " << source_node << std::endl; )

		return buffer_ptr<T>(ptr, Derived::this_node());
	}

	template<typename T>
	void free_buffer(buffer_ptr<T> ptr)
	{
		assert(ptr.node() == Derived::this_node());
		// NOTE: no ctor calls (buffer vs. array)
		//delete [] ptr;
		free((void*)ptr.get());
	}

	size_t round_to_full_pages(size_t size, size_t page_size) 
	{
		return size % page_size == 0 ? size : ((size / page_size) + 1) * page_size;
	}

protected:
	void errno_handler(int ret, const char * hint)
	{
		if (ret < 0)
		{
			//char buffer[256];
			// TODO:
			// NOTE: strerror_r not supported by NEC compiler, will probably get fixed with VEOS transition to glibc with v2.0
			//char* error_message = strerror_r(errno, buffer, 256);
			//HAM_LOG << "Errno(" << errno << ") Message for \"" << hint << "\": " << error_message << std::endl;
			HAM_LOG << "Errno(" << errno << ") Message for \"" << hint << "\": " << "[no msg available]" << std::endl;
			exit(-1);
		}
	}
	
};

// for singleton implementation
template<class Derived>
typename communicator_base<Derived>::communicator* communicator_base<Derived>::instance_ = nullptr;

} // namespace net
} // namespace ham

#endif // ham_net_communicator_veo_base_hpp
