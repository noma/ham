// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_util_at_end_of_scope_do_hpp
#define ham_util_at_end_of_scope_do_hpp

namespace ham {
namespace util {

/**
 * Helper template to execute code after the last statement of a scope 
 * (e.g. after return) by wrapping it into a destructor.
 */
template<typename T>
class at_end_of_scope_functor
{
public:
	at_end_of_scope_functor(T callable) : callable(callable) {}

	~at_end_of_scope_functor()
	{
		callable();
	}

private:
	T callable;
};

template<typename T>
auto at_end_of_scope_do(T callable) -> at_end_of_scope_functor<T>
{
	return at_end_of_scope_functor<T>(callable);
}

} // namespace util
} // namespace ham

#endif // ham_util_at_end_of_scope_do_h
