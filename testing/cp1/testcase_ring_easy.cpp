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
 * This test-case exercises a simple ring topology with 3 Mixnet nodes.
 * We subscribe to packet updates from all the nodes, then send a FLOOD
 * packet using every node as src. We'd expect to see a total of 6 such
 * packets on the output (two for each source).
 */
class testcase_ring_easy final : public testcase {
public:
    explicit testcase_ring_easy() :
        testcase("testcase_ring_easy") {}

    virtual void pcap(const uint16_t, const mixnet_packet
                                *const packet) override {
        if (packet->type == PACKET_TYPE_FLOOD) {
            pcap_count_++;
        }
    }

    virtual void setup() override {
        init_graph(3);
        // Use default mixnet addresses
        graph_->generate_topology(graph::type::RING);
    }

    virtual error_code run(orchestrator& o) override {
        sleep(5); // Wait for STP convergence

        // Subscribe to packets from all nodes
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.pcap_change_subscription(i, true));
        }
        // Try every node as source
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.send_packet(i, 0, PACKET_TYPE_FLOOD));
        }
        sleep(5); // Wait for packets to propagate
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == (3 * 2));
    }
};

int main(int argc, char **argv) {
    testcase_ring_easy tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
