// Copyright (c) 2013-2018 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

/**
 * @file
 * This file contains macros for debug statements (i.e. output).
 * Usage: Write "HAM_DEBUG( statement )" instead of "statement" and define HAM_DEBUG_ON to switch the statements on.
 */

#ifndef ham_util_debug_hpp
#define ham_util_debug_hpp

#ifdef HAM_DEBUG_ON
	#define HAM_DEBUG(expr) expr
#else
	#define HAM_DEBUG(expr) ;
#endif

// surpress compiler warnings
#define HAM_UNUSED_VAR(var) (void)(var)

#endif // ham_util_debug_hpp
