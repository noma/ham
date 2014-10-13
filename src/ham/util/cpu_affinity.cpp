// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/util/cpu_affinity.hpp"

#include <sched.h>

#include "ham/util/debug.hpp"
#include "ham/util/log.hpp"

namespace ham {
namespace util {

// TODO(improvement): check portability of this approach
void set_cpu_affinity(int core) // NOTE: default argument set in header
{
		HAM_DEBUG( HAM_LOG << "Setting CPU affinity to: " << core << std::endl; )
		cpu_set_t mask;
		CPU_ZERO(&mask);
		CPU_SET(core, &mask);

		if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
		{
			HAM_LOG << "WARNING: Could not set CPU affinity, continuing..." << std::endl;
		}
}

} // namespace util
} // namespace ham

