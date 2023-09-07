/**
 * Copyright (C) 2023 Carnegie Mellon University
 *
 * This file is part of the Mixnet course project developed for
 * the Computer Networks course (15-441/641) taught at Carnegie
 * Mellon University.
 *
 * No part of the Mixnet project may be copied and/or distributed
 * without the express permission of the 15-441/641 course staff.
 */
#ifndef TESTING_COMMON_MACROS_H_
#define TESTING_COMMON_MACROS_H_

#include "graph.h"
#include "testcase.h"

#include "framework/orchestrator.h"

#include <unistd.h>

/**
 * Collection of helpful headers, macros, functions, and typedefs.
 * Note: This header should only be included in source (i.e., non-
 * header) files to avoid polluting the global namespace.
 */
using namespace testing;
using namespace framework;

// Macros
#define DIE_ON_ERROR(x)                             \
    error_ = x;                                     \
    if (error_ != framework::error_code::NONE) {    \
        return error_;                              \
    }

#endif // TESTING_COMMON_MACROS_H_
