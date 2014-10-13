// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_functor_buffer_hpp
#define ham_functor_buffer_hpp

#include "ham/net/communicator.hpp"

namespace ham {

template<class T>
class new_buffer {
public:
	typedef net::buffer_ptr<T> result_type;
	new_buffer(size_t n, node_t source) : n(n), source(source) { }

	result_type operator()() const
	{
		return net::communicator::instance().allocate_buffer<T>(n, source); // bind to source
		//return new T[n];
	}
private:
	size_t n;
	node_t source;
};

template<class T>
class delete_buffer {
public:
	typedef void result_type;
	delete_buffer(net::buffer_ptr<T> ptr) : ptr(ptr) { }

	result_type operator()()
	{
		net::communicator::instance().free_buffer<T>(ptr);
		//delete [] ptr;
	}
private:
	net::buffer_ptr<T> ptr;
};

} // namespace ham

#endif // ham_functor_buffer_hpp
