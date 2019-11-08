// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_misc_traits_hpp
#define ham_misc_traits_hpp

#include <type_traits>

namespace ham {

/**
 * NOTES:
 * Things that should be safe:
 *     migratable<some_ham_ptr<T>>
 * Things that should be considered un-safe:
 *     T*
 *     T&
 *     all complex types
 * Things that are safe with a special migratable:
 *     T& const (not writable, can be copied and passed into the call, but of course is not kept consistent )
 *     T* const (see above)
 *     T&& (check how to implement move semantics per RMI)
 */

/**
 * Trait to flag functor types as potentially blocking (default) or non-blocking. The latter allows for optimisations, i.e. a special execution policy.
 */
template<typename First, typename... Pars>
struct is_blocking {
	static constexpr bool value = is_blocking<First>::value && is_blocking<Pars...>::value;
};

template<typename T>
struct is_blocking<T> {
	static constexpr bool value = true; // NOTE: default is true (pessimistic assumption)
};

/**
 * Trait to flag data types as bitwise, flat copyable. Can be used to produce warnings when generating functors whose remote use might fail.
 */
template<typename First, typename... Pars>
struct is_bitwise_copyable {
	static constexpr bool value = is_bitwise_copyable<First>::value && is_bitwise_copyable<Pars...>::value;
};

template<typename T>
struct is_bitwise_copyable<T> {
	static constexpr bool value = std::is_fundamental<T>::value; // for any type outside a migratable wrapper, the safeness is pessimistically assumed to be false if it is not a fundamental type
	// cout << "is_safe<void>: " << is_safe<void>::value << std::endl;
};

} // namespace ham

#endif // ham_misc_traits_hpp
