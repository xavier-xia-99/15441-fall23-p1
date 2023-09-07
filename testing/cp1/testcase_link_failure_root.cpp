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
 * the previous root gets disconnected from the network.
 */
class testcase_link_failure_root final : public testcase {
public:
    explicit testcase_link_failure_root() :
        testcase("testcase_link_failure_root") {}

    virtual void pcap(const uint16_t, const mixnet_packet
                                *const packet) override {
        if (packet->type == PACKET_TYPE_FLOOD) {
            pcap_count_++;
        }
    }

    virtual void setup() override {
        init_graph(8);
        graph_->generate_topology(graph::type::STAR);
        graph_->generate_topology(graph::type::RING,
                                  {1, 2, 3, 4, 5, 6, 7});

        graph_->set_mixaddrs({2, 13, 14, 15, 4, 21, 22, 23});

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
        // Try the root as source
        DIE_ON_ERROR(o.send_packet(0, 0, PACKET_TYPE_FLOOD));
        sleep(5); // Wait for packets to propagate

        // Disconnect the root from the topology
        for (uint16_t i = 1; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.change_link_state(0, i, false));
        }
        sleep(5); // Wait for STP reconvergence

        // Now try two nodes of the ring as source
        DIE_ON_ERROR(o.send_packet(1, 0, PACKET_TYPE_FLOOD));
        DIE_ON_ERROR(o.send_packet(4, 0, PACKET_TYPE_FLOOD));
        sleep(5); // Wait for packets to propagate

        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == (7 + (2 * 6)));
    }
};

int main(int argc, char **argv) {
    testcase_link_failure_root tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
