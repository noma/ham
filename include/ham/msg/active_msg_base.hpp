// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_msg_active_msg_base_hpp
#define ham_msg_active_msg_base_hpp

#include "ham/msg/msg_handler_registry.hpp"

namespace ham {
namespace msg {

/**
 * This class encapsulates all the message-type independent code which is
 * needed to process a received active message. Processing starts by calling
 * active_msg::operator()(void* buffer) with a receive buffer containing the
 * message (which of course must inherit from acvite_msg_base). The necessary
 * key that references the handler for the type-dependent part is specified
 * on construction (prior to transfer).
 */
class active_msg_base {
public:
	void operator()(void* buffer) 
	{
		// get local handler address by key and call the handler with buffer
		msg_handler_registry::get_handler(handler_key)(buffer);
	}

protected:
	/**
	 * The key for the message handler registry is set on construction and thus
	 * is transferred with the message.
	 */
	active_msg_base(msg_handler_registry::key_type key)
	: handler_key(key)
	{ }

private:	
	/**
	 * Stores each message handler's key for the message handler registry.
	 * This key is used to lookup the right handler on the receiving side.
	 */
	const msg_handler_registry::key_type handler_key;
};

} // namespace msg
} // namespace ham

#endif // ham_msg_active_msg_base_hpp
