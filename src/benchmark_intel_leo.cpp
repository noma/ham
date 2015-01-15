// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <stdlib.h> // posix_memalign

#include "ham/util/time.hpp"

// memory allocation macros for offload pragma modifiers
#define ALLOC alloc_if(1)
#define FREE free_if(1)
#define REUSE alloc_if(0)
#define RETAIN free_if(0)

#ifndef HOST_ALIGNMENT
#define HOST_ALIGNMENT 64
#endif

#ifndef MIC_ALIGNMENT
#define MIC_ALIGNMENT 64
#endif

using namespace std;
using namespace ham::util::time;


void set_cpu_affinity(int core = 0)
{
		std::cout << "Setting CPU affinity to: " << core << std::endl;
		cpu_set_t mask;
		CPU_ZERO(&mask);
		CPU_SET(core, &mask);

		if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
		{
			std::cout << "WARNING: Could not set CPU affinity, continuing..." << std::endl;
		}
}

#ifdef BOOST_NO_EXCEPTIONS
namespace boost
{
void throw_exception( std::exception const & e )
{
	cerr << "Exception handler called... bye." << endl;
}
}
#endif

void offload_allocate(char* data, size_t data_size)
{
	#pragma offload target(mic:0) nocopy(data:length(data_size) align(MIC_ALIGNMENT) alloc_if(1) free_if(0))
	{}
}

void offload_free(char* data, size_t data_size)
{
	#pragma offload target(mic:0) nocopy(data:length(data_size) alloc_if(0) free_if(1))
	{}
}

void offload_copy_in(char* data, size_t data_size)
{

#ifdef LEO_SYNC_TRANSFER
//	data[0] = data[data_size - 1]; // touch data on host
	#pragma offload_transfer target(mic:0) in(data:length(data_size) alloc_if(0) free_if(0)) // out(data:length(1) alloc_if(0) free_if(0))
//	{
//		data[0] = data[data_size - 1]; // touch data on phi
//	}
#elif defined LEO_ASYNC_TRANSFER
	#pragma offload_transfer target(mic:0) in(data:length(data_size) alloc_if(0) free_if(0)) signal(data)
	#pragma offload_transfer target(mic:0) wait(data)
#else
	#pragma offload target(mic:0) in(data:length(data_size) alloc_if(0) free_if(0))
	{}
#endif
}

void offload_copy_out(char* data, size_t data_size)
{
#ifdef LEO_SYNC_TRANSFER
	#pragma offload_transfer target(mic:0) out(data:length(data_size) alloc_if(0) free_if(0))
//	{
//		data[0] = data[data_size - 1]; // touch data on phi
//	}
//	data[0] = data[data_size - 1]; // touch data on host
#elif defined LEO_ASYNC_TRANSFER
	#pragma offload_transfer target(mic:0) out(data:length(data_size) alloc_if(0) free_if(0)) signal(data)
	#pragma offload_transfer target(mic:0) wait(data)
#else
	#pragma offload target(mic:0) out(data:length(data_size) alloc_if(0) free_if(0))
	{}
#endif
}

__attribute__((target(mic)))
float fun_mul(float a, float b)
{
	return a * b;
}

__attribute__((target(mic)))
void fun()
{
	return;
}

void offload_call()
{
	#pragma offload target(mic:0)
	fun();
	//res = fun_mul(a, b);
}

char* local_allocate(size_t size)
{
	//return new char[data_size];
	char* ptr = NULL;
	int err = 0;
	if ((err = posix_memalign((void**)&ptr, HOST_ALIGNMENT, size * sizeof(char))))
		std::cout << "error, posix_memalign() returned: " << err << std::endl;
	//std::cout << "posix_memalign result: " << ptr << ", aligned to: " << HOST_ALIGNMENT << ", ptr % alignment = " << ((size_t)ptr % HOST_ALIGNMENT) << std::endl;
	return ptr;
	// NOTE: no ctors will be called!
}

void local_free(char* ptr)
{
	//delete [] ptr;
	free((void*)ptr);
	// NOTE: no dtors will be called!
}

