// Copyright (c) 2013-2019 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_net_communicator_veo_vedma_post_hpp
#define ham_net_communicator_veo_vedma_post_hpp

namespace ham {
namespace net {

template<typename T>
buffer_ptr<T>::buffer_ptr() : buffer_ptr(nullptr, net::communicator::this_node()) { }

template<typename T>
T& buffer_ptr<T>::operator[](size_t i)
{
	assert(node_ == net::communicator::this_node());
	return ptr_[i];
}

} // namespace net
} // namespace ham

#endif // ham_net_communicator_veo_vedma_post_hpp
