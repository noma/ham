// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define HAM_EXPLICIT // set here, before including HAM-Offload, or in your build system
#include "ham/offload.hpp"
#include <iostream>
#include <list>

using namespace ham;

int function_test_1()
{
	std::cout << "function_test_1 executed" << std::endl;
	return 42;
}

int function_test_2(int i)
{
	return i;
}

void function_test_void(int i)
{
	std::cout << "function_test_void executed: i=" << i  << std::endl;
}

int main(int argc, char* argv[])
{
	// MPI-like SPMD programme, main() is run in all processes
	std::cout << "ham offload test" << std::endl;
	// explicitly initialise HAM-Offload
	if (offload::ham_init(argc, argv)) // NOTE: ham_init returns true on the host process
	{
		std::cout << "ham offload test" << std::endl;
		for (size_t i = 0; i < offload::num_nodes(); ++i)
		{
			if(!offload::is_host(i))
			{
				std::cout << "Testing offload with ham node: " << i << std::endl;
				std::cout << "Node's hostname is: " << offload::get_node_description(i).name() << std::endl;

				int r = offload::sync(i, f2f(&function_test_1));
				std::cout << "r = " << r << std::endl;

				std::cout << "Testing async with future<int>..." << std::endl;
				offload::future<int> f1 = offload::async(i, f2f(&function_test_2, 23));
				std::cout << "i = " << f1.get() << std::endl;
				std::cout << "DONE" << std::endl;

				std::cout << "Testing async with future<int> again..." << std::endl;
				offload::future<int> f2 = offload::async(i, f2f(&function_test_2, 25));
				std::cout << "i = " << f2.get() << std::endl;
				std::cout << "DONE" << std::endl;

				std::cout << "Testing sync with future<int>..." << std::endl;
				int r1 = offload::sync(i, f2f(&function_test_2, 42));
				std::cout << "i = " << r1 << std::endl;
				std::cout << "DONE" << std::endl;

				std::cout << "Testing async with future<void>" << std::endl;
				offload::future<void> f3 = offload::async(i, f2f(&function_test_void, 31337));
				f3.get();
				std::cout << "DONE" << std::endl;

				std::cout << "Testing async multiple future<int> read in reverse order" << std::endl;
				{
					std::list<offload::future<int>> results;
					for (size_t j = 0; j < 10; ++j)
					{
						results.emplace_back(offload::async(i, f2f(&function_test_2, j)));
					}
					results.reverse();
					for (auto& result : results)
					{
						std::cout << "Got result: " << result.get() << std::endl;
					}
				}
			}
		}
	}

	offload::ham_finalise(); // explicitly deinitialise HAM-Offload
	
	return 0;	
}
