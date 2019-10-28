// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/net/communicator.hpp"

namespace ham {
namespace net {

node_t this_node()
{
	return communicator::this_node();
}	

} // namespace net
} // namespace ham


