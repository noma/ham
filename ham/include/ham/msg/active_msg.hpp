// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_msg_active_msg_hpp
#define ham_msg_active_msg_hpp

#include "ham/msg/active_msg_base.hpp"
#include "ham/msg/execution_policy.hpp"

namespace ham {
namespace msg {

/**
 * This class template is the base for all active message classes.
 * It ensures that a unique type-specific handler is generated and registered.
 */
template<class Derived, template<class> class ExecutionPolicy = default_execution_policy>
class active_msg : private active_msg_base, public ExecutionPolicy<Derived> {
protected:
	active_msg()
	: active_msg_base(handler_key_static)
	{ }

public:
	using ExecutionPolicy<Derived>::handler;
	
	/**
	 * This method is called by the message registry to re-assign the
	 * handler key after initialisation. This may or may not be necessary
	 * and depends on the actual implementation of the message registry.
	 */
	static void set_handler_key(msg_handler_registry::key_type key)
	{
		handler_key_static = key;
	}

private:
	/**
	 * Contains the key of the registered handler for each type. It is
	 * used to set active_msg_base::handler_key in constant time at message
	 * creation. Is is set by active_msg_base::init() via a stored function
	 * pointer to set_handler_key() which is stored together with the handler
	 * by register_handler().
	 */
	static msg_handler_registry::key_type handler_key_static;
};

/**
 * NOTE: The initialisation of this static member template is used to call
 * register_handler() exactly once per active-message type during static
 * member initialisation.
 * The actual value might be determined later by message_registry::init() using
 * set_handler_key()
 */
template<class Derived, template<class> class ExecutionPolicy>
msg_handler_registry::key_type active_msg<Derived, ExecutionPolicy>::handler_key_static = msg_handler_registry::register_handler<active_msg<Derived, ExecutionPolicy>>();

} // namespace msg
} // namespace ham

#endif // ham_msg_active_msg_hpp
