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

double print_buffer_content(offload::buffer_ptr<double> x, size_t n)
{
	std::cout << "printing data on node " << x.node() << std::endl;
	for (size_t i = 0; i < n; ++i)
		std::cout << x[i] << " ";
	std::cout << std::endl;
	return 50.0;
}

int main(int argc, char* argv[])
{
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
	offload::node_t target_b = 2; // we simply the second device/node

	// allocate device memory (returns a buffer_ptr<T>)
	auto target_buffer_a = offload::allocate<double>(target_a, n);
	auto target_buffer_b = offload::allocate<double>(target_b, n);

	offload::sync(target_a, f2f(&print_buffer_content, target_buffer_a, n));

	std::cout << "a - get: " << target_buffer_a.get() << std::endl;
	std::cout << "a - node: " << target_buffer_a.node() << std::endl;

#ifdef HAM_COMM_MPI_RMA_DYNAMIC
	std::cout << "a - mpi: " << target_buffer_a.get_mpi_address() << std::endl;
#endif

	std::cout << "b - get: " << target_buffer_b.get() << std::endl;
	std::cout << "b - node: " << target_buffer_b.node() << std::endl;

#ifdef HAM_COMM_MPI_RMA_DYNAMIC
	std::cout << "b - mpi: " << target_buffer_b.get_mpi_address() << std::endl;
#endif
	
	// host -> target_a -> target_b -> host
	std::cout << "put to target_a: ";
	offload::put(write_buffer.data(), target_buffer_a, n);
	std::cout << "done" << std::endl;

	offload::sync(target_a, f2f(&print_buffer_content, target_buffer_a, n));

	std::cout << "copy from target_a to target_b: ";
	offload::copy_sync(target_buffer_a, target_buffer_b, n);
	std::cout << "done" << std::endl;

	offload::async(target_b, f2f(&print_buffer_content, target_buffer_b, n));

	std::cout << "get from target_b: ";
	offload::get(target_buffer_b, read_buffer.data(), n);
	std::cout << "done" << std::endl;
	// verify
	bool passed = compare(write_buffer, read_buffer);
	
	std::cout << "Verified data? " << passed << std::endl;
	
	return 0;	
}

