// Copyright (c) 2013-2018 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload.hpp"

#include <CLI/CLI11.hpp>
#include <noma/bmt/bmt.hpp>

#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib> // posix_memalign

using namespace std;
using namespace noma::bmt;
using namespace ham;

constexpr node_t mic_node = 1;

#ifdef BOOST_NO_EXCEPTIONS
namespace boost
{
void throw_exception( std::exception const & e )
{
	cerr << "Exception handler called... bye." << endl;
}
}
#endif

offload::buffer_ptr<char> offload_allocate(size_t data_size)
{
	// equals: #pragma offload target(mic) nocopy(data:length(data_size) alloc_if(1) free_if(0))
	return offload::allocate<char>(mic_node, data_size);
}

void offload_free(offload::buffer_ptr<char> remote)
{
	// equals: #pragma offload target(mic) nocopy(data:length(data_size) alloc_if(0) free_if(1))
	offload::free(remote);
}

void offload_copy_in(offload::buffer_ptr<char> remote, char* local, size_t data_size)
{
	// equals :#pragma offload target(mic) in(data:length(data_size) alloc_if(0) free_if(0))
	offload::put_sync(local, remote, data_size); // sync
}

void offload_copy_out(offload::buffer_ptr<char> remote, char* local, size_t data_size)
{
	// equals: #pragma offload target(mic) in(data:length(data_size) alloc_if(0) free_if(0))
	offload::get_sync(remote, local, data_size); //sync
}

#ifndef HAM_COMM_ONE_SIDED
// no LEO/OpenMP equivalent
void offload_copy_direct(offload::buffer_ptr<char> source, offload::buffer_ptr<char> dest, size_t data_size)
{
	offload::copy_sync(source, dest, data_size); // TODO: use async
}
#endif

float fun_mul(float a, float b)
{
	return a * b;
}

void fun()
{
}

void offload_call()
{
	offload::sync(mic_node, f2f(&fun));
}

#ifndef HOST_ALIGNMENT
#define HOST_ALIGNMENT 64
#endif

