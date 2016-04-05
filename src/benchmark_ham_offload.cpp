// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload.hpp"

#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <stdlib.h> // posix_memalign

#include "ham/util/time.hpp"

using namespace std;
using namespace ham::util::time;
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

offload::buffer_ptr<float> offload_allocate(size_t data_size)
{
	// equals: #pragma offload target(mic) nocopy(data:length(data_size) alloc_if(1) free_if(0))
	return offload::allocate<float>(mic_node, data_size);
}

void offload_free(offload::buffer_ptr<float> remote)
{
	// equals: #pragma offload target(mic) nocopy(data:length(data_size) alloc_if(0) free_if(1))
	offload::free(remote);
}

void offload_copy_in(offload::buffer_ptr<float> remote, float* local, size_t data_size)
{
	// equals :#pragma offload target(mic) in(data:length(data_size) alloc_if(0) free_if(0))
	offload::put_sync(local, remote, data_size); // sync
}

void offload_copy_out(offload::buffer_ptr<float> remote, float* local, size_t data_size)
{
	// equals: #pragma offload target(mic) in(data:length(data_size) alloc_if(0) free_if(0))
	offload::get_sync(remote, local, data_size); //sync
}

#ifndef HAM_COMM_ONE_SIDED
void offload_copy_direct(offload::buffer_ptr<float> source, offload::buffer_ptr<float> dest, size_t data_size)
{
	// no equivalent
	offload::copy_sync(source, dest, data_size); // TODO: async if ready
}
#endif

float fun_mul(float a, float b)
{
	return a * b;
}

void fun()
{
	return;
}

void offload_call()
{
	offload::sync(mic_node, f2f(&fun));
}

#ifndef HOST_ALIGNMENT
#define HOST_ALIGNMENT 64
#endif

float* local_allocate(size_t size)
{
	//return new float[data_size];
	float* ptr = NULL;
	int err = 0;
	if ((err = posix_memalign((void**)&ptr, HOST_ALIGNMENT, size * sizeof(float))))
		std::cout << "error, posix_memalign() returned: " << err << std::endl;
	return ptr;
	// NOTE: no ctors will be called!
}

void local_free(float* ptr)
{
	//delete [] ptr;
	free((void*)ptr);
	// NOTE: no dtors will be called!
}


