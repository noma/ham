// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_functor_function_hpp
#define ham_functor_function_hpp

#include <tuple>
#include <type_traits>

#include <boost/preprocessor/facilities/overload.hpp>

#include "ham/misc/traits.hpp"
#include "ham/util/make_index_sequence.hpp"
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
		return evaluation_helper(typename util::make_index_sequence<sizeof...(Pars)>::type());
	}
private:
	// this helper is needed to extract the actual integer sequence from seq by template argument deduction
	template<std::size_t... S>
	Result evaluation_helper(util::index_sequence<S...>) const
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
// NOTE: violates the standard, as '...' must get at least one argument, hence the BOOST_PP_OVERLOAD work-around
//#define f2f(function_ptr, ...) ham::function<decltype(function_ptr), function_ptr>(__VA_ARGS__)

// Macro for the function template instantiation without argument list
#define F2F_TEMPLATE(function_ptr)    ham::function<decltype(function_ptr), function_ptr>
// Macro that adds an empty argument list to F2F_TEMPLATE
#define F2F_ZERO_ARGS(function_ptr)   F2F_TEMPLATE(function_ptr)()
// Macro that adds a non-empty argument list to F2F_TEMPLATE using variadic macros again
#define F2F_N_ARGS(function_ptr, ...) F2F_TEMPLATE(function_ptr)(__VA_ARGS__)

// Macros to be used with BOOST_PP_OVERLOAD for F2F_<N> arguments (including function ptr)
#define F2F_1(function_ptr) F2F_ZERO_ARGS(function_ptr)
#define F2F_2(...)          F2F_N_ARGS(__VA_ARGS__)
#define F2F_3(...)          F2F_N_ARGS(__VA_ARGS__)
#define F2F_4(...)          F2F_N_ARGS(__VA_ARGS__)
#define F2F_5(...)          F2F_N_ARGS(__VA_ARGS__)
#define F2F_6(...)          F2F_N_ARGS(__VA_ARGS__)
#define F2F_7(...)          F2F_N_ARGS(__VA_ARGS__)
#define F2F_8(...)          F2F_N_ARGS(__VA_ARGS__)
#define F2F_9(...)          F2F_N_ARGS(__VA_ARGS__)
#define F2F_10(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_11(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_12(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_13(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_14(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_15(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_16(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_17(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_18(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_19(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_20(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_21(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_22(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_23(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_24(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_25(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_26(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_27(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_28(...)         F2F_N_ARGS(__VA_ARGS__)
#define F2F_29(...)         F2F_N_ARGS(__VA_ARGS__)

#define f2f(...) BOOST_PP_OVERLOAD(F2F_,__VA_ARGS__)(__VA_ARGS__)

/* NOTE: a generate function using type deduction is not an option
 *
 *     template<typename F_PTR_T, typename... Args>
 *     constexpr auto f2f(F_PTR_T&& function_ptr, Args&&... args) -> decltype(ham::function<F_PTR_T, fptr>(args...)) { ... }
 *
 * as function_ptr 'is not a constant expression' (the function could be called with a runtime value, even if it is not).
 */

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
