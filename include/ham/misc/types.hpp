// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_misc_types_hpp
#define ham_misc_types_hpp

#include <algorithm>
#include <cstddef>

namespace ham {

typedef size_t node_t; // node type, e.g. MPI rank, identifies remote target process
typedef size_t flag_t; // MPI RMA completion flag / buffer index
typedef char*  msg_buffer_t; // buffer type for messages

namespace net {
class communicator;
}

namespace detail {

template<typename T>
class result_container
{
public:
	result_container() = default;
	result_container(T&& res) : res(res) { }
	T get() { return T(std::move(res)); }

private:
	T res;
};

template<>
class result_container<void>
{
public:
	result_container() = default;
	void get() { }
};

} // namespace detail

} // namespace ham

#endif // ham_misc_types_hpp
