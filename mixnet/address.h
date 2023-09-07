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
#ifndef MIXNET_ADDRESS_H_
#define MIXNET_ADDRESS_H_

#include <stdint.h>

// Constant parameters
#define INVALID_MIXADDR ((uint16_t) -1)

/**
 * Represents the address of a node on the Mixnet network. Each node is
 * identified by a unique, 16-bit address (somewhat like a MAC address).
 */
typedef uint16_t mixnet_address;

#endif // MIXNET_ADDRESS_H_