int main(int argc, char * argv[])
{
	// document command line
	std::cout << "# Command line was: " << std::endl << "# ";
	for(int i = 0; i < argc; ++i)
		std::cout << argv[i] << " ";
	std::cout << std::endl;

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
		("size,s", boost::program_options::value(&data_size)->default_value(data_size), "size of transferred data in byte (multiple of 4)")
		("allocate,a", boost::program_options::value<bool>()->zero_tokens(), "benchmark memory allocation/deallocation on target")
		("copy-in,i", boost::program_options::value<bool>()->zero_tokens(), "benchmark data copy to target")
		("copy-out,o", boost::program_options::value<bool>()->zero_tokens(), "benchmark data copy from target")
		("call,c", boost::program_options::value<bool>()->zero_tokens(), "benchmark function call on target")
		("call-mul,m", boost::program_options::value<bool>()->zero_tokens(), "benchmark function call (multiplication) on target")
		("async,y", boost::program_options::value<bool>()->zero_tokens(), "perform benchmark function calls asynchronously")
	;

	boost::program_options::variables_map vm;

	// NOTE: no try-catch here to avoid exceptions, that cause offload-dependencies to boost exception in the MIC code
	//boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
	boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), vm);
	boost::program_options::notify(vm);

	if(vm.count("help"))
	{
		std::cout << desc << std::endl;
		return 1;
	}

	///////////////// output compile configuration and run data ////////////
	#ifdef BOOST_NO_EXCEPTIONS
		std::cout << "# BOOST_NO_EXCEPTIONS          enabled" << std::endl;
	#else
		std::cout << "# BOOST_NO_EXCEPTIONS          disabled" << std::endl;
	#endif
	#ifdef HAM_DEBUG_ON
		std::cout << "# HAM_DEBUG_ON                 enabled" << std::endl;
	#else
		std::cout << "# HAM_DEBUG_ON                 disabled" << std::endl;
	#endif
	#ifdef HAM_COMM_MPI
		std::cout << "# COMM_MPI                     enabled" << std::endl;
	#else
		std::cout << "# COMM_MPI                     disabled" << std::endl;
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
	#ifdef HAM_COMM_ONE_SIDED
		std::cout << "# HAM_COMM_ONE_SIDED           enabled" << std::endl;
	#else
		std::cout << "# HAM_COMM_ONE_SIDED           disabled" << std::endl;
	#endif
		std::cout << "# HAM_MESSAGE_SIZE             " << HAM_MESSAGE_SIZE << std::endl;
		std::cout << "# HOST_ALIGNMENT               " << HOST_ALIGNMENT << std::endl;


	if (data_size % sizeof(float) != 0)
		std::cout << "WARNING specified size must be a multiple of 4" << std::endl;
	// data allocate data of given size for benchmark
	size_t data_size_byte = data_size;
	data_size = data_size / sizeof(float); // convert data size specified in bytes to floats
	float* data = local_allocate(data_size);

	std::string header_string = "name\t" + statistics::header_string();
	std::string header_string_data = "name\t" + statistics::header_string() + "\tdata_size";

	if (vm.count("allocate"))
	{
		statistics allocate_time(runs);
		statistics free_time(runs);
		offload::buffer_ptr<float> remote_data;
		for (size_t i = 0; i < warmup_runs; ++i)
		{
			remote_data = offload_allocate(data_size);
			offload_free(remote_data);
		}

		for (size_t i = 0; i < runs; ++i)
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
			 << "allocate:\t" << allocate_time.string() << "\t" << data_size_byte << endl;
		allocate_time.to_file(filename + "allocate_time");

		cout << "HAM-Offload free time: " << endl
			 << header_string_data << endl
			 << "free:\t" << free_time.string() << "\t" << data_size_byte << endl;
		free_time.to_file(filename + "free_time");
	}

	if (vm.count("copy-in"))
	{
		// first allocate memory
		offload::buffer_ptr<float> remote_data = offload_allocate(data_size);
		statistics copy_in_time(runs);
		for (size_t i = 0; i < warmup_runs; ++i)
		{
			offload_copy_in(remote_data, data, data_size);
		}

		for (size_t i = 0; i < runs; ++i)
		{
			timer clock;
			offload_copy_in(remote_data, data, data_size);
			copy_in_time.add(clock);
		}
		// free memory
		offload_free(remote_data);

		cout << "HAM-Offload copy-in time: " << endl
			 << header_string_data << endl
			 << "copy-in:\t" << copy_in_time.string() << "\t" << data_size_byte << endl;
		copy_in_time.to_file(filename + "copy_in_time");
	}

	if (vm.count("copy-out"))
	{
		// first allocate memory
		offload::buffer_ptr<float> remote_data = offload_allocate(data_size);
		statistics copy_out_time(runs);
		for (size_t i = 0; i < warmup_runs; ++i)
		{
			offload_copy_out(remote_data, data, data_size);
		}

		for (size_t i = 0; i < runs; ++i)
		{
			timer clock;
			offload_copy_out(remote_data, data, data_size);
			copy_out_time.add(clock);
		}
		// free memory
		offload_free(remote_data);

		cout << "HAM-Offload copy-out time: " << endl
			 << header_string_data << endl
			 << "copy-out:\t" << copy_out_time.string() << "\t" << data_size_byte << endl;
		copy_out_time.to_file(filename + "copy_out_time");
	}

	if (vm.count("call"))
	{
		statistics call_time(runs);
		auto func = f2f(&fun);
		for (size_t i = 0; i < warmup_runs; ++i)
		{
			if(vm.count("async"))
			{
				auto f = offload::async(mic_node, func);
				f.get();
			}
			else // sync
			{
				offload::sync(mic_node, func);
			}
		}

		for (size_t i = 0; i < runs; ++i)
		{
			if(vm.count("async"))
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

	if (vm.count("call-mul"))
	{
		statistics call_mul_time(runs);

		float a = 2.0f;
		float b = 4.0f;
		float res = 0.0f;

		auto func = f2f(&fun_mul, a, b);

		for (size_t i = 0; i < warmup_runs; ++i)
		{
			if(vm.count("async"))
			{
				auto f = offload::async(mic_node, func);
				res = f.get();
			}
			else // sync
			{
				res = offload::sync(mic_node, func);
			}

		}
		for (size_t i = 0; i < runs; ++i)
		{
			if(vm.count("async"))
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
