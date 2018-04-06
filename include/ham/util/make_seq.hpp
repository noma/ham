// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

/**
 * @file
 * This is a helper to extract values from std::tuple.
 */

#ifndef ham_util_make_seq_hpp
#define ham_util_make_seq_hpp

namespace ham {
namespace util {

template<int... Ints>
struct seq { };

template<int N, int... Tail>
struct make_seq : make_seq<N - 1, N - 1, Tail...> { };

template<int... Tail>
struct make_seq<0, Tail...> {
	using type = seq<Tail...>;
};

} // namespace util
} // namespace ham

#endif // ham_util_make_seq_hpp
