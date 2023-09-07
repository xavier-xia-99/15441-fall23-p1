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
 * This test-case checks whether the node implementation does
 * anything weird when a node in the topology is unreachable.
 */
class testcase_unreachable final : public testcase {
public:
    explicit testcase_unreachable() :
        testcase("testcase_unreachable") {}

    virtual void pcap(const uint16_t, const mixnet_packet
                                *const packet) override {
        if (packet->type == PACKET_TYPE_FLOOD) {
            pcap_count_++;
        }
    }

    virtual void setup() override {
        init_graph(8);
        graph_->set_mixaddrs({1, 3, 5, 4, 7, 6, 8, 0});

        // Initialize a quasi-line topology
        graph_->generate_topology(graph::type::LINE,
                                  {0, 1, 2, 3, 4, 5, 6});
    }

    virtual error_code run(orchestrator& o) override {
        sleep(5); // Wait for STP convergence

        // Subscribe to the unreachable node
        DIE_ON_ERROR(o.pcap_change_subscription(7, true));

        // Try unreachable node as sink
        for (size_t i = 0; i < 7; i++) {
            DIE_ON_ERROR(o.send_packet(i, 0, PACKET_TYPE_FLOOD));
        }
        sleep(5); // Wait for packets to propagate

        // Subscribe to packets from the remaining nodes
        for (uint16_t i = 0; i < 7; i++) {
            DIE_ON_ERROR(o.pcap_change_subscription(i, true));
        }
        // Unreachable node as source
        for (uint16_t i = 0; i < 7; i++) {
            DIE_ON_ERROR(o.send_packet(7, 0, PACKET_TYPE_FLOOD));
        }
        sleep(5); // Wait for packets to propagate
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == 0);
    }
};

int main(int argc, char **argv) {
    testcase_unreachable tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
