// Copyright (c) 2013-2017 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <noma/bmt/bmt.hpp>

#include <iostream>
#include <thread>

namespace bmt = ::noma::bmt;

int main(int args, char* argv[])
{
	size_t iterations = 100; // iterations to be measured
	size_t warmup_iterations = 5; // iterations to be skipped before starting measuring

	// generate a table header with name column
	std::cout << bmt::statistics::header_string(true) << std::endl;

	// benchmark the cost of timing
	bmt::statistics timing_overhead_stats {"timing_overhead", iterations, warmup_iterations};
	
	// do all iterations, inlcuding warmup_iterations which will be ignored by stats
	for (size_t i = 0; i < (iterations + warmup_iterations); ++i)
	{
		bmt::timer timer; // creata a timer, starts measuring on construction
		// nothing to do
		timing_overhead_stats.add(timer); // add timer to statistics object (measuring is stopped)
	}


	// benchmark something that takes time
	bmt::statistics sleep_for_stats {"sleep_for", iterations, warmup_iterations};
	
	// do all iterations, inlcuding warmup_iterations which will be ignored by stats
	for (size_t i = 0; i < (iterations + warmup_iterations); ++i)
	{
		bmt::timer timer; // creata a timer, starts measuring on construction
		std::this_thread::sleep_for(bmt::milliseconds { 25 }), // spend some time
		sleep_for_stats.add(timer); // add timer to statistics object (measuring is stopped)
	}


	// output table entries with complete data
	std::cout << timing_overhead_stats.string() << std::endl;
	std::cout << sleep_for_stats.string() << std::endl;
	
	// output just the averages in differend units
	std::cout << timing_overhead_stats.name() << " average: " 
	          << std::chrono::duration_cast<bmt::nanoseconds>(timing_overhead_stats.average()).count() << " ns" 
	          << std::endl;
	std::cout << sleep_for_stats.name() << " average: " 
	          << std::chrono::duration_cast<bmt::milliseconds>(sleep_for_stats.average()).count() << " ms" 
	          << std::endl;

	return 0;
}
