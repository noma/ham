// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_offload_hpp
#define ham_offload_hpp

// configure everything for offloading
#define HAM_OFFLOAD
// prepend node to log output
//#define HAM_LOG_NODE_PREFIX 

#ifdef HAM_COMM_MPI
#include <mpi.h> // must be included before stdio.h for Intel MPI
#endif

// NOTE: add everything in the right order

// heterogeneous active message layer
#include "ham/msg/active_msg.hpp"

// functors and generators
#include "ham/functor/function.hpp"
#include "ham/functor/buffer.hpp"

// offload
#include "ham/offload/offload.hpp"

// runtime
#ifdef HAM_EXPLICIT
#include "ham/offload/main_explicit.hpp"
#else
#include "ham/offload/main.hpp"
#endif

#endif // ham_offload_hpp
