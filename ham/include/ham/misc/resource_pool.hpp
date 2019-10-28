// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_misc_resource_pool_hpp
#define ham_misc_resource_pool_hpp

#include <iostream>
#include <stack>
#include <vector>
#include "ham/misc/constants.hpp"

namespace ham {
namespace detail {

template<typename T>
class resource_pool
{
public:
    using value_type = T;
    using size_type = int;
    using container_type = std::vector<value_type>;

	//resource_pool(size_t count) //: values(count)
    resource_pool() = default; //: values(count)

	resource_pool(const resource_pool&) = delete;
	resource_pool(resource_pool&& other)
	{
		free_stack = std::move(other.free_stack);
	}

	resource_pool& operator=(const resource_pool&) = delete;
	resource_pool& operator=(resource_pool&& other)
	{
		free_stack = std::move(other.free_stack);
		return *this;
	}

	~resource_pool() = default;

	//void add(value_type&& v)
	void add(value_type v)
	{
		free_stack.push(v);
		//std::cout << "resource_pool::add(), added value: " << v << std::endl;
	}

	value_type allocate()
	{
		// test if stack is non-empty
		if (!free_stack.empty()) {
			value_type result = free_stack.top();
			free_stack.pop();
			return result;
		}
		// else:
		std::cerr << "resource_pool::allocate(), error: ran out of resources, returning default object." << std::endl;
		return value_type();
	}

	value_type next()
	{
		// test if stack is non-empty
		if (!free_stack.empty()) {
			return free_stack.top();
		} else {
			std::cerr << "resource_pool::next(), error: ran out of resources, returning default object." << std::endl;
			return value_type();
		}
	}

	void free(value_type& buffer)
	{
		// push buffer back on free-stack
		free_stack.emplace(buffer);
	}

	void free(value_type&& buffer)
	{
		// push buffer back on free-stack
		free_stack.emplace(std::move(buffer));
	}

private:
	//container_type values;
	std::stack<value_type, std::vector<value_type>> free_stack; // pointers to resources
};

} // namespace detail
} // namespace ham

#endif // ham_misc_resource_pool_hpp

