// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

/**
 * @file
 * The ExecutionPolicies contain the actual message handlers that are
 * registered at the message handler registry.
 * Different policies allow different handling for the same message type.
 */

#ifndef ham_msg_execution_policy_hpp
#define ham_msg_execution_policy_hpp

#include <string>

#include "ham/util/debug.hpp"
#include "ham/util/log.hpp"

namespace ham {
namespace msg {

template<class Derived>
class execution_policy_direct {
protected:
	static void handler(void* buffer) {
		HAM_DEBUG( HAM_LOG << "execution_policy_direct::handler()" << std::endl; )
		// downcast the buffer to the actual message type (Derived)
		Derived& functor = *reinterpret_cast<Derived*>(buffer);
		// call the message as functor
		functor();
	}
};

/**
 * Defines a default execution policy.
 */
template<class Derived>
using default_execution_policy = execution_policy_direct<Derived>;

} // namespace msg
} // namespace ham

#endif // ham_msg_execution_policy_hpp
