// Copyright (c) 2016-2017 Matthias Noack (ma.noack.pr@gmail.com)
//
// See accompanying file LICENSE and README for further information.

/**
 * @file
 * This file contains macros for debug statements (i.e. output).
 * Usage: Write "DEBUG_ONLY( statement )" instead of "statement".
 * It uses the C++ standard defined NDEBUG to switch on/off the statements.
 */

#ifndef noma_debug_debug_hpp
#define noma_debug_debug_hpp

#ifndef NDEBUG
	#define DEBUG_ONLY(expr) expr
	// same as above but more expressive for code that is only needed by an assert,
	// which would lead to some "unused variable" warning when compiled in release mode
	#define ASSERT_ONLY(expr) expr
#else
	#define DEBUG_ONLY(expr) ;
	#define ASSERT_ONLY(expr) ;
#endif

#endif // noma_debug_debug_hpp
