// Copyright (c) 2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <ve_offload.h>

#include <CLI/CLI11.hpp>
#include <noma/bmt/bmt.hpp>

#include <assert.h>
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include <cstdlib> // posix_memalign

using namespace std;
using namespace noma::bmt;

struct veo_proc_handle* veo_proc = nullptr;
uint64_t veo_lib_handle = 0;
struct veo_thr_ctxt* veo_ctx = nullptr;

#ifdef BOOST_NO_EXCEPTIONS
namespace boost
{
void throw_exception( std::exception const & e )
{
	cerr << "Exception handler called... bye." << endl;
}
}
#endif

void errno_handler(int ret, const char * hint)
{
	if (ret < 0)
	{
		char buffer[256];
		char* error_message = strerror_r(errno, buffer, 256);
		std::cerr << "Errno(" << errno << ") Message for \"" << hint << "\": " << error_message << std::endl;
		exit(-1);
	}
}

uint64_t offload_allocate(size_t data_size)
{
	// 
	uint64_t addr = 0;
	int err =
	veo_alloc_mem(veo_proc, &addr, data_size);
	// TODO: check err in Debug mode
	return addr;
}

void offload_free(uint64_t addr)
{
	int err = 
	veo_free_mem(veo_proc, addr);
	// TODO: check err in Debug mode
}

void offload_copy_in(uint64_t dst_addr, const void* src_ptr, size_t data_size)
{
	int err = 
	veo_write_mem(veo_proc, dst_addr, src_ptr, data_size);
	// TODO: check err in Debug mode
}

void offload_copy_out(void* dst_ptr, uint64_t src_addr, size_t data_size)
{
	int err = 
	veo_read_mem(veo_proc, dst_ptr, src_addr, data_size);
	// TODO: check err in Debug mode
}

// TODO: handle return value for fun_mul
void offload_call(uint64_t sym, veo_args* args)
{
	int err = 0;
	
	// call the function
	uint64_t id = veo_call_async(veo_ctx, sym, args);
	assert(id != VEO_REQUEST_ID_INVALID);

	// sync on result
	uint64_t res_addr = 0;
	err = veo_call_wait_result(veo_ctx, id, &res_addr); // sync on call
	// TODO: debug only
//	errno_handler(err, "veo_call_wait_result()");
}

