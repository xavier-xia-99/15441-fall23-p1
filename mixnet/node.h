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
#ifndef MIXNET_NODE_H_
#define MIXNET_NODE_H_

#include "address.h"
#include "config.h"
#include "packet.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
//structure of our Node 
struct Node {
    uint16_t num_neighbors;  // number of neighbors
    mixnet_address *neighbors_addy; // Array of neighbors (mixnet address)
    uint16_t *neighbors_cost;
    bool *neighbors_blocked; // Block of neighbors (false is unblocked)

    uint16_t total_nodes;
    bool random;
    bool done;

    mixnet_address* global_best_path[1 << 16][1 << 8]; // List of [Path := List of mixnet_address
    mixnet_lsa_link_params* graph[1 << 16][1 << 8]; // 2^16 nodes, 2^8 List : []]

    mixnet_address root_addr; // root addr
    mixnet_address my_addr; // self addr
    mixnet_address next_hop; // Next hop
    uint16_t path_len;

    bool visited[1<<16];
    uint16_t distance[1<<16];
    mixnet_address prev_neighbor[1<<16];
    uint16_t visitedCount;
    uint16_t smallestindex;

    uint16_t mixingfactor;
    mixnet_packet* queue[1<<16];
    uint16_t queue_size;

    bool random_routing;
    uint16_t stp_unused;
    bool* neighbors_unused;
    
    uint16_t stp_received;
    uint16_t stp_sent;
};

void run_node(void *const handle,
              volatile bool *const keep_running,
              const struct mixnet_node_config c);

#ifdef __cplusplus
}
#endif

#endif // MIXNET_NODE_H_
