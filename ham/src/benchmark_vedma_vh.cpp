// Copyright (c) 2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <CLI/CLI11.hpp>
#include <noma/bmt/bmt.hpp>
#include <noma/debug/debug.hpp>

#include <assert.h>
#include <iostream>
#include <string>
#include <unistd.h>

// for System V SHM 
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

using namespace std;
using namespace noma::bmt;

//void errno_handler(int ret, const char * hint)
//{
//	if (ret < 0)
//	{
//		char buffer[256];
//		char* error_message = strerror_r(errno, buffer, 256);
//		std::cerr << "Errno(" << errno << ") Message for \"" << hint << "\": " << error_message << std::endl;
//		exit(-1);
//	}
//}

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
	string filename = "benchmark/offload_vedma_";
	size_t data_size = 1024*1024; // 1 MiB
	int shm_key = 1337;
	bool copy_in = false;
	bool copy_out = false;
	bool use_dma = false;
	bool use_instructions = false;
	int pinning = 0; // for cpu affinity
	
	// command line options
	// key, size, -i, -o, --ve-dma, --instructions
	CLI::App app("Supported options");
	app.add_option("--filename,-f", filename, "filename(-prefix) for results");
	app.add_option("--runs,-r", runs, "number of identical inner runs for which the average time will be computed");
	app.add_option("--warmup-runs", warmup_runs, "number of number of additional warmup runs before times are measured");
	app.add_option("--size,-s", data_size, "size of transferred data in byte (multiple of 4)");
	app.add_option("--shm-key,-k", shm_key, "key for System V SHM segment");
	app.add_flag("--copy-in,-i", copy_in, "benchmark data copy to target");
	app.add_flag("--copy-out,-o", copy_out, "benchmark data copy from target");
	app.add_flag("--dma,-d", use_dma, "Benchmark DMA for data transfer");
	app.add_flag("--instructions,-t", use_instructions, "Benchmark instructions for data transfer");
	app.add_option("--pinning,-p", pinning, "pin to cpu core, default is 0, disable with -1, no effect on VE");

	CLI11_PARSE(app, argc, argv);

	// pin host process
	if (pinning >= 0)
		set_cpu_affinity(pinning);

	// round data size to full VE pages (64 Mib)
	assert(data_size % sizeof(uint64_t) == 0);
	constexpr size_t page_size = 64 * 1024 * 1024;
	const size_t data_size_page = data_size % page_size == 0 ? data_size : (((data_size / page_size) + 1) * page_size);


	// setup SHM segment
	int shm_id = shmget(shm_key, data_size_page, IPC_CREAT | SHM_HUGETLB | S_IRWXU); 
	assert(shm_id != -1);
	
	struct shmid_ds shm_ds;
//	int err = shmctl(shm_id, IPC_STAT, &shm_ds);
	
	// attach to local memory
	void* local_shm_addr = nullptr;
	local_shm_addr = shmat(shm_id, NULL, 0);
	assert(local_shm_addr != (void *)-1);


	// wait for VE to attach (2nd one after ourselves
	do {
		shmctl(shm_id, IPC_STAT, &shm_ds);
		if (shm_ds.shm_nattch > 1)
			break;
	} while (1);


	// VE side does it's thing

	// wait for VE side to detach
	do {
		usleep(1000); // take it easy
		shmctl(shm_id, IPC_STAT, &shm_ds);
		if (shm_ds.shm_nattch == 1)
			break;
	} while (1);

	// cleanup
	ASSERT_ONLY( int err = )
	shmdt(local_shm_addr);
	assert(err == 0);
	ASSERT_ONLY( err = )
	shmctl(shm_id, IPC_RMID, NULL);
	assert(err == 0);

	return EXIT_SUCCESS;
}
