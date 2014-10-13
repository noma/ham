// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_misc_migratable_hpp
#define ham_misc_migratable_hpp

#include <type_traits>
#include <iostream>

#include "ham/misc/traits.hpp"

namespace ham {

template<typename T>
class migratable
{
public:
	migratable(const migratable&) = default;
	migratable(migratable&&) = default;
	migratable& operator=(const migratable&) = default;
	migratable& operator=(migratable&&) = default;

	// forward compatible arg into T's ctor
	template<typename Compatible>
	migratable(Compatible&& arg)
	 //: value(std::forward<T>(arg)) // NOTE: compatible types are allowed
	 : value(std::forward<Compatible>(arg)) // NOTE: compatible types are allowed
	{ 
//		std::cout << "migratable-ctor: " << value << std::endl;
	}

	operator const T& () const
	{
//		std::cout << "migratable-conversion: " << value << std::endl;
		return value;
	}
private:
	T value;
};

// NOTE: if no specializisation for T or migratable<T> exists, the default is T's property
template<typename T>
struct is_bitwise_copyable<migratable<T>> {
	static constexpr bool value = is_bitwise_copyable<T>::value;
//	static constexpr bool value = true;
};

} // namespace ham

#endif // ham_misc_migratable_hpp
