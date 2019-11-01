// Copyright (c) 2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload.hpp"
#include <iostream>

// expected: node == this_node()
ham::node_t hello(ham::node_t node)
{
	std::cout << '\t' << "Hello from target " << ham::offload::this_node() << " aka '"
	          << ham::offload::get_node_description(ham::offload::this_node()).name()
	          << "', got: " << node << " from host '"
	          << ham::offload::get_node_description(0).name() << '\'' << std::endl;

	return ham::offload::this_node();
}

int main(int argc, char* argv[])
{
	// avoid compiler warning
	HAM_UNUSED_VAR(argc);
	HAM_UNUSED_VAR(argv);

	bool passed = true;

	// iterate through all nodes
	for (size_t i = 0; i < ham::offload::num_nodes(); ++i) {
		ham::offload::node_t target = static_cast<ham::node_t>(i);

		// skip the host, i.e. the process running this code
		if (target != ham::offload::this_node()) {

			std::cout << "Calling hello(" << target << ") on target " << target << " aka '"
			          << ham::offload::get_node_description(ham::offload::this_node()).name()
			          << "' from " << ham::offload::this_node() << " aka '"
			          << ham::offload::get_node_description(target).name() << '\'' << std::endl;

			auto hello_future = ham::offload::async(target, f2f(&hello, target));
			ham::node_t result = hello_future.get();

			// output the result
			std::cout << "hello() returned: " << result << std::endl;

			passed = passed && (target == result);
		}
	}

	return passed ? 0 : -1;;
}

