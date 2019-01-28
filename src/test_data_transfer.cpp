// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload.hpp"
#include <algorithm>
#include <iostream>
#include <vector>

using namespace ham;

template<typename T>
void initialise_memory(std::vector<T>& a, T&& value = T()) 
{
	// initialise host memory
	for (size_t i = 0; i < a.size(); ++i) {
		a[i] = value;
	}
}

template<typename T>
bool compare(const std::vector<T>& a, const std::vector<T>& b)
{
	if(a.size() != b.size()) return false;
	
	return std::equal(a.begin(), a.end(), b.begin());
}

int main(int argc, char* argv[])
{
	HAM_UNUSED_VAR(argc);
	HAM_UNUSED_VAR(argv);

	std::cout << "Testing data transfer: host -> target_a -> target_b -> host." << std::endl;
	
	// buffer size
	constexpr size_t n = 1024;

	// allocate host memory
	std::vector<double> write_buffer(n);
	std::vector<double> read_buffer(n);
	
	initialise_memory(write_buffer, 1.0);
	initialise_memory(read_buffer, 2.0);

	// specify two offload targets
	offload::node_t target_a = 1; // we simply the first device/node
#ifdef HAM_COMM_ONE_SIDED
	offload::node_t target_b = 2; // we simply the second device/node
#else
	offload::node_t target_b = 1; // we use the same device
#endif

	// allocate device memory (returns a buffer_ptr<T>)
	auto target_buffer_a = offload::allocate<double>(target_a, n);
	auto target_buffer_b = offload::allocate<double>(target_b, n);
	
	// host -> target_a -> target_b -> host
	// NOTE: in case of HAM_COMM_ONE_SIDED we do: host -> target_a -> host
	offload::put(write_buffer.data(), target_buffer_a, n);
#ifndef HAM_COMM_ONE_SIDED
	offload::copy_sync(target_buffer_a, target_buffer_b, n);
#endif	
	offload::get(target_buffer_b, read_buffer.data(), n);
	
	// verify
	bool passed = compare(write_buffer, read_buffer);
	
	std::cout << "Verified data? " << passed << std::endl;

	return passed ? 0 : -1;
}