char* local_allocate(size_t size)
{
	//return new char[data_size];
	char* ptr = nullptr;
	int err = 0;
	if ((err = posix_memalign((void**)&ptr, HOST_ALIGNMENT, size * sizeof(char))))
		std::cout << "error, posix_memalign() returned: " << err << std::endl;
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

	// configuration and default values
	unsigned int warmup_runs = 1;
	unsigned int runs = 1000;
	string filename = "benchmark/offload_intel_";
	size_t data_size = 1024*1024; // 1 MiB
	bool allocate = false;
	bool copy_in = false;
	bool copy_out = false;
	bool call = false;
	bool call_mul = false;
	bool async = false;

	// command line options
	CLI::App app("Supported options");
	app.allow_extras(); // ignore options for HAM
	app.add_option("--filename,-f", filename, "filename(-prefix) for results");
	app.add_option("--runs,-r", runs, "number of identical inner runs for which the average time will be computed");
	app.add_option("--warmup-runs", warmup_runs, "number of number of additional warmup runs before times are measured");
	app.add_option("--size,-s", data_size, "size of transferred data in byte (multiple of 4)");
	app.add_flag("--allocate,-a", allocate, "benchmark memory allocation/deallocation on target");
	app.add_flag("--copy-in,-i", copy_in, "benchmark data copy to target");
	app.add_flag("--copy-out,-o", copy_out, "benchmark data copy from target");
	app.add_flag("--call,-c", call, "benchmark function call on target");
	app.add_flag("--call-mul,-m", call_mul, "benchmark function call (multiplication) on target");
	app.add_flag("--async,-y", async, "perform benchmark function calls asynchronously");

	CLI11_PARSE(app, argc, argv);

	///////////////// output compile configuration and run data ////////////
		std::cout << "# HOST_ALIGNMENT               " << HOST_ALIGNMENT << std::endl;

	#ifdef HAM_DEBUG_ON
		std::cout << "# HAM_DEBUG_ON                 enabled" << std::endl;
	#else
		std::cout << "# HAM_DEBUG_ON                 disabled" << std::endl;
	#endif

		std::cout << "# HAM_MESSAGE_SIZE             " << HAM_MESSAGE_SIZE << std::endl;

	#ifdef HAM_COMM_ONE_SIDED
		std::cout << "# HAM_COMM_ONE_SIDED           enabled" << std::endl;
	#else
		std::cout << "# HAM_COMM_ONE_SIDED           disabled" << std::endl;
	#endif

	#ifdef HAM_COMM_MPI
		std::cout << "# HAM_COMM_MPI                 enabled" << std::endl;
	#else
		std::cout << "# HAM_COMM_MPI                 disabled" << std::endl;
	#endif

	#ifdef HAM_COMM_VEO
		std::cout << "# HAM_COMM_VEO                 enabled" << std::endl;
	#else
		std::cout << "# HAM_COMM_VEO                 disabled" << std::endl;
	#endif
	#ifdef HAM_COMM_VEDMA
		std::cout << "# HAM_COMM_VEDMA               enabled" << std::endl;
	#else
		std::cout << "# HAM_COMM_VEDMA               disabled" << std::endl;
	#endif

	#if defined(HAM_COMM_VEO) || defined(HAM_COMM_VEDMA)
	#ifdef HAM_COMM_VEO_VE
		std::cout << "# HAM_COMM_VEO_VH              enabled" << std::endl;
	#else
		std::cout << "# HAM_COMM_VEO_VH              disabled" << std::endl;
	#endif
	#ifdef HAM_COMM_VEO_VE
		std::cout << "# HAM_COMM_VEO_VE              enabled" << std::endl;
	#else
		std::cout << "# HAM_COMM_VEO_VE              disabled" << std::endl;
	#endif
	#ifdef HAM_COMM_VEO_STATIC
		std::cout << "# HAM_COMM_VEO_STATIC          enabled" << std::endl;
	#else
		std::cout << "# HAM_COMM_VEO_STATIC          disabled" << std::endl;
	#endif
	#endif

	#ifdef HAM_COMM_SCIF
		std::cout << "# HAM_COMM_SCIF                enabled" << std::endl;
	#ifdef HAM_SCIF_RMA_CPU
		std::cout << "# HAM_SCIF_RMA_CPU             enabled" << std::endl;
	#else
		std::cout << "# HAM_SCIF_RMA_CPU             disabled" << std::endl;
	#endif
		std::cout << "# HAM_SCIF_RMA_CPU_THRESHOLD   " << HAM_SCIF_RMA_CPU_THRESHOLD << std::endl;
	#else
		std::cout << "# HAM_COMM_SCIF                disabled" << std::endl;
	#endif


	// allocate host data of given size
	char* data = local_allocate(data_size);

	std::string header_string = statistics::header_string(true);
	std::string header_string_data = statistics::header_string(true) + "\tdata_size";

	if (allocate)
	{
		statistics allocate_time(runs, warmup_runs);
		statistics free_time(runs, warmup_runs);
		offload::buffer_ptr<char> remote_data;

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			{
				timer clock;
				remote_data = offload_allocate(data_size);
				allocate_time.add(clock);
			}
			{
				timer clock;
				offload_free(remote_data);
				free_time.add(clock);
			}

		}

		cout << "HAM-Offload allocate time: " << endl
			 << header_string_data << endl
			 << "allocate:\t" << allocate_time.string() << "\t" << data_size << endl;
		allocate_time.to_file(filename + "allocate_time");

		cout << "HAM-Offload free time: " << endl
			 << header_string_data << endl
			 << "free:\t" << free_time.string() << "\t" << data_size << endl;
		free_time.to_file(filename + "free_time");
	}

	if (copy_in)
	{
		// first allocate memory
		offload::buffer_ptr<char> remote_data = offload_allocate(data_size);
		statistics copy_in_time(runs, warmup_runs);

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			timer clock;
			offload_copy_in(remote_data, data, data_size);
			copy_in_time.add(clock);
		}
		// free memory
		offload_free(remote_data);

		cout << "HAM-Offload copy-in time: " << endl
			 << header_string_data << endl
			 << "copy-in:\t" << copy_in_time.string() << "\t" << data_size << endl;
		copy_in_time.to_file(filename + "copy_in_time");
	}

	if (copy_out)
	{
		// first allocate memory
		offload::buffer_ptr<char> remote_data = offload_allocate(data_size);
		statistics copy_out_time(runs, warmup_runs);

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			timer clock;
			offload_copy_out(remote_data, data, data_size);
			copy_out_time.add(clock);
		}
		// free memory
		offload_free(remote_data);

		cout << "HAM-Offload copy-out time: " << endl
			 << header_string_data << endl
			 << "copy-out:\t" << copy_out_time.string() << "\t" << data_size << endl;
		copy_out_time.to_file(filename + "copy_out_time");
	}

	if (call)
	{
		statistics call_time(runs, warmup_runs);
		auto func = f2f(&fun);

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			if (async)
			{
				timer clock;
				auto f = offload::async(mic_node, func);
				f.get();
				call_time.add(clock);
			}
			else // sync
			{
				timer clock;
				offload::sync(mic_node, func);
				//offload_call();
				call_time.add(clock);
			}
		}

		cout << "HAM-Offload function call runtime: " << endl
			 << header_string << endl
			 << "call:\t" << call_time.string() << endl;
		call_time.to_file(filename + "call_time");
	}

	if (call_mul)
	{
		statistics call_mul_time(runs, warmup_runs);

		float a = 2.0f;
		float b = 4.0f;
		float res = 0.0f;

		auto func = f2f(&fun_mul, a, b);

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			if (async)
			{
				timer clock;
				auto f = offload::async(mic_node, func);
				res = f.get();
				call_mul_time.add(clock);
			}
			else // sync
			{
				timer clock;
				res = offload::sync(mic_node, func);
				call_mul_time.add(clock);
			}
		}

		float c = res; res = c; // actually read res (avoids compiler warning)
		
		cout << "HAM-Offload function call mul runtime: " << endl
			 << header_string << endl
			 << "call-mul:\t" << call_mul_time.string() << endl;
		call_mul_time.to_file(filename + "call_mul_time");
	}

	local_free(data);

	return EXIT_SUCCESS;
}
