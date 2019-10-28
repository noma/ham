// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_misc_types_hpp
#define ham_misc_types_hpp

#include <algorithm>
#include <cstddef>

#include "ham/misc/migratable.hpp"

namespace ham {

using node_t = int; // node type, e.g. MPI rank, identifies remote target process
using msg_buffer_t = char*; // buffer type for messages

namespace net {
class communicator;
} // namespace net

namespace detail {

template<typename T>
class result_container
{
public:
	result_container() = default;
	result_container(T&& res) : res(res) { }
	T get() { return T(std::move(res)); }

private:
	migratable<T> res;
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
