// Copyright (c) 2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload.hpp"
#include <iostream>

int hello(int num)
{
	std::cout << '\t' << "Hello from target " << ham::offload::this_node() << ", got: " << num << std::endl;
	
	return 42;
}

int main(int argc, char* argv[])
{
	// avoid compiler warning
	HAM_UNUSED_VAR(argc);
	HAM_UNUSED_VAR(argv);

	// use first offload target
	ham::offload::node_t target = 1; // we simply use the first device/node

	// some argument for hello
	int n = 1337;

	std::cout << "Calling hello(" << n << ") on target " << target << std::endl;

	// asynchronously call "hello(n)" on the target
	auto hello_future = ham::offload::async(target, f2f(&hello, n));

	// host can do something in parallel here

	// synchronise on the result, get() will block until the result is ready
	int result = hello_future.get();

	// output the result
	std::cout << "hello() returned: " << result << std::endl;

	return 0;
}

