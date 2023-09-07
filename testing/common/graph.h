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
#ifndef TESTING_COMMON_GRAPH_H_
#define TESTING_COMMON_GRAPH_H_

#include "mixnet/address.h"

#include <vector>

namespace testing {

// Helper macros
#define DISALLOW_COPY_AND_ASSIGN(TypeName)                  \
    TypeName(const TypeName&) = delete;                     \
    void operator=(const TypeName&) = delete

/**
 * Represents a Mixnet graph.
 */
class graph final {
public:
    // Typedefs
    typedef std::vector<uint16_t> adjacency_list;
    typedef std::vector<adjacency_list> adjacency_graph;

    // Maximum node count
    const uint16_t num_nodes;

    /**
     * Represents a half-edge in the Mixnet graph.
     */
    struct half_edge {
        uint16_t id = -1;                           // Node ID
        uint16_t cost = 1;                          // Link cost for routing

        half_edge(const uint16_t id) : id(id) {} // Allow implicit typecast
        half_edge(const uint16_t id, const uint16_t cost) : id(id), cost(cost) {}
    };

    /**
     * Represents a node in the Mixnet graph.
     */
    class node final {
    private:
        // Node configuration
        uint16_t mixing_factor_ = 1;                // Default: 1
        bool do_random_routing_ = false;            // Default: false
        std::vector<uint16_t> link_costs_;          // Default: all 1
        mixnet_address mixaddr_ = INVALID_MIXADDR;  // Node's mixnet address

        // Helper function
        void add_link_cost(const uint16_t v) {
            link_costs_.push_back(v);
        }
    public:
        // Public constructor
        explicit node(const mixnet_address mixaddr) :
                      mixaddr_(mixaddr) {}
        // Accessors
        mixnet_address mixaddr() const { return mixaddr_; }
        uint16_t mixing_factor() const { return mixing_factor_; }
        bool do_random_routing() const { return do_random_routing_; }
        const std::vector<uint16_t>& link_costs() const { return link_costs_; }

        // Mutators
        void set_mixaddr(const uint16_t v) { mixaddr_ = v; }
        void set_mixing_factor(const uint16_t v) { mixing_factor_ = v; }
        void set_use_random_routing(const bool b) { do_random_routing_ = b; }

        // Expose internal state
        friend class graph;
    };

private:
    // List of graph nodes
    std::vector<node> nodes_;

    // Network topology represented using adjacency lists. For each
    // node, adjacency list indices correspond to the neighbor IDs.
    adjacency_graph topology_;

public:
    DISALLOW_COPY_AND_ASSIGN(graph);
    explicit graph(const uint16_t size);

    // Accessors
    node& get_node(const uint16_t id) { return nodes_.at(id); }
    const adjacency_graph& topology() const { return topology_; }
    const node& get_node(const uint16_t id) const { return nodes_.at(id); }

    // Mutators. TODO(natre): Ability to modify edge costs.
    graph& add_edge(const half_edge src, const half_edge dst);
    void set_mixaddrs(const std::vector<mixnet_address>& mixaddrs);

    /**
     * Instantiates special graph topologies using a subset of vertices
     * (corresponding to the given index set). If the idx set is empty,
     * incorporates all graph vertices.
     */
    enum class type { LINE = 0, RING, STAR, FULL_MESH };
    void generate_topology(const type t, std::vector<uint16_t> idxs={});
};

// Cleanup
#undef DISALLOW_COPY_AND_ASSIGN

} // namespace testing

#endif // TESTING_COMMON_GRAPH_H_
