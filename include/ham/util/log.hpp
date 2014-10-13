// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_util_log_h
#define ham_util_log_h

#include <iostream>

#include "ham/misc/types.hpp"

namespace ham {
namespace net {

// forward declarations
node_t this_node();

} // namespace net
} // namespace ham

#ifdef HAM_LOG_NODE_PREFIX
#define HAM_LOG std::cout << ham::net::this_node() << ":\t"
#else
#define HAM_LOG std::cout
#endif
// TODO(improvement): 
// - add thread-id to prefix
// - use an actual stream object inside the namespace instead of this macro

#endif // ham_util_log_h
