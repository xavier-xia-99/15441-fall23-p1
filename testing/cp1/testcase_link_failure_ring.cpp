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
 * This test-case checks whether nodes can recover
 * when a random link goes down in a ring network.
 */
class testcase_link_failure_ring final : public testcase {
public:
    explicit testcase_link_failure_ring() :
        testcase("testcase_link_failure_ring") {}

    virtual void pcap(const uint16_t, const mixnet_packet
                                *const packet) override {
        if (packet->type == PACKET_TYPE_FLOOD) {
            pcap_count_++;
        }
    }

    virtual void setup() override {
        init_graph(5);
        graph_->set_mixaddrs({13, 14, 15, 4, 21});
        graph_->generate_topology(graph::type::RING);

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
        for (uint16_t i = 0; i < 7; i++) {
            DIE_ON_ERROR(o.send_packet(3, 0, PACKET_TYPE_FLOOD));
        }
        sleep(5); // Wait for packets to propagate

        // Disconnect a link, creating a line topology
        DIE_ON_ERROR(o.change_link_state(2, 3, false));
        sleep(5); // Wait for STP reconvergence

        // Now try the root again as source
        for (uint16_t i = 0; i < 7; i++) {
            DIE_ON_ERROR(o.send_packet(3, 0, PACKET_TYPE_FLOOD));
        }
        sleep(5); // Wait for packets to propagate
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == ((2 * 7 * 4)));
    }
};

int main(int argc, char **argv) {
    testcase_link_failure_ring tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
