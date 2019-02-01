// Copyright (c) 2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload.hpp"
#include <iostream>
#include <string>
#include <string.h>

using namespace ham;

// compile on VE side only
#if defined(HAM_COMM_VH)
	#include <sys/ipc.h>
	#include <sys/shm.h>
#elif defined(HAM_COMM_VE)
extern "C" {
	#include <vhshm.h>
}
#endif

uint64_t hello(key_t shm_key, size_t size) 
{
#ifdef HAM_COMM_VH
std::cout << "hello: HAM_COMM_VH" << std::endl;
#endif
#ifdef HAM_COMM_VE
std::cout << "hello: HAM_COMM_VE" << std::endl;
#endif

// compile on VE side only
#ifdef HAM_COMM_VE
	// https://veos-sxarr-nec.github.io/libsysve/group__vhshm.html#gad8fc6108e95dab9c18c4e56f610f3f03
	// https://linux.die.net/man/2/shmat
	// vh_shmget (key_t key, size_t size, int shmflag)

// TODO:
// #include <vedma.h>
//	if (ve_dma_init() != 0)
//		std::cout << "hello: (ve_dma_init != 0) " << std::endl;
//	

//	// try 2 MiB page, default on VE is 64 MiB, on VH 4 KiB, play around a bit
//	size_t size = 2 * 1024 * 1024;

//	// create shared memory segment, get id back, fail if segment already exists
//	int shm_key = vh_shmgget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL); 
	if (shm_key == -1)
		std::cout << "hello: (shm_key != -1) " << std::endl;

	// attach shared memory VH address space and register it to DMAATB
	void* remote_addr = nullptr;
	void* local_addr = vh_shmat(shm_key, NULL, 0, &remote_addr);

	if (local_addr == nullptr)
		std::cout << "hello: (local_addr == nullptr) " << std::endl;
	if (remote_addr == nullptr)
		std::cout << "hello: (remote_addr == nullptr) " << std::endl;

	std::cout << "hello: pre strcpy " << std::endl;
	// TODO: this fails, remote_addr is null
	// do some data access (without DMA)
	char msg[] = "DMA successfull! :-)";
 	strcpy((char*)local_addr, msg);
 	std::cout << "hello: post strcpy " << std::endl;
 	
 	// TODO: use VE DMA
	std::cout << "hello: pre vh_shmdt " << std::endl;
	// detach
	int err = vh_shmdt(local_addr);
	assert(err == 0);
	
//	return msg.size();
#endif

	std::cout << "hello: returning " << std::endl;
	return 0;
}

int main(int argc, char* argv[])
{
	HAM_UNUSED_VAR(argc);
	HAM_UNUSED_VAR(argv);

#ifdef HAM_COMM_VH
std::cout << "hello: HAM_COMM_VH" << std::endl;
#endif
#ifdef HAM_COMM_VE
std::cout << "hello: HAM_COMM_VE" << std::endl;
#endif

// NOTE: all f2f's must be visible in the VE to trigger template code generation for active message handlers
	offload::node_t target = 1; // we simply use the first device/node

	// https://veos-sxarr-nec.github.io/libsysve/group__vhshm.html#gad8fc6108e95dab9c18c4e56f610f3f03
	// https://linux.die.net/man/2/shmat
	// vh_shmget (key_t key, size_t size, int shmflag)

	// try 2 MiB page, default on VE is 64 MiB, on VH 4 KiB, play around a bit
	size_t size = 2 * 1024 * 1024;

	// create shared memory segment, get id back, fail if segment already exists

	int shm_key = -1;
// compile on VH only
#if defined(HAM_COMM_VH)
	shm_key = shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | SHM_HUGETLB); 
	assert(shm_key != -1);
#endif

	// attach shared memory to VH address space
	void* local_addr = nullptr;
#if defined(HAM_COMM_VE)
	local_addr = shmat(shm_key, NULL, 0);
	assert(local_addr != nullptr);
#endif

	// call hello on VE with key and size
	std::cout << "Calling hello" << std::endl;
	auto hello_future = offload::async(target, f2f(&hello, shm_key, size));
	std::cout << "Received hello return: " << hello_future.get() << std::endl;

	// read a msg from the DMA buffer and output it
	std::string msg(reinterpret_cast<char*>(local_addr));
	std::cout << "Hello from target: " << msg << std::endl;

	// TODO: use shm DMA 

	// detach
#if defined(HAM_COMM_VH)
	int err = shmdt(local_addr);
	assert(err == 0);
#endif

	return 0;
}

