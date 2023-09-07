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
 * This test-case exercises a ring topology with 8 Mixnet nodes. We
 * subscribe to packet updates from every node, then send one FLOOD
 * packet using a subset of nodes as src.
 */
class testcase_ring_hard final : public testcase {
public:
    explicit testcase_ring_hard() :
        testcase("testcase_ring_hard") {}

    virtual void pcap(const uint16_t, const mixnet_packet
                                *const packet) override {
        if (packet->type == PACKET_TYPE_FLOOD) {
            pcap_count_++;
        }
    }

    virtual void setup() override {
        init_graph(8);
        graph_->generate_topology(graph::type::RING);
        graph_->set_mixaddrs({12, 37, 52, 71, 34, 16, 28, 46});
    }

    virtual error_code run(orchestrator& o) override {
        sleep(5); // Wait for STP convergence

        // Subscribe to packets from all nodes
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.pcap_change_subscription(i, true));
        }
        // Try every alternate node as source
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            if ((i % 2) == 1) { continue; }
            DIE_ON_ERROR(o.send_packet(i, 0, PACKET_TYPE_FLOOD));
        }
        sleep(5); // Wait for packets to propagate
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == (4 * 7));
    }
};

int main(int argc, char **argv) {
    testcase_ring_hard tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
