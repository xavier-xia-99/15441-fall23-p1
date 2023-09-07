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
 * This test-case checks whether nodes perform tie-breaking correctly
 * based on the parent's mixnet address in a mesh network.
 *
 * Since the pcap harness doesn't guarantee in-order packet delivery,
 * we can't rely on that to check how the spanning-tree is traversed;
 * instead, we simply disable low-priority links post STP convergence
 * and see if FLOOD packets still propagate through the network OK.
 */
class testcase_tiebreak_parent_mesh final : public testcase {
public:
    explicit testcase_tiebreak_parent_mesh() :
        testcase("testcase_tiebreak_parent_mesh") {}

    virtual void pcap(const uint16_t, const mixnet_packet
                                *const packet) override {
        if (packet->type == PACKET_TYPE_FLOOD) {
            pcap_count_++;
        }
    }

    virtual void setup() override {
        init_graph(8);
        // Generate a reverse-dumbbell toplogy
        for (uint16_t i = 1; i < (graph_->num_nodes - 1); i++) {
            graph_->add_edge(0, i);
            graph_->add_edge(i, (graph_->num_nodes - 1));
        }
        graph_->set_mixaddrs({18, 27, 23, 35, 62, 20, 51, 19});

        // Set a high reelection interval (never kicks in)
        reelection_interval_ms_ = 1000000; // 1000 seconds
    }

    virtual error_code run(orchestrator& o) override {
        sleep(5); // Wait for STP convergence

        // Disable the low-priority links (should be blocked)
        for (uint16_t i = 1; i < (graph_->num_nodes - 1); i++) {
            if (i == 5) { continue; }
            DIE_ON_ERROR(o.change_link_state(
                i, (graph_->num_nodes - 1), false));
        }
        // Subscribe to packets from all nodes
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.pcap_change_subscription(i, true));
        }
        // Try a subset of the nodes as source
        for (uint16_t i = 0; i < 3; i++) {
            DIE_ON_ERROR(o.send_packet(0, 0, PACKET_TYPE_FLOOD));
            DIE_ON_ERROR(o.send_packet(3, 0, PACKET_TYPE_FLOOD));
            DIE_ON_ERROR(o.send_packet(6, 0, PACKET_TYPE_FLOOD));
            DIE_ON_ERROR(o.send_packet(7, 0, PACKET_TYPE_FLOOD));
        }
        sleep(5); // Wait for packets to propagate
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == (3 * 4 * 7));
    }
};

int main(int argc, char **argv) {
    testcase_tiebreak_parent_mesh tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
