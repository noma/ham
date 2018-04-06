// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

/**
 * @file
 * This is a helper to extract values from std::tuple.
 */

#ifndef ham_util_make_index_sequence_hpp
#define ham_util_make_index_sequence_hpp

#include <cstddef> // size_t

namespace ham {
namespace util {

template<std::size_t... Ints>
struct index_sequence { };

template<std::size_t N, std::size_t... Tail>
struct make_index_sequence : make_index_sequence<N - 1, N - 1, Tail...> { };

template<std::size_t... Tail>
struct make_index_sequence<0, Tail...> {
	using type = index_sequence<Tail...>;
};

} // namespace util
} // namespace ham

#endif // ham_util_make_index_sequence_hpp
