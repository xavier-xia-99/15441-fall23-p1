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
 * This test-case exercises a binary tree topology with 7 Mixnet nodes.
 * We subscribe to packet updates from all the nodes, then send a fixed
 * number of FLOOD packets using every node as src.
 */
class testcase_tree_hard final : public testcase {
public:
    explicit testcase_tree_hard() :
        testcase("testcase_tree_hard") {}

    virtual void pcap(const uint16_t, const mixnet_packet
                                *const packet) override {
        if (packet->type == PACKET_TYPE_FLOOD) {
            pcap_count_++;
        }
    }

    virtual void setup() override {
        init_graph(7);
        // Level 1
        graph_->add_edge(0, 1);
        graph_->add_edge(0, 2);
        // Level 2
        graph_->add_edge(1, 3);
        graph_->add_edge(1, 4);
        graph_->add_edge(2, 5);
        graph_->add_edge(2, 6);
        // Assign mixnet addresses
        graph_->set_mixaddrs({52, 31, 108, 77, 23, 41, 62});
    }

    virtual error_code run(orchestrator& o) override {
        sleep(5); // Wait for STP convergence

        // Subscribe to packets from all nodes
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.pcap_change_subscription(i, true));
        }
        // Try every node as source
        for (uint16_t t = 0; t < 5; t++) {
            for (uint16_t idx = 0; idx < 7; idx++) {
                DIE_ON_ERROR(o.send_packet(idx, 0, PACKET_TYPE_FLOOD));
            }
        }
        sleep(5); // Wait for packets to propagate
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == (5 * 7 * 6));
    }
};

int main(int argc, char **argv) {
    testcase_tree_hard tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
