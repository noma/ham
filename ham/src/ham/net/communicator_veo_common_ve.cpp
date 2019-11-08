// Copyright (c) 2013-2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/net/communicator.hpp"

// VEO usable interface for initialisation
extern "C" {

// TODO: cleanup
//uint64_t ham_comm_veo_ve_get_node_descriptor()
//{
//std::cerr << "ham_comm_veo_ve_get_node_descriptor" << std::endl;

//	static ::ham::net::node_descriptor local_node;
////	local_node.name_[0] = 't';
////	local_node.name_[1] = '\0';
//	gethostname(local_node.name_, local_node.name_length_);

//	return reinterpret_cast<uint64_t>(&local_node);
//	//return reinterpret_cast<uint64_t>(::ham::net::communicator::instance().veo_ve_get_host_address_descriptor());
//}

//uint64_t ham_comm_veo_ve_set_descriptor_addr(uint64_t addr)
//{
//std::cerr << "ham_comm_ve_set_descriptor_addr 0: comm: " << &(::ham::net::communicator::instance()) << ", got " << addr << std::endl;
//	static ::ham::net::node_descriptor local_node;
//	local_node.name_[0] = 't';
//	local_node.name_[1] = '\0';
//
//	//copy local_node to addr (pre-allocated from host)
//	memcpy((void*)addr, &local_node, sizeof(::ham::net::node_descriptor));

////	::ham::net::communicator::instance().veo_ve_set_node_descriptor(reinterpret_cast<void*>(addr));
//std::cerr << "ham_comm_ve_set_descriptor_addr 1" << std::endl;
//	return 0;
//}

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

uint64_t ham_comm_veo_ve_device_number(uint64_t device_number, uint64_t set)
{
	static uint64_t current_device_number = 0;

	if (set) {
		current_device_number = device_number;
		return 0;
	} else { // get
		return current_device_number;
	}
}

} // extern "C"

