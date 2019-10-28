// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload.hpp"
#include <algorithm>
#include <array>
#include <iostream>

using namespace ham;

class base
{
public:
	virtual int test_method() = 0;
};

class derived_a : public base
{
public:
	virtual int test_method() { return 0; };

};

class derived_b : public base
{
public:
	virtual int test_method() { return 1; };

};

// offloaded function
int test_method_call_function(derived_a arg)
{
	base* base = &arg;
	std::cout << "Kernel: &derived_a::test_method = " << (&derived_a::test_method) << std::endl;
//	std::cout << "Kernel: &base::test_method = " << static_cast<void*>(&base::test_method) << std::endl;

	return base->test_method();
}

/**
 * Tests polymporphic objects that have virtual methods, and thus v-tables.
 */
int main(int argc, char* argv[])
{
	std::cout << "Testing polymorphism (virtual methods):" << std::endl;

	offload::node_t target = 1;

	derived_a a;
	derived_b b;
	
	std::cout << "Host: &derived_a::test_method = " << (&derived_a::test_method) << std::endl;
	
	int result_a = offload::sync(target, f2f(&test_method_call_function, a));
//	int result_b = offload::sync(target, f2f(&test_method_call_function, b));

	
	bool passed = (a.test_method() == result_a); // && (b.test_method() == result_b)
	std::cout << "Result: " << (passed ? "pass" : "fail") << std::endl;
	
	return 0;	
}

