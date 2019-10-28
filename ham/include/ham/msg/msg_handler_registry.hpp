// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

/**
 * @file
 * This file introduces a layer of indirection to allow multiple
 * compile-time configurable message handler registry implementations.
 * (E.g. a trivial one that directly uses addresses without any conversion,
 * a very complex one that uses some heavy weight, maybe even versioned,
 * identifiers for maximal compatibility, and of course, anything in between).
 */

#ifndef ham_msg_msg_handler_registry_hpp
#define ham_msg_msg_handler_registry_hpp

#include "ham/msg/msg_handler_registry_abi.hpp"
// NOTE: include other implementations here

namespace ham {
namespace msg {

// NOTE: this sets the default, maybe use defines here for simple compile time configuration
using msg_handler_registry = msg_handler_registry_abi;

} // namespace msg
} // namespace ham

#endif // ham_msg_msg_handler_registry_hpp
