// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_functor_function_hpp
#define ham_functor_function_hpp

#include <tuple>
#include <type_traits>

#include "ham/misc/traits.hpp"
#include "ham/util/make_seq.hpp"
#include "ham/misc/migratable.hpp"

namespace ham {

/**
 * Function functor used by f2f.
 * The general case is just used to yield a static_assert in case of misuse.
 */
template<typename FunctionPtrT, FunctionPtrT FunctionPtr>
class function {
	static_assert(std::is_pointer<FunctionPtrT>::value && std::is_function<typename std::remove_pointer<FunctionPtrT>::type>::value, "Template argument for FunctionPtr is not a pointer to function type. Use f2f to create a Function instance.");
};

/**
 * This function functor specialisation encapsulates a Function call and its
 * arguments. The function pointer is a template value
 * argument, and thus part of the type and not a data member. This is
 * important for network transfers.
 * The specialisation is used to ensure the template argument is an actual
 * function pointer and to deduce all needed types.
 */
template<typename Result, typename... Pars, Result (*FunctionPtr)(Pars...)>
class function<Result (*)(Pars...), FunctionPtr> {
public:
	typedef Result result_type;

	function(const function&) = default;
	function(function&&) = default;
	function& operator=(const function&) = default;
	function& operator=(function&&) = default;

	/**
	 * This ctor template forwards the arguments into a tuple of Migratables
	 * that have the actual parameter types of the function. So the conversion
	 * between the argument types (Args...) and the parameter types
	 * (Pars...) happens here (more precisely in migratable's ctor).
	 */
	template<typename... Args>
	function(Args&&... arguments)
	 : args(std::forward<Args>(arguments)...)
	{ }

	Result operator()() const
	{
		// call a helper with an integer sequence to unpack the tupel
		return evaluation_helper(typename util::make_seq<sizeof...(Pars)>::type());
	}
private:
	// this helper is needed to extract the actual integer sequence from seq by template argument deduction
	template<int... S>
	Result evaluation_helper(util::seq<S...>) const
	{
		// call the function with the unpacked tuple
		return FunctionPtr(std::get<S>(args)...);
	}

	std::tuple<migratable<Pars>...> args;
};


/**
 * Sadly, this macro is necessary to achieve the f2f syntax AND store
 * the function pointer as template value parameter.
 */
#define f2f(function_ptr, ...) ham::function<decltype(function_ptr), function_ptr>(__VA_ARGS__)


/**
 * Copyability of Function<...> is defined by the conjunction of the copyability of
 * its parameter types.
 */
template<typename Result, typename... Pars, Result (*FunctionPtr)(Pars...)>
struct is_bitwise_copyable<function<Result (*)(Pars...), FunctionPtr>> {
	static constexpr bool value = is_bitwise_copyable<migratable<Pars>..., migratable<void>>::value; // NOTE: void is used here in case Pars is empty
};

} // namespace ham

#endif // ham_functor_function_hpp

