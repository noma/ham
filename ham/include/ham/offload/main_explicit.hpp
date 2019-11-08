// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_offload_main_explicit_hpp
#define ham_offload_main_explicit_hpp

namespace ham {
namespace offload {

// This must be called explicitly before you start using the HAM-Offload API, usually at the beginning of main().
// NOTE: This call blocks in all processes except the logical host process and 
bool ham_init(int* argc_ptr, char** argv_ptr[]);

// This must be called explicitly when you are done using the HAM-Offload API, usually at the end of main().
int ham_finalise();

} // namespace offload
} // namespace ham

// just a dummy for satisfying the runtime implementation
int ham_user_main(int argc, char* argv_ptr[]);

#ifdef HAM_COMM_VEO_VE // vector engine
// rename USER main for VE library build
// TODO: see main.hpp
	#define main lib_main
#endif

#endif // ham_offload_main_explicit_hpp
