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

} // extern "C"
