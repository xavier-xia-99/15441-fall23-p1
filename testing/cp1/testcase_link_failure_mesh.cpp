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
#include "common/testing.h"

/**
 * This test-case checks whether nodes can recover when
 * some random links go down in a partial mesh network.
 */
class testcase_link_failure_mesh final : public testcase {
public:
    explicit testcase_link_failure_mesh() :
        testcase("testcase_link_failure_mesh") {}

    virtual void pcap(const uint16_t, const mixnet_packet
                                *const packet) override {
        if (packet->type == PACKET_TYPE_FLOOD) {
            pcap_count_++;
        }
    }

    virtual void setup() override {
        init_graph(7);
        graph_->set_mixaddrs({13, 14, 15, 4, 21, 23, 98});

        // Create a 4-node ring
        graph_->generate_topology(
            graph::type::RING, {0, 1, 2, 3});

        // Form a secondary star at 2
        graph_->generate_topology(
            graph::type::STAR, {2, 4, 5, 6});

        // Form another loop
        graph_->add_edge(4, 5);
        graph_->add_edge(5, 6);

        // Update STP convergence parameters
        root_hello_interval_ms_ = 100; // 100 ms
        reelection_interval_ms_ = 1000; // 1 second
    }

    virtual error_code run(orchestrator& o) override {
        sleep(5); // Wait for STP convergence

        // Subscribe to packets from all nodes
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.pcap_change_subscription(i, true));
        }
        // Try the root as source several times
        for (uint16_t i = 0; i < 4; i++) {
            DIE_ON_ERROR(o.send_packet(3, 0, PACKET_TYPE_FLOOD));
        }
        sleep(5); // Wait for packets to propagate

        // Disconnect a few links in the network
        DIE_ON_ERROR(o.change_link_state(2, 3, false));
        DIE_ON_ERROR(o.change_link_state(2, 4, false));
        DIE_ON_ERROR(o.change_link_state(2, 5, false));
        sleep(5); // Wait for STP reconvergence

        // Now try a leaf node as source
        for (uint16_t i = 0; i < 2; i++) {
            DIE_ON_ERROR(o.send_packet(4, 0, PACKET_TYPE_FLOOD));
        }
        sleep(5); // Wait for packets to propagate
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == ((4 * 6) + (2 * 6)));
    }
};

int main(int argc, char **argv) {
    testcase_link_failure_mesh tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
