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
#include "graph.h"

#include <algorithm>
#include <assert.h>
#include <numeric>

namespace testing {

graph::graph(const uint16_t n) : num_nodes(n) {
    topology_.resize(num_nodes); // Initialize adjacency graph
    // By default, use the node indices as their mixnet addresses
    for (uint16_t i = 0; i < n; i++) { nodes_.push_back(node(i)); }
}

graph& graph::add_edge(const half_edge src, const half_edge dst) {
    // Sanity check: Ensure indices, costs are permissible
    assert((src.id < num_nodes) && (dst.id < num_nodes));
    assert((src.cost >= 0) && (src.cost <= 1.0));
    assert((dst.cost >= 0) && (dst.cost <= 1.0));
    assert(src.id != dst.id);

    // Sanity check: Ensure no duplicate edges exist
    assert(std::find(topology_[src.id].begin(),
        topology_[src.id].end(), dst.id) ==
        topology_[src.id].end());

    // Update the set of edges and cost vectors
    nodes_[src.id].add_link_cost(src.cost);
    nodes_[dst.id].add_link_cost(dst.cost);
    topology_[src.id].push_back(dst.id);
    topology_[dst.id].push_back(src.id);

    return *this;
}

void graph::set_mixaddrs(const std::vector<mixnet_address>& m) {
    assert(m.size() == num_nodes); // Sanity check
    for (uint16_t i = 0; i < num_nodes; i++) {
        nodes_[i].set_mixaddr(m[i]);
    }
}

void graph::generate_topology(const type t,
                std::vector<uint16_t> idxs) {
    if (num_nodes < 2) { return; }
    if (idxs.empty()) { idxs.resize(num_nodes);
                        std::iota(idxs.begin(), idxs.end(), 0); }
    switch (t) {
    // Line topology
    case type::LINE: {
        for (uint16_t i = 0; i < (idxs.size() - 1); i++) {
            add_edge(idxs[i], idxs[i + 1]);
        }
    } break;

    // Ring topology
    case type::RING: {
        assert(idxs.size() > 2); // Too few vertices
        for (uint16_t i = 0; i < idxs.size(); i++) {
            add_edge(idxs[i], idxs[(i + 1) % idxs.size()]);
        }
    } break;

    // Full mesh topology
    case type::FULL_MESH: {
        for (uint16_t i = 0; i < idxs.size(); i++) {
            for (uint16_t j = (i + 1); j < idxs.size(); j++) {
                add_edge(idxs[i], idxs[j]);
            }
        }
    } break;

    // Star topology
    case type::STAR: {
        for (uint16_t i = 1; i < idxs.size(); i++) {
            add_edge(idxs[0], idxs[i]);
        }
    } break;

    // Unimplemented topology
    default: { assert(false); } break;
    } // switch
}

} // namespace testing