int main(int argc, char * argv[])
{
	// document command line
	std::cout << "# Command line was: " << std::endl << "# ";
	for (int i = 0; i < argc; ++i)
		std::cout << argv[i] << " ";
	std::cout << std::endl;

	//set cpu affinity
	int pinning = 0;

	// configuration and default values
	unsigned int warmup_runs = 1;
	unsigned int runs = 1000;
	string filename = "benchmark/offload_intel_";
	size_t data_size = 1024*1024; // 1 MiB

	// command line options
	boost::program_options::options_description desc("Supported options");
	desc.add_options()
		("help,h", "Shows this message")
		("filename,f", boost::program_options::value(&filename), "filename(-prefix) for results")
		("runs,r", boost::program_options::value(&runs)->default_value(runs), "number of identical inner runs for which the average time will be computed")
		("warmup-runs", boost::program_options::value(&warmup_runs)->default_value(warmup_runs), "number of number of additional warmup runs before times are measured")
		("pinning,p", boost::program_options::value(&pinning)->default_value(pinning), "pin host process to this core")
		("size,s", boost::program_options::value(&data_size)->default_value(data_size), "size of transferred data in byte (multiple of 4)")
		("allocate,a", boost::program_options::value<bool>()->zero_tokens(), "benchmark memory allocation/deallocation on mic")
		("copy-in,i", boost::program_options::value<bool>()->zero_tokens(), "benchmark data copy to mic")
		("copy-out,o", boost::program_options::value<bool>()->zero_tokens(), "benchmark data copy from mic")
		("call,c", boost::program_options::value<bool>()->zero_tokens(), "benchmark function call on mic")
		("call-mul,m", boost::program_options::value<bool>()->zero_tokens(), "benchmark function call (multiplication) on mic")
	;

	boost::program_options::variables_map vm;

	// NOTE: no try-catch here to avoid exceptions, that cause offload-dependencies to boost exception in the MIC code
	boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
	boost::program_options::notify(vm);

	if(vm.count("help"))
	{
		std::cout << desc << std::endl;
		return 1;
	}

	set_cpu_affinity(pinning);

	///////////////// output compile configuration and run data ////////////
	#ifdef __INTEL_OFFLOAD
		std::cout << "# __INTEL_OFFLOAD          enabled" << std::endl;
	#else
		std::cout << "# __INTEL_OFFLOAD          disabled" << std::endl;
	#endif
	#ifdef BOOST_NO_EXCEPTIONS
		std::cout << "# BOOST_NO_EXCEPTIONS      enabled" << std::endl;
	#else
		std::cout << "# BOOST_NO_EXCEPTIONS      disabled" << std::endl;
	#endif
	#ifdef LEO_SYNC_TRANSFER
		std::cout << "# LEO_SYNC_TRANSFER        enabled" << std::endl;
	#else
		std::cout << "# LEO_SYNC_TRANSFER        disabled" << std::endl;
	#endif
	#ifdef LEO_ASYNC_TRANSFER
		std::cout << "# LEO_ASYNC_TRANSFER       enabled" << std::endl;
	#else
		std::cout << "# LEO_ASYNC_TRANSFER       disabled" << std::endl;
	#endif
		std::cout << "# HOST_ALIGNMENT           " << HOST_ALIGNMENT << std::endl;
		std::cout << "# MIC_ALIGNMENT            " << MIC_ALIGNMENT << std::endl;

	// allocate host data of given size
	char* data = local_allocate(data_size);

	std::string header_string = "name\t" + statistics::header_string();
	std::string header_string_data = "name\t" + statistics::header_string() + "\tdata_size";

	if (vm.count("allocate"))
	{
		statistics allocate_time(runs, warmup_runs);
		statistics free_time(runs, warmup_runs);

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			{
				timer clock;
				offload_allocate(data, data_size);
				allocate_time.add(clock);
			}
			{
				timer clock;
				offload_free(data, data_size);
				free_time.add(clock);
			}

		}

		cout << "Intel Pragma Offload allocate time: " << endl
			 << header_string_data << endl
			 << "allocate:\t" << allocate_time.string() << "\t" << data_size << endl;
		allocate_time.to_file(filename + "allocate_time");

		cout << "Intel Pragma Offload free time: " << endl
			 << header_string_data << endl
			 << "free:\t" << free_time.string() << "\t" << data_size << endl;
		free_time.to_file(filename + "free_time");
	}

	if (vm.count("copy-in"))
	{
		// first allocate memory
		offload_allocate(data, data_size);
		statistics copy_in_time(runs, warmup_runs);

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			timer clock;
			offload_copy_in(data, data_size);
			copy_in_time.add(clock);
		}
		// free memory
		offload_free(data, data_size);

		cout << "Intel Pragma Offload copy-in time: " << endl
			 << header_string_data << endl
			 << "copy-in:\t" << copy_in_time.string() << "\t" << data_size << endl;
		copy_in_time.to_file(filename + "copy_in_time");
	}

	if (vm.count("copy-out"))
	{
		// first allocate memory
		offload_allocate(data, data_size);
		statistics copy_out_time(runs, warmup_runs);

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			timer clock;
			offload_copy_out(data, data_size);
			copy_out_time.add(clock);
		}
		// free memory
		offload_free(data, data_size);

		cout << "Intel Pragma Offload copy-out time: " << endl
			 << header_string_data << endl
			 << "copy-out:\t" << copy_out_time.string() << "\t" << data_size << endl;
		copy_out_time.to_file(filename + "copy_out_time");
	}



	if (vm.count("call"))
	{
		statistics call_time(runs, warmup_runs);

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			timer clock;
			offload_call();
			call_time.add(clock);
		}

		cout << "Intel Pragma Offload function call runtime: " << endl
			 << header_string << endl
			 << "call:\t" << call_time.string() << endl;
		call_time.to_file(filename + "call_time");
	}

	if (vm.count("call-mul"))
	{
		statistics call_mul_time(runs, warmup_runs);

		float a = 2.0f;
		float b = 4.0f;
		float res = 0.0f;

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			timer clock;
			{
			#pragma offload target(mic:0) in(a,b) out(res)
			res = fun_mul(a, b);
			}
			call_mul_time.add(clock);
		}

		cout << "Intel Pragma Offload function call mul runtime: " << endl
			 << header_string << endl
			 << "call-mul:\t" << call_mul_time.string() << endl;
		call_mul_time.to_file(filename + "call_mul_time");
	}

	local_free(data);

	return EXIT_SUCCESS;
}
