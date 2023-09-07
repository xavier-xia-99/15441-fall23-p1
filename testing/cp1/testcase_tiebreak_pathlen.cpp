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
 * based on the path length to the root.
 *
 * Since the pcap harness doesn't guarantee in-order packet delivery,
 * we can't rely on that to check how the spanning-tree is traversed;
 * instead, we simply disable low-priority links post STP convergence
 * and see if FLOOD packets still propagate through the network OK.
 */
class testcase_tiebreak_pathlen final : public testcase {
public:
    explicit testcase_tiebreak_pathlen() :
        testcase("testcase_tiebreak_pathlen") {}

    virtual void pcap(const uint16_t, const mixnet_packet
                                *const packet) override {
        if (packet->type == PACKET_TYPE_FLOOD) {
            pcap_count_++;
        }
    }

    virtual void setup() override {
        init_graph(5);
        graph_->set_mixaddrs({18, 22, 12, 23, 15});
        graph_->generate_topology(graph::type::RING);

        // Set a high reelection interval (never kicks in)
        reelection_interval_ms_ = 1000000; // 1000 seconds
    }

    virtual error_code run(orchestrator& o) override {
        sleep(5); // Wait for STP convergence

        // Disable the low-priority link (should be blocked)
        DIE_ON_ERROR(o.change_link_state(0, 4, false));

        // Subscribe to packets from all nodes
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.pcap_change_subscription(i, true));
        }
        // Try both ends of the ring as source
        for (uint16_t i = 0; i < 7; i++) {
            DIE_ON_ERROR(o.send_packet(0, 0, PACKET_TYPE_FLOOD));
            DIE_ON_ERROR(o.send_packet(4, 0, PACKET_TYPE_FLOOD));
        }
        sleep(5); // Wait for packets to propagate
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == (7 * 2 * 4));
    }
};

int main(int argc, char **argv) {
    testcase_tiebreak_pathlen tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
