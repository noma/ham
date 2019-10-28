// Copyright (c) 2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <CLI/CLI11.hpp>
#include <noma/bmt/bmt.hpp>

#include <assert.h>
#include <iostream>
#include <string>
#include <string.h>
#include <sys/mman.h>

extern "C" {
	#include <vhshm.h>
	#include <vedma.h>
}

// https://github.com/efocht/vh-ve-shm
extern "C"{
	
static inline void ve_inst_fenceSF(void)
{
	asm ("fencem 1":::"memory");
}

static inline void ve_inst_fenceLF(void)
{
	asm ("fencem 2":::"memory");
}

static inline uint64_t ve_inst_lhm(void *vehva)
{
    uint64_t ret;
	asm ("lhm.l %0,0(%1)":"=r"(ret):"r"(vehva));
	return ret;
}

static inline void ve_inst_shm(void *vehva, uint64_t value)
{
	asm ("shm.l %0,0(%1)"::"r"(value),"r"(vehva));
}

} // extern "C"

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
	size_t dma_chunk_size = 64 * 1024 * 1024; // 64 MiB one page, < 128 MiB (!)
	bool use_instructions = false;
	int pinning = 0; // for cpu affinity, NOTE: no effect on VE side

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
	app.add_option("--dma-chunk-size", dma_chunk_size, "chunk size used for DMA");
	app.add_flag("--instructions,-t", use_instructions, "Benchmark instructions for data transfer");
	app.add_option("--pinning,-p", pinning, "pin to cpu core, default is 0, disable with -1, no effect on VE");

	CLI11_PARSE(app, argc, argv);

	// round data size to full VE pages (64 Mib)
	assert(data_size % sizeof(uint64_t) == 0);
	constexpr size_t page_size = 64 * 1024 * 1024;
	const size_t data_size_page = data_size % page_size == 0 ? data_size : (((data_size / page_size) + 1) * page_size);

	assert(dma_chunk_size < (128 * 1024 * 1024)); // less than (excluding equal!) 128 MiB

	// SHM setup
	// get VH shm segment by key
	int shm_id = vh_shmget(shm_key, data_size_page, SHM_HUGETLB); 
	assert(shm_id != -1);

	uint64_t remote_shm_vehva = 0;
	void *remote_shm_addr = nullptr;

	// attach shm segment
	remote_shm_addr = vh_shmat(shm_id, nullptr, 0, (void **)&remote_shm_vehva);
	
	if (remote_shm_addr == nullptr) {
		std::cerr << "VE: (remote_shm_addr == nullptr) " << std::endl;
		return 3;
	}
	if (remote_shm_vehva == (uint64_t)-1) {
		std::cerr << "VE: (remote_shm_vehva == -1) " << std::endl;
		return 4;
	}

	// init DMA
	int err = ve_dma_init();
	if (err)
		std::cerr << "VE: Failed to initialize DMA" << std::endl;

	// allocate local memory
	void *local_buffer_addr = nullptr;
	uint64_t local_buffer_vehva = 0;

	local_buffer_addr = mmap(nullptr, data_size_page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_64MB, -1, 0);
	if (local_buffer_addr == nullptr) {
		std::cerr << "VE: (local_buffer_addr == nullptr) " << std::endl;
		return 5;
	}

	// register for DMA	
	local_buffer_vehva = ve_register_mem_to_dmaatb(local_buffer_addr, data_size_page);

	if (local_buffer_vehva == (uint64_t)-1) {
		std::cerr << "VE: local_buffer_vehva == -1 " << std::endl;
		return 6;
	}

	// for output table
	std::string header_string = statistics::header_string(true);
	std::string header_string_data = statistics::header_string(true) + "\tdata_size";

	// NOTE: copy in means VH-->VE
	if (copy_in)
	{
		statistics copy_in_time(runs, warmup_runs);

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			timer clock;
			// copy VH to VE
			if (use_dma)
			{
				size_t chunks = data_size / dma_chunk_size;
				size_t remainder = data_size % dma_chunk_size;
				int err = 0;
				for (size_t i = 0; i < chunks; ++i) {
					uint64_t offset = i * dma_chunk_size;
					ve_dma_post_wait(local_buffer_vehva + offset, remote_shm_vehva + offset, dma_chunk_size); // dst (VE), src (VH), size
					assert(err == 0);
				}
				if (remainder > 0) {
					uint64_t offset = chunks * dma_chunk_size;
					ve_dma_post_wait(local_buffer_vehva + offset, remote_shm_vehva + offset, remainder); // dst (VE), src (VH), size
					assert(err == 0);
				}
					
			}
			else if (use_instructions) 
			{
				size_t words = data_size / sizeof(uint64_t);
				uint64_t* local_buffer_words = (uint64_t*)local_buffer_addr;
				// chunkwise loads from VH to VE
				for (size_t i = 0; i < words; ++i)
					local_buffer_words[i] = ve_inst_lhm((void *)((uint64_t)remote_shm_vehva + i * sizeof(uint64_t)));
				ve_inst_fenceLF(); // make sure the loads above have been completed before the next iteration 
			}
			else 
			{
				assert(false);
			}
			copy_in_time.add(clock);
		}

		cout << "VEO copy-in time: " << endl
			 << header_string_data << endl
			 << "copy-in:\t" << copy_in_time.string() << "\t" << data_size << endl;
		copy_in_time.to_file(filename + "copy_in_time");
	}

	// NOTE: copy in means VE-->VH
	if (copy_out)
	{
		statistics copy_out_time(runs, warmup_runs);

		for (size_t i = 0; i < (runs + warmup_runs); ++i)
		{
			timer clock;
			// copy VE to VH
			if (use_dma)
			{
				size_t chunks = data_size / dma_chunk_size;
				size_t remainder = data_size % dma_chunk_size;
				int err = 0;
				for (size_t i = 0; i < chunks; ++i) {
					uint64_t offset = i * dma_chunk_size;
					ve_dma_post_wait(remote_shm_vehva + offset, local_buffer_vehva + offset, dma_chunk_size); // dst (VH), src (VE), size
					assert(err == 0);
				}
				if (remainder > 0) {
					uint64_t offset = chunks * dma_chunk_size;
					ve_dma_post_wait(remote_shm_vehva + offset, local_buffer_vehva + offset, remainder); // dst (VH), src (VE), size
					assert(err == 0);
				}			
			}
			else if (use_instructions) 
			{
				size_t words = data_size / sizeof(uint64_t);
				uint64_t* local_buffer_words = (uint64_t*)local_buffer_addr;
				// chunkwise stores from VE to VH
				for (size_t i = 0; i < words; ++i)
					ve_inst_shm((void *)((uint64_t)remote_shm_vehva + i * sizeof(uint64_t)), local_buffer_words[i]);
				ve_inst_fenceSF(); // make sure the stores above have been completed before the next iteration 
			}
			else 
			{
				assert(false);
			}
			copy_out_time.add(clock);
		}

		cout << "VEO copy-out time: " << endl
			 << header_string_data << endl
			 << "copy-out:\t" << copy_out_time.string() << "\t" << data_size << endl;
		copy_out_time.to_file(filename + "copy_out_time");
	}


	// cleanup
	err = ve_unregister_mem_from_dmaatb(local_buffer_vehva);
	if (err)
		std::cerr << "VE: Failed to unregister local buffer from DMAATB" << std::endl;

	//
	// detach VH sysV shm segment
	//
	err = vh_shmdt(remote_shm_addr);
	if (err)
		std::cerr << "VE: Failed to detach from VH sysV shm" << std::endl;
	
	
	return EXIT_SUCCESS;
}
