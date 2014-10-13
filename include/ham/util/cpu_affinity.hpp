// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_util_cpu_affinity_hpp
#define ham_util_cpu_affinity_hpp

namespace ham {
namespace util {

void set_cpu_affinity(int core = 0);

} // namespace util
} // namespace ham


#endif // ham_util_cpu_affinity_hpp
