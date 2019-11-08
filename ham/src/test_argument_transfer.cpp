// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload.hpp"
#include <algorithm>
#include <array>
#include <iostream>

using namespace ham;

// offloaded function
template<typename T>
T type_transfer_function(T arg)
{
//	std::cout << "Target: type_transfer_function() got: '" << arg << "'" << std::endl;
	return arg;
}

// performs the offload for one type and value
template<typename T>
bool test_type_invocation(offload::node_t target, T arg)
{
	T result = offload::sync(target, f2f(&type_transfer_function<T>, arg));
	bool passed = result == arg;
//	std::cerr << "Host: Passed '" << arg << "', got '" << result << "'" << std::endl;
	std::cout << "Result for type \"" << typeid(T).name() << "\": " << (passed ? "pass" : "fail") << std::endl;
	return passed;
}

// performs the test for a list of types and values
template<typename... Ts>
bool test_type_transfer(offload::node_t target, Ts... args)
{
	std::array<bool, sizeof...(Ts)> results = { {test_type_invocation(target, args)...} };
	return std::all_of(std::begin(results), std::end(results), [](bool v) { return v; });
}

/**
 * Tests the argument and return value transfer for offloaded function.
 * For each type, a value is passed as argument and then returned. The test is
 * passed, if the the passed argument and the return value are equal.
 */
int main(int argc, char* argv[])
{
	HAM_UNUSED_VAR(argc);
	HAM_UNUSED_VAR(argv);

	std::cout << "Testing argument and return value transfer for different types:" << std::endl;

	offload::node_t target = 1;
	bool passed = false;
	
	// list of types and values to test, static_cast syntax is not only for types with unclear literals, but also for readability
	passed = test_type_transfer(target, 
		// boolean
		static_cast<bool>(true),
		// character types
		static_cast<char>('c'), // 99
		static_cast<char16_t>(u'd'), // 100
		static_cast<char32_t>(U'e'), // 101
		static_cast<wchar_t>(L'f'), // 102
		// integer types
		static_cast<signed char>(103), // 'g'
		static_cast<short>(1337),
		static_cast<int>(1338),
		static_cast<long>(1339l),
		static_cast<long long>(1340ll),
		// unsigned integer types
		static_cast<unsigned char>(105), // 'h'
		static_cast<unsigned short>(1341),
		static_cast<unsigned>(1342),
		static_cast<unsigned long>(1343ul),
		static_cast<unsigned long long>(1344ull),
		// floating point types
		static_cast<float>(13.45f),
		static_cast<double>(13.46),
		static_cast<long double>(13.47l),
		// null pointer
		static_cast<std::nullptr_t>(nullptr),
		std::tuple<void*>(nullptr)
	);
	
	std::cout << "Overall result: " << (passed ? "pass" : "fail") << std::endl;
	
	return passed ? 0 : -1;
}

