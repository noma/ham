// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_misc_constants_hpp
#define ham_misc_constants_hpp

// defines and defaults fpr compile-time configurable values
#ifndef HAM_MESSAGE_SIZE
#define HAM_MESSAGE_SIZE 4096
#endif

namespace ham {
namespace constants {

enum net {
	MSG_SIZE = HAM_MESSAGE_SIZE,
	MSG_BUFFERS = 256,
	FLAG_SIZE = sizeof(size_t),
};

enum arch {
	CACHE_LINE_SIZE = 0x40, // 64 B
	PAGE_SIZE = 0x1000, // 4 KiB
	HUGE_PAGE_SIZE = 0x200000, // 2 MiB
};

// NOTE: change if values collide with application
enum mpi {
	DEFAULT_TAG = 0,
	RESULT_TAG = 1,
	DATA_TAG = 2,
	SYNC_TAG = 3,
};


} // namespace constants
} // namespace ham

#endif // ham_misc_constants_hpp
