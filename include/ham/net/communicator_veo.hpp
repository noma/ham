// Copyright (c) 2013-2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_net_communicator_veo_hpp
#define ham_net_communicator_veo_hpp

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
	
	// TODO(feature): add properties: 
	//    - node type (HOST | MIC | ...)
	//    - socket (to which the mic is connected)
//private: // TODO: removed for testin, 2019-01-24, re-enable and do propper friends if needed
	static constexpr size_t name_length_ {256};
	char name_[name_length_];

	friend class communicator;
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
	

	static communicator* instance_;

	template<typename T>
	buffer_ptr<T> allocate_buffer(const size_t n, node_t source_node)
	{
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

#endif // ham_net_communicator_veo_hpp
