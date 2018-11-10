// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// something for "requires ham_offload_hpp"

#ifndef ham_offload_stream_hpp
#define ham_offload_stream_hpp

#include "ham/net/communicator.hpp"

#include <sstream>
#include <string>

#include "ham/misc/types.hpp"
#include "ham/functor/buffer.hpp"
#include "ham/offload/offload_msg.hpp"
#include "ham/offload/offload.hpp"
#include "ham/offload/runtime.hpp"
#include "ham/util/at_end_of_scope_do.hpp"
#include "ham/util/debug.hpp"
#include "ham/util/log.hpp"


namespace ham {
namespace offload {
namespace stream {

using ::ham::net::buffer_ptr;
using ::ham::node_t;
using ::ham::byte_t;

class ostream;

class stream_base {
public:
	stream_base(node_t target) : target_(target) {}

	stream_base(node_t target, buffer_ptr<byte_t> buffer, size_t size) : target_(target), buffer_(buffer),
	                                                                     size_(size) {}
	// put common stuff of ostream/istream here

	buffer_ptr<byte_t> buffer() { return buffer_; }

	void buffer(buffer_ptr<byte_t> buffer) { buffer_ = buffer; }

	size_t size() { return size_; }

	void size(size_t size) { size_ = size; }

	node_t target() { return target_; } // no setting intended
protected:
	node_t target_;
	buffer_ptr<byte_t> buffer_; // remote sink, remote memory
	size_t size_; // size of remote sink
};

class stream_proxy {

	friend class istream;

public:
	stream_proxy() = default; // default contstuctor needed for return transport dummy entries
	stream_proxy(stream_base *stream) : target_(stream->target()), buffer_(stream->buffer()),
	                                    size_(stream->size()) {
		HAM_DEBUG( HAM_LOG << "stream_proxy::ctor() called" << std::endl; )
	}

private:
	node_t target_;
	buffer_ptr<byte_t> buffer_;
	size_t size_;
};

class ostream : public stream_base, public std::ostringstream {

public:
	// always need the node associated with this stream
	ostream(node_t target) : stream_base(target), std::ostringstream() {
		HAM_DEBUG( HAM_LOG << "ostream::ctor() called" << std::endl; )
	}

	ostream(node_t target, size_t size) : stream_base(target), std::ostringstream(), fixed_(true) {
		posix_memalign((void **) &fixed_ptr_, constants::CACHE_LINE_SIZE, size);
		rdbuf()->pubsetbuf(fixed_ptr_, size);
		HAM_DEBUG( HAM_LOG << "ostream::ctor() for provided buffer size called" << std::endl; )
		// NOTE: this does NOT set the streams buffer or size. It will only associate a buffer that should be large enough to not need resizing (user's responsibility)
		// if it should not be large enough, it may still be resized/reallocated
	}

	~ostream() {
		if (fixed_) std::free((void *) fixed_ptr_);
		HAM_DEBUG( HAM_LOG << "ostream::dtor()" << std::endl; )
	}

	const stream_proxy sync() {
		std::string temp = rdbuf()->str(); // COPY ... no other option, direct pointers not accessible
		if (ham::offload::is_host()) { // on host
			HAM_DEBUG( HAM_LOG << "host executing ostream::sync()" << std::endl; )
			size_ = temp.size();
			buffer_ = offload::allocate<byte_t>(target_, size_);
			HAM_DEBUG( HAM_LOG << "ostream::sync() allocated buffer @" << target_ << std::endl; )
			offload::put_sync((byte_t *) temp.c_str(), buffer_, size_);
			HAM_DEBUG( HAM_LOG << "ostream::sync() sent data to " << target_ << std::endl; )
			return stream_proxy(this);
		} else { // on target
			HAM_DEBUG( HAM_LOG << "target executing ostream::sync()" << std::endl; )
			ham::net::communicator &comm = ham::offload::runtime::instance().communicator();
			size_ = temp.size();
			buffer_ = comm.allocate_buffer<byte_t>(size_, ham::offload::this_node());
			HAM_DEBUG( HAM_LOG << "ostream::sync() allocated local buffer" << std::endl; )
			strcpy((char *) buffer_.get(), temp.c_str()); // COPY ... no other option, depending on backend we need the mem to be allocated by new_buffer
			return stream_proxy(this);
		}
	}

	// we reduce the dynamic here
	/*
	- use like a local in-memory stream, i.e. stringstream, maybe inherit stringstream, or output version
		- ss.str().data() and size()
		- on explicit synchronisation request from user
			- allocate remote memory, set internal butter_ptr with known size
			- put() data onto target
	*/
private:
	bool fixed_ = false;
	byte_t *fixed_ptr_ = nullptr;
};


class istream : public stream_base, public std::istringstream {
public:
	istream(const stream_proxy proxy) : stream_base(proxy.target_, proxy.buffer_, proxy.size_),
	                                    std::istringstream() {
		HAM_DEBUG( HAM_LOG << "istream::ctor() called with stream_proxy" << target_ << std::endl; )
		if (ham::offload::is_host()) {
			posix_memalign((void **) &local_ptr_, constants::CACHE_LINE_SIZE, size_);
			offload::get_sync(buffer_, local_ptr_, size_);
			HAM_DEBUG( HAM_LOG << "istream::sync() host retrieved data from " << buffer_.node() << std::endl; )
			this->rdbuf()->pubsetbuf(local_ptr_, size_);
		} else {
			rdbuf()->pubsetbuf(buffer_.get(), size_); // avoid a copy that would be necessary when using str(string) to set the content
			HAM_DEBUG( HAM_LOG << "istream::sync() target set streambuffer to remote buffer" << target_ << std::endl; )
		}
	}
	// fail on underflow, set flags/state whatever, check std::istream interface

	// maybe use stringstream and reconstruct from data_

	~istream() {
		if (ham::offload::is_host()) {
			offload::free(buffer_);
			HAM_DEBUG( HAM_LOG << "istream::dtor() freed memory @" << target_ << std::endl; )
			std::free((void *) local_ptr_);
		} else {
			ham::net::communicator &comm = ham::offload::runtime::instance().communicator();
			comm.free_buffer(buffer_); // this is where we trash "used" buffers on the targets
			HAM_DEBUG( HAM_LOG << "istream::dtor() freed local memory" << std::endl; )
		}
	}

private:
	byte_t *local_ptr_ = nullptr;
};


} // namespace stream
}
} // namespace ham
#endif // ham_offload_stream_hpp
