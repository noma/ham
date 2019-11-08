// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_offload_main_hpp
#define ham_offload_main_hpp

namespace ham {
namespace offload {

int ham_main(int* argc_ptr, char** argv_ptr[]);

} // namespace offload
} // namespace ham

// We need to start the main programme provided by the user with some pre- and
// post-processing, so the user's main is renamed to ham_user_main using the
// preprocessor. We also inject a new main that calls the ham_main().
// This indirection is needed because ham_main() is inside a library.

#ifdef HAM_COMM_VEO_VE // vector engine
	// rename main for VE library build
	// TODO: find a solution against C++ name mangling, the one below does not work because of the int before main ;-)
	//#define main extern "C" { lib_main(int argc, char* argv[]) { return ham::offload::ham_main(&argc, &argv); } } int ham_user_main
	#define main lib_main(int argc, char* argv[]) { return ham::offload::ham_main(&argc, &argv); } int ham_user_main
#else
	#define main main(int argc, char* argv[]) { return ham::offload::ham_main(&argc, &argv); } int ham_user_main
#endif

#endif // ham_offload_main_hpp
