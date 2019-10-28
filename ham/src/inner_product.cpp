// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload.hpp"
#include <array>
#include <iostream>

using namespace ham;

double inner_product(offload::buffer_ptr<double> x, offload::buffer_ptr<double> y, size_t n)
{
	double z = 0.0;
	for (size_t i = 0; i < n; ++i)
		z += x[i] * y[i];
	return z;
}

int main(int argc, char* argv[])
{
	HAM_UNUSED_VAR(argc);
	HAM_UNUSED_VAR(argv);

	// buffer size
	constexpr size_t n = 1024;

	// allocate host memory
	std::array<double, n> a, b;

	// initialise host memory
	for (size_t i = 0; i < n; ++i) {
		a[i] = i;
		b[i] = n-i;
	}

	// specify an offload target
	offload::node_t target = 1; // we simply the first device/node

	// get some information about the target
	auto target_description = offload::get_node_description(target);
	std::cout << "Using target node " << target << " with hostname " << target_description.name() << std::endl;

	// allocate device memory (returns a buffer_ptr<T>)
	auto a_target = offload::allocate<double>(target, n);
	auto b_target = offload::allocate<double>(target, n);
	
	// transfer data to the device (the target is implicitly specified by the destination buffer_ptr)	
	auto future_a_put = offload::put(a.data(), a_target, n); // async
	offload::put(b.data(), b_target, n); // sync (implicitly returned future performs synchronisation in dtor), alternative: put_sync()
	
	// synchronise
	future_a_put.get();
	
	// asynchronously offload the call to inner_product
	auto c_future = offload::async(target, f2f(&inner_product, a_target, b_target, n));

	// synchronise on the result
	double c = c_future.get(); 

	// we also could have used:
	// double c = offload::async(...).get();
	// double c = offload::sync(...);
	// if we weren't interested in the offload's result, this would also be synchronous, because in this case, the returned future's dtor performs the synchronisation:
	// offload.async(...);

	// output the result
	std::cout << "Result: " << c << std::endl;

	// compare with local computation, with explicitly created buffer_ptr temporaries pointing to the local data of the std:array objects
	double c_compare = inner_product(offload::buffer_ptr<double>(a.data(), offload::this_node()), offload::buffer_ptr<double>(b.data(), offload::this_node()), n);
	bool passed = c == c_compare; // NOTE: floating point comparison

	return passed ? 0 : -1;
}

