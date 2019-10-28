// Copyright (c) 2013-2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_net_communicator_veo_common_ve_hpp
#define ham_net_communicator_veo_common_ve_hpp

#include "ham/net/communicator_veo_base.hpp"

// VEO C-linkage interface
extern "C" {

// TODO: cleanup
// TODO: exchange node descriptors
//uint64_t ham_comm_veo_ve_get_node_descriptor();
//uint64_t ham_comm_veo_ve_set_descriptor_addr(uint64_t addr);

uint64_t ham_comm_veo_ve_local_buffers_addr(uint64_t addr);
uint64_t ham_comm_veo_ve_local_flags_addr(uint64_t addr);
uint64_t ham_comm_veo_ve_remote_buffers_addr(uint64_t addr);
uint64_t ham_comm_veo_ve_remote_flags_addr(uint64_t addr);

uint64_t ham_comm_veo_ve_address(uint64_t address, uint64_t set);
uint64_t ham_comm_veo_ve_process_count(uint64_t process_count, uint64_t set);
uint64_t ham_comm_veo_ve_host_address(uint64_t host_address, uint64_t set);

}

#endif // ham_net_communicator_veo_common_ve_hpp
