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
 * Exercises ping functionality in a line topology.
 */
class testcase_ping final : public testcase {
private:
    std::vector<mixnet_address> expected_route_req_{13, 11, 9, 72, 84, 2};
    std::vector<mixnet_address> expected_route_rsp_{2, 84, 72, 9, 11, 13};

public:
    explicit testcase_ping() :
        testcase("testcase_ping") {}

    virtual void pcap(
        const uint16_t fragment_id,
        const mixnet_packet *const packet) override {

        if (packet->type == PACKET_TYPE_PING) {
            auto rh = reinterpret_cast<const
                mixnet_packet_routing_header*>(packet->payload());

            int src_node_id = graph_->get_node_id(rh->src_address);
            if (fragment_id == 7) {
                pass_pcap_ &= (src_node_id == 0);
                pass_pcap_ &= check_route(rh, expected_route_req_);
            }
            else if (fragment_id == 0) {
                pass_pcap_ &= (src_node_id == 7);
                pass_pcap_ &= check_route(rh, expected_route_rsp_);
            }
            else { pass_pcap_ = false; }

            pass_pcap_ &= (rh->dst_address ==
                           graph_->get_node(fragment_id).mixaddr());
            pcap_count_++;
        }
        // Unexpected packet type
        else { pass_pcap_ = false; }
    }

    virtual void setup() override {
        init_graph(8);
        graph_->generate_topology(graph::type::LINE);
        graph_->set_mixaddrs({15, 13, 11, 9, 72, 84, 2, 42});
    }

    virtual error_code run(orchestrator& o) override {
        await_convergence(); // Await STP convergence

        // Subscribe to packets from all nodes
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.pcap_change_subscription(i, true));
        }
        // Send a ping packet
        DIE_ON_ERROR(o.send_packet(0, 7, PACKET_TYPE_PING));

        await_packet_propagation();
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == 2);
    }
};

int main(int argc, char **argv) {
    testcase_ping tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