float offload_call_mul(uint64_t sym, veo_args* args)
{
	int err = 0;
	
	// call the function
	uint64_t id = veo_call_async(veo_ctx, sym, args);
	assert(id != VEO_REQUEST_ID_INVALID);

	// sync on result
	uint64_t res_addr = 0;
	err = veo_call_wait_result(veo_ctx, id, &res_addr); // sync on call
	// TODO: debug only
//	errno_handler(err, "veo_call_wait_result()");

	// return the result
	return *(reinterpret_cast<float*>(&res_addr));
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

	string veo_library_path;
	int veo_ve_node = 0;

	// command line options
	CLI::App app("Supported options");
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
	app.add_option("--veo-ve-lib", veo_library_path, "path to VEO library");
	app.add_option("--veo-ve-node", veo_ve_node, "VE node to use for offloading");

	CLI11_PARSE(app, argc, argv);

	///////////////// output compile configuration and run data ////////////
	#ifdef BOOST_NO_EXCEPTIONS
		std::cout << "# BOOST_NO_EXCEPTIONS          enabled" << std::endl;
	#else
		std::cout << "# BOOST_NO_EXCEPTIONS          disabled" << std::endl;
	#endif
		std::cout << "# HOST_ALIGNMENT               " << HOST_ALIGNMENT << std::endl;

	// basic VEO setup
	// TODO: make this nicer
	#ifndef HAM_COMM_VEO // means HAM CMake defines to not apply
	#define HAM_COMM_VEO_STATIC // so default to static build
	#endif


	// VEO setup	
	int err = 0;

	// create VE process
#ifdef HAM_COMM_VEO_STATIC
	veo_proc = veo_proc_create_static(veo_ve_node, veo_library_path.c_str());
	if (veo_proc == 0) {
		std::cerr << "error: veo_proc_create_static(" << veo_ve_node << ") returned 0" << std::endl;
		exit(1);
	}
#else
	veo_proc = veo_proc_create(veo_ve_node);
	if (veo_proc == 0) {
		std::cerr << "error: veo_proc_create() returned 0" << std::endl;
		exit(1);
	}

#endif
	// TODO: nicify and unify error handling
	if (veo_proc == 0) {
		std::cerr << "error: veo_proc_create() returned 0" << std::endl;
		exit(1);
	}

	// load VE library image 
#ifdef HAM_COMM_VEO_STATIC
	veo_lib_handle = 0;
#else
	veo_lib_handle = veo_load_library(veo_proc, veo_library_path.c_str());
	if (veo_lib_handle == 0) {
		std::cerr << "veo_load_library() returned handle: " << (void*)veo_proc << std::endl;
	}
#endif

	// VEO context
	veo_ctx = veo_context_open(veo_proc);
	if (veo_ctx == nullptr) {
		std::cerr << "veo_context_open() returned ctx: " << veo_ctx << std::endl;
	}

	// allocate host data of given size
	char* data = local_allocate(data_size);
	
	std::string header_string = statistics::header_string(true);
	std::string header_string_data = statistics::header_string(true) + "\tdata_size";

	if (allocate)
	{
		statistics allocate_time(runs, warmup_runs);
		statistics free_time(runs, warmup_runs);
		
		uint64_t remote_data;

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

		cout << "VEO allocate time: " << endl
			 << header_string_data << endl
			 << "allocate:\t" << allocate_time.string() << "\t" << data_size << endl;
		allocate_time.to_file(filename + "allocate_time");

		cout << "VEO free time: " << endl
			 << header_string_data << endl
			 << "free:\t" << free_time.string() << "\t" << data_size << endl;
		free_time.to_file(filename + "free_time");
	}

	if (copy_in)
	{
		// first allocate memory
		uint64_t remote_data = offload_allocate(data_size);
		statistics copy_in_time(runs, warmup_runs);

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			timer clock;
			offload_copy_in(remote_data, (const void*)data, data_size);
			copy_in_time.add(clock);
		}
		// free memory
		offload_free(remote_data);

		cout << "VEO copy-in time: " << endl
			 << header_string_data << endl
			 << "copy-in:\t" << copy_in_time.string() << "\t" << data_size << endl;
		copy_in_time.to_file(filename + "copy_in_time");
	}

	if (copy_out)
	{
		// first allocate memory
		uint64_t remote_data = offload_allocate(data_size);
		statistics copy_out_time(runs, warmup_runs);

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			timer clock;
			offload_copy_out((void*)data, remote_data, data_size);
			copy_out_time.add(clock);
		}
		// free memory
		offload_free(remote_data);

		cout << "VEO copy-out time: " << endl
			 << header_string_data << endl
			 << "copy-out:\t" << copy_out_time.string() << "\t" << data_size << endl;
		copy_out_time.to_file(filename + "copy_out_time");
	}

	if (call)
	{
		statistics call_time(runs, warmup_runs);

		// setup VEO call
		struct veo_args* args = veo_args_alloc(); // empty args

		uint64_t sym = veo_get_sym(veo_proc, veo_lib_handle, "fun");
		assert(sym != 0); // i.e. symbol was found

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			if (async)
			{
				timer clock;
				offload_call(sym, args);
				call_time.add(clock);
			}
			else // sync: NOTE: there is no sync version currently
			{
				timer clock;
				offload_call(sym, args);
				call_time.add(clock);
			}
		}

		veo_args_free(args);

		cout << "VEO function call runtime: " << endl
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

		// setup VEO call
		struct veo_args* args = veo_args_alloc();
		err = veo_args_set_float(args, 0, a); // 1st set argument
		errno_handler(err, "veo_args_set_float(0)");
		err = veo_args_set_float(args, 1, b); // 2st set argument
		errno_handler(err, "veo_args_set_float(1)");

		uint64_t sym = veo_get_sym(veo_proc, veo_lib_handle, "fun_mul");
		assert(sym != 0); // i.e. symbol was found

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			if (async)
			{
				timer clock;
				res = offload_call_mul(sym, args);
				call_mul_time.add(clock);
			}
			else // sync: NOTE: there is no sync version currently
			{
				timer clock;
				res = offload_call_mul(sym, args);
				call_mul_time.add(clock);
			}
		}

		veo_args_free(args);

		float c = res; res = c; // actually read res (avoids compiler warning)
		
		cout << "VEO function call mul runtime: " << endl
			 << header_string << endl
			 << "call-mul:\t" << call_mul_time.string() << endl;
		call_mul_time.to_file(filename + "call_mul_time");
	}

	local_free(data);

	return EXIT_SUCCESS;
}
