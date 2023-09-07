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
#ifndef MIXNET_CONFIG_H_
#define MIXNET_CONFIG_H_

#include "address.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Default parameters
#define DEFAULT_ROOT_HELLO_INTERVAL_MS  (2000)
#define DEFAULT_REELECTION_INTERVAL_MS  (20000)

// Node configuration
struct mixnet_node_config {
    mixnet_address node_addr;           // Mixnet address of this node
    uint16_t num_neighbors;             // This node's total neighbor count

    // STP parameters
    uint32_t root_hello_interval_ms;    // Time (in ms) between 'hello' messages
    uint32_t reelection_interval_ms;    // Time (in ms) before starting reelection

    // Routing parameters
    bool do_random_routing;             // Whether this node performs random routing
    uint16_t mixing_factor;             // Exact number of (non-control) packets to mix
    uint16_t *link_costs;               // Per-neighbor routing costs, in range [0, 2^16)

    #ifdef __cplusplus
    mixnet_node_config() :
        node_addr(INVALID_MIXADDR), num_neighbors(0),
        root_hello_interval_ms(DEFAULT_ROOT_HELLO_INTERVAL_MS),
        reelection_interval_ms(DEFAULT_REELECTION_INTERVAL_MS),
        do_random_routing(false), mixing_factor(1), link_costs(nullptr) {}
    #endif
};

#ifdef __cplusplus
}
#endif

#endif // MIXNET_CONFIG_H_
