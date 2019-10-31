// Copyright (c) 2015-2016 Matthias Noack <ma.noack.pr@gmail.com>
//
// See accompanying file LICENSE and README for further information.

#include "noma/memory/memory.hpp"

#include <sstream>

namespace noma {
namespace memory {

const std::string posix_memalign_error_to_string(int err)
{
	switch (err) {
		case 0:
			return "No error.";
		case EINVAL:
			return "The alignment argument was not a power of two, or was not a multiple of sizeof(void *).";
		case ENOMEM:
			return "There was insufficient memory to fulfill the allocation request.";
		default: {
			std::stringstream msg;
			msg << "Unknwon error code: " << err;
			return msg.str();
		}
	}
}

} // namespace memory
} // namespace noma
