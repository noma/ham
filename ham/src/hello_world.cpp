// Copyright (c) 2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload.hpp"
#include <iostream>

using namespace ham;

int hello(int id) 
{
	std::cout << "Hello from target: " << id << std::endl;
	
	return 1337;
}

int main(int argc, char* argv[])
{
	HAM_UNUSED_VAR(argc);
	HAM_UNUSED_VAR(argv);
	
	offload::node_t target = 1; // we simply use the first device/node
	
	int n = 42;
	auto hello_future = offload::async(target, f2f(&hello, n));
	std::cout << "Received hello return: " << hello_future.get() << std::endl;

	return 0;
}

