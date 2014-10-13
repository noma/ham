// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload/offload.hpp"

namespace ham {
namespace offload {

node_t this_node()
{
	return runtime::instance().communicator().this_node();
}

size_t num_nodes()
{
	return runtime::instance().communicator().num_nodes();
}

bool is_host()
{
	return runtime::instance().communicator().is_host();
}

bool is_host(node_t node)
{
	return runtime::instance().communicator().is_host(node);
}

const node_descriptor& get_node_description(node_t node)
{
	return runtime::instance().communicator().get_node_description(node);
}

} // namespace offload
} // namespace ham

