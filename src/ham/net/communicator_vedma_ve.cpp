// Copyright (c) 2013-2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/net/communicator.hpp"

// VEO usable interface for initialisation
extern "C" {

uint64_t ham_comm_veo_ve_shm_key(uint64_t key)
{
	static uint64_t current_key = 0;
	
	if (key != 0) {
		current_key = key;
		return 0;
	} else { // key == 0
		return current_key;
	}
}

uint64_t ham_comm_veo_ve_shm_size(uint64_t size)
{
	static uint64_t current_size = 0;
	
	if (size != 0) {
		current_size = size;
		return 0;
	} else { // size == 0
		return current_size;
	}
}


void reset_flags(uint64_t flags_addr)
{
	using ::ham::net::cache_line_buffer;
	cache_line_buffer fill_value;
	cache_line_buffer* fill_value_ptr = &fill_value;
	cache_line_buffer* flags_ptr = reinterpret_cast<cache_line_buffer*>(flags_addr);
	// null fill_value
	std::fill(reinterpret_cast<unsigned char*>(fill_value_ptr), reinterpret_cast<unsigned char*>(fill_value_ptr) + sizeof(cache_line_buffer), 0);
	// set to flag false
	*reinterpret_cast<size_t*>(fill_value_ptr) = ::ham::net::communicator::FLAG_FALSE;
	// set all flags to fill_value
	std::fill(flags_ptr, flags_ptr + ::ham::constants::MSG_BUFFERS, fill_value);
}

// setter/getter hybrid function, if addr is 0, current value is returned, if addr is != 0 value is updated, and 0 returned
uint64_t ham_comm_veo_ve_local_buffers_addr(uint64_t addr)
{
	static uint64_t current_addr = 0;
	
	if (addr != 0) {
		current_addr = addr;
		return 0;
	} else { // addr == 0
		return current_addr;
	}
}

uint64_t ham_comm_veo_ve_local_flags_addr(uint64_t addr)
{
	static uint64_t current_addr = 0;
	
	if (addr != 0) {
		current_addr = addr;
		reset_flags(current_addr);
		return 0;
	} else { // addr == 0
		return current_addr;
	}
}

uint64_t ham_comm_veo_ve_remote_buffers_addr(uint64_t addr)
{
	static uint64_t current_addr = 0;
	
	if (addr != 0) {
		current_addr = addr;
		return 0;
	} else { // addr == 0
		return current_addr;
	}
}

uint64_t ham_comm_veo_ve_remote_flags_addr(uint64_t addr)
{
	static uint64_t current_addr = 0;
	
	if (addr != 0) {
		current_addr = addr;
		reset_flags(current_addr);		
		return 0;
	} else { // addr == 0
		return current_addr;
	}
}

uint64_t ham_comm_veo_ve_address(uint64_t address, uint64_t set)
{
	static uint64_t current_address = 0;
	
	if (set) {
		current_address = address;
		return 0;
	} else { // get
		return current_address;
	}
}

uint64_t ham_comm_veo_ve_process_count(uint64_t process_count, uint64_t set)
{
	static uint64_t current_process_count = 0;
	
	if (set) {
		current_process_count = process_count;
		return 0;
	} else { // get
		return current_process_count;
	}
}


uint64_t ham_comm_veo_ve_host_address(uint64_t host_address, uint64_t set)
{
	static uint64_t current_host_address = 0;
	
	if (set) {
		current_host_address = host_address;
		return 0;
	} else { // get
		return current_host_address;
	}
}



} // extern "C"

