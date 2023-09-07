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
 * This test-case exercises a fully-connected topology with 4 Mixnet
 * nodes. We subscribe to packet updates from every node, then send
 * a few FLOOD packets using the root of the spanning tree as src.
 */
class testcase_full_mesh_easy final : public testcase {
public:
    explicit testcase_full_mesh_easy() :
        testcase("testcase_full_mesh_easy") {}

    virtual void pcap(const uint16_t, const mixnet_packet
                                *const packet) override {
        if (packet->type == PACKET_TYPE_FLOOD) {
            pcap_count_++;
        }
    }

    virtual void setup() override {
        init_graph(4);
        graph_->set_mixaddrs({15, 13, 11, 9});
        graph_->generate_topology(graph::type::FULL_MESH);
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
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == (4 * 3));
    }
};

int main(int argc, char **argv) {
    testcase_full_mesh_easy tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
